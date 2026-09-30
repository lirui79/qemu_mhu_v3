/*
 * npu_codec_bridge — NPU Decoder C++ Wrapper (Pybind11 bridge)
 *
 * Wraps the VastAI VMPP low-level decoder SDK into a RAII C++ class
 * designed to be exposed to Python via pybind11.
 *
 * Key design goals:
 *  1. Isolate all C-level SDK details (device fd, runtime dlopen, channel lifecycle)
 *  2. Support both "simple" (host-memory) and "zero-copy" (device-memory-mapped) output paths
 *  3. Expose decoded frames as torch::Tensor via torch::from_blob (zero overhead)
 *  4. Thread-safe: parallel SendStream / ReceiveFrame per SDK's PARALLEL API mode
 */

#ifndef NPU_CODEC_BRIDGE_NPU_DECODER_WRAPPER_H_
#define NPU_CODEC_BRIDGE_NPU_DECODER_WRAPPER_H_

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

// Torch — this bridge always requires libtorch
#include <torch/torch.h>

// ---------------------------------------------------------------------------
// SDK opaque types (fully defined in vmpp_common.h / vmpp_dec_defs.h)
// ---------------------------------------------------------------------------
typedef int     vmppDevice;
typedef void *  vmppChannel;
typedef size_t  vmppAddr;
typedef uint64_t vmppDevAddr;

// Thin struct mirrors for compile-time layout compatibility.
// Full definitions live in the SDK headers; these are sufficient for the
// wrapper to store and forward them.

enum class PixelFormat : int {
    NV12   = 13,   // vmpp_PIX_FMT_NV12
    NV21   = 14,   // vmpp_PIX_FMT_NV21
    P010   = 25,   // vmpp_PIX_FMT_YUV420_PLANAR_10BIT_P010
    I010   = 24,   // vmpp_PIX_FMT_YUV420_PLANAR_10BIT_I010
};

enum class CodecType : int {
    JPEG = 0,
    H264 = 1,
    HEVC = 2,
    AV1  = 3,
    VP9  = 4,
    AVS2 = 5,
};

enum class DecMemoryMode : int {
    NORMAL            = 0,  // SDK-allocated, output to data[] or busAddress[]
    USER_OUT_BUF_HOST = 1,  // User provides host buffer via frame.data[0]
    USER_OUT_BUF_DEV  = 2,  // User provides device buffer via frame.busAddress[0]
    LESS_DEV_MEM      = 3,  // Minimise device memory, HOST-only output
    USER_AS_HWOUT     = 4,  // User buffer via stream.outputBusAddress[], HW writes directly
    USER_OUT_HANDLE   = 5,  // User provides DMA-BUF fd handle
};

enum class ZeroCopyMode : int {
    COPY_TO_HOST = 0,  // Let SDK DMA-copy to host; wrap data[] with from_blob
    MMAP_DEVICE  = 1,  // mmap NPU physical addresses; wrap with from_blob (true zero-copy)
    DMA_BUF_FD   = 2,  // Export DMA-BUF fd; mmap the fd; wrap with from_blob (sg100 only)
};

// ---------------------------------------------------------------------------
// Configuration structures
// ---------------------------------------------------------------------------

struct DecoderConfig {
    std::string  devicePath{"/dev/vastai_video0"};  // DRM render node
    std::string  codecType{"h264"};                 // "h264", "hevc", "av1", "vp9", "avs2"
    std::string  pixelFormat{"NV12"};
    DecMemoryMode memoryMode{DecMemoryMode::NORMAL};
    ZeroCopyMode zeroCopyMode{ZeroCopyMode::COPY_TO_HOST};

    uint32_t     maxWidth{8192};
    uint32_t     maxHeight{8192};
    uint32_t     streamBufferSize{16 * 1024 * 1024};
    uint32_t     extraBufferNumber{20};   // 转码场景需要 >= 20
    uint32_t     outputAlign{0};          // 0 = SDK 默认对齐
    uint32_t     enableSEIParser{1};
    bool         noOutputReordering{false};
    bool         bufSlimMode{false};
    int          logLevel{2};             // 2 = INFO
    int          coreMode{0};             // 0 = AUTO

    // When true, the input thread runs in a background std::thread,
    // mimicking the SDK's PARALLEL API mode.
    bool         asyncInput{true};
};

struct DecodedFrame {
    // --- Primary data: torch Tensors sharing NPU/Host memory ---
    //
    // For NV12 / P010:
    //   y_tensor:   shape [height, stride[0]], dtype uint8 (or uint16 for P010)
    //   uv_tensor:  shape [height/2, stride[1]], dtype uint8 (or uint16 for P010)
    //
    // The tensors are created via torch::from_blob and share the underlying
    // buffer. Users MUST NOT use these tensors after calling release_frame().
    //
    torch::Tensor y_tensor;
    torch::Tensor uv_tensor;

    // Convenience: packed buffer (Y plane + UV plane), shape [dataSize]
    // Only valid when copyToHost==true and the buffer was consolidated.
    torch::Tensor packed_tensor;

    // --- Metadata ---
    int64_t      pts{-1};
    uint32_t     width{0};
    uint32_t     height{0};
    uint32_t     stride_y{0};
    uint32_t     stride_uv{0};
    uint32_t     crop_width{0};
    uint32_t     crop_height{0};
    uint32_t     crop_x{0};
    uint32_t     crop_y{0};
    PixelFormat  pixel_format{PixelFormat::NV12};
    int          frame_type{0};  // 0=I, 1=P, 2=B
    uint32_t     data_size{0};
};

// Callback for ancillary data (SEI messages)
using SEICallback = std::function<void(int payload_type,
                                        const uint8_t* data, uint32_t size)>;

// ---------------------------------------------------------------------------
// NPUDecoderWrapper
// ---------------------------------------------------------------------------

class NPUDecoderWrapper {
public:
    // ---- Lifecycle ----

    /**
     * Construct from a DecoderConfig.
     *
     * Performs:
     *   1. open(devicePath, O_RDWR)
     *   2. dlopen("libvaccrt.so") + dlsym all runtime symbols
     *   3. vaccrt_init(dieId)
     *   4. vmppInitDecoder(&cfg)
     *
     * Throws std::runtime_error on failure.
     */
    explicit NPUDecoderWrapper(const DecoderConfig& config);

    /**
     * Tears down in reverse order:
     *   1. stop() if still running
     *   2. vmppDecDestroyChannel
     *   3. vmppDeInitDecoder
     *   4. close_runtime (dlclose)
     *   5. close(deviceFd)
     *
     * No-throw guarantee.
     */
    ~NPUDecoderWrapper();

    // Non-copyable, non-movable (owns hardware resources)
    NPUDecoderWrapper(const NPUDecoderWrapper&) = delete;
    NPUDecoderWrapper& operator=(const NPUDecoderWrapper&) = delete;
    NPUDecoderWrapper(NPUDecoderWrapper&&) = delete;
    NPUDecoderWrapper& operator=(NPUDecoderWrapper&&) = delete;

    // ---- Decoding ----

    /**
     * Push a compressed bitstream access unit into the decoder.
     *
     * The input data MUST be a complete Access Unit (frame-aligned NAL units),
     * including Annex B start codes (00 00 00 01 / 00 00 01).
     *
     * This call is non-blocking; frames are produced asynchronously and
     * retrieved via get_frame().
     *
     * Returns false if the decoder input queue is full (caller should retry).
     * Returns true on success.
     * Throws on fatal error.
     */
    bool decode(const uint8_t* data, size_t size, int64_t pts = -1);

    /**
     * Pull one decoded frame from the output queue.
     *
     * Returns std::nullopt if no frame is ready (caller should poll).
     * Returns DecodedFrame with torch Tensors sharing the underlying buffer.
     *
     * IMPORTANT: The returned tensors are valid ONLY until the next call
     * to release_frame(). Users should clone the tensor (.clone()) if they
     * need to retain data beyond that point.
     *
     * Blocks up to timeoutMs (default 500ms) waiting for a frame.
     */
    std::optional<DecodedFrame> get_frame(uint32_t timeoutMs = 500);

    /**
     * Release a previously obtained frame back to the SDK's buffer pool.
     *
     * MUST be called after the caller is done with the DecodedFrame's tensors.
     * Failing to call this will eventually exhaust the decoder's DPB.
     */
    void release_frame(const DecodedFrame& frame);

    /**
     * Convenience: push + pull in one call (synchronous, single-threaded).
     * Equivalent to decode() followed by get_frame() in a loop.
     *
     * Returns all available frames after pushing this AU.
     */
    std::vector<DecodedFrame> decode_and_receive(const uint8_t* data,
                                                  size_t size,
                                                  int64_t pts = -1);

    // ---- Flush / Drain ----

    /**
     * Drain all pending frames from the decoder.
     * Call this after the last decode() to retrieve buffered frames.
     *
     * In the PARALLEL API mode, the SDK signals vmpp_RSLT_WARN_EOS
     * after the last frame has been pulled.
     */
    std::vector<DecodedFrame> flush(uint32_t timeoutMs = 500);

    // ---- Seek ----

    /**
     * Seek to a target timestamp (milliseconds).
     *
     * Implementation strategy (since SDK has no native seek):
     *  1. If seeking backwards: reset the decoder entirely.
     *  2. Seek the input stream to the last I-frame before targetTs.
     *  3. Decode without output (discard frames) until targetTs is reached.
     *
     * The caller is responsible for managing the input stream position.
     * This method handles the decoder side: draining current state and
     * optionally recreating the channel.
     *
     * Returns the actual PTS of the first frame after seek.
     * Returns -1 if seek failed.
     */
    int64_t seek(double targetTimestampMs);

    /**
     * Hard reset: stop, destroy channel, re-create channel, start.
     * Use this before feeding a new stream from a different position.
     */
    void reset();

    // ---- Stream Info ----

    /**
     * Probe a bitstream prefix to extract resolution / framerate / DPB size.
     * The data must contain at least one complete AU with SPS/PPS.
     */
    struct StreamInfo {
        uint32_t width{0};
        uint32_t height{0};
        uint32_t cropWidth{0};
        uint32_t cropHeight{0};
        float    fps{0.0f};
        uint32_t requiredBufNum{0};   // DPB size needed
        uint32_t reorderNum{0};       // Reorder frame count
        PixelFormat pixelFormat{PixelFormat::NV12};
    };
    StreamInfo probe_stream(const uint8_t* data, size_t size);

    // ---- Status ----

    /** Number of idle DPB buffers (if 0, decoder is back-pressured). */
    int32_t idle_dpb_count();

    /** Current decoder state string. */
    std::string state_string();

    // ---- Configuration accessors ----
    const DecoderConfig& config() const { return config_; }

    // ---- SEI callback registration ----
    void set_sei_callback(SEICallback cb) { sei_callback_ = std::move(cb); }

private:
    // ---- Internal helpers ----

    void open_device();
    void load_runtime();
    void init_runtime();
    void init_decoder();
    void create_channel();
    void start_channel();

    void input_thread_loop();     // runs in background when asyncInput==true
    DecodedFrame wrap_frame(void* rawFrame, bool ownsDeviceMemory);

    // ---- Zero-copy memory mapping ----

    /**
     * Attempt to mmap NPU device physical memory into user-space.
     *
     * Strategy:
     *  - The SDK driver exposes the NPU DDR region via the DRM device node.
     *  - We query the physical address from vmppFrame.busAddress[0] (for Y)
     *    and busAddress[1] (for UV).
     *  - mmap(deviceFd, offset=phys_addr, PROT_READ, MAP_SHARED) maps the
     *    NPU DDR pages directly into the process address space.
     *  - torch::from_blob(mappedVirtualAddr, sizes, deleter) creates a
     *    tensor that:
     *      (a) shares the NPU output buffer (zero copy from NPU → Tensor)
     *      (b) calls munmap when the tensor's data pointer is freed
     *
     * Returns {virtualAddr, size} pairs for Y and UV planes.
     * Returns empty vector if mmap is not available on this platform/driver.
     */
    struct MappedPlane {
        void*    virtualAddr;
        uint64_t physicalAddr;
        size_t   size;
    };
    std::vector<MappedPlane> map_device_memory(const vmppDevAddr* busAddresses,
                                                const uint32_t* strides,
                                                uint32_t width,
                                                uint32_t height,
                                                PixelFormat format);

    /**
     * Try to export a DMA-BUF fd from the NPU buffer.
     * sg100 platform only; sv100 does not support this path.
     */
    std::optional<int> export_dma_buf_fd(vmppDevAddr busAddress);

    /**
     * mmap a DMA-BUF fd into user space.
     */
    void* mmap_dma_buf(int fd, size_t size);

    // ---- Member variables ----

    DecoderConfig config_;

    // Device & Runtime
    int              deviceFd_{-1};
    void*            runtimeHandle_{nullptr};
    // Runtime function pointers
    void*            vaccrt_init_{nullptr};
    void*            vaccrt_deinit_{nullptr};
    void*            vaccrt_malloc64_{nullptr};
    void*            vaccrt_free64_{nullptr};
    void*            vaccrt_get_video_reserver_ddr_{nullptr};

    // Decoder state
    vmppChannel      channel_{nullptr};
    std::atomic<bool> running_{false};
    std::atomic<bool> eos_signaled_{false};

    // Async input thread
    std::unique_ptr<std::thread> input_thread_;
    std::mutex                    input_mutex_;
    std::condition_variable       input_cv_;

    // SEI callback
    SEICallback sei_callback_;

    // Track mapped memory for cleanup
    struct MappedRegion {
        void*    addr;
        size_t   size;
    };
    std::vector<MappedRegion> mapped_regions_;
    std::mutex                 mapped_mutex_;

    // Current active frame (for release_frame validation)
    struct ActiveFrame {
        void*    sdkFramePtr;       // pointer to SDK's internal frame struct
        bool     ownsDeviceMemory;  // true if we mmap'd, false if SDK-allocated
    };
    std::optional<ActiveFrame> active_frame_;

    // Opaque pointer to the last SDK frame (vmppFrame) for release_frame().
    // Allocated/freed in the .cpp where vmpp_dec_defs.h is available.
    void* last_sdk_frame_{nullptr};
};

// =========================================================================
//  Zero-Copy Design Notes (零拷贝设计说明)
// =========================================================================
//
// ┌─────────────────────────────────────────────────────────────────────┐
// │  PATH A: COPY_TO_HOST (default, always works)                       │
// │                                                                     │
// │  vmppDecReceiveFrame(memoryType=vmpp_MEM_HOST)                      │
// │       │                                                             │
// │       ▼                                                             │
// │  SDK internally DMA-copies NPU DDR → host bounce buffer             │
// │  (allocated via vaccrt_malloc64 or malloc)                          │
// │       │                                                             │
// │       ▼                                                             │
// │  frame.data[0] → valid CPU virtual address                          │
// │  frame.data[1] → valid CPU virtual address (UV plane)               │
// │       │                                                             │
// │       ▼                                                             │
// │  torch::from_blob(frame.data[0], {height, strideY},                 │
// │     /*deleter=*/[](void*){})                                        │
// │       │                                                             │
// │  Tensor shares the bounce buffer (no extra copy).                   │
// │  Buffer is freed when SDK recycles it (via vmppDecReleaseFrame).    │
// │                                                                     │
// │  Copies: 1 (NPU DDR → Host bounce buffer, done by SDK DMA)          │
// └─────────────────────────────────────────────────────────────────────┘
//
// ┌─────────────────────────────────────────────────────────────────────┐
// │  PATH B: MMAP_DEVICE (true zero-copy, needs driver mmap support)    │
// │                                                                     │
// │  vmppDecReceiveFrame(memoryType=vmpp_MEM_DEVICE)                    │
// │       │                                                             │
// │       ▼                                                             │
// │  SDK does NOT copy; data stays on NPU DDR                           │
// │  frame.busAddress[0] → physical address of Y plane                  │
// │  frame.busAddress[1] → physical address of UV plane                 │
// │       │                                                             │
// │       ▼                                                             │
// │  map_device_memory():                                               │
// │    offset_in_dev_mem = busAddress - soc_base_addr                   │
// │    void* virt = mmap(deviceFd, offset_in_dev_mem, size,             │
// │                       PROT_READ, MAP_SHARED)                        │
// │       │                                                             │
// │       ▼                                                             │
// │  torch::from_blob(virt, {height, strideY},                          │
// │     /*deleter=*/[fd=deviceFd, addr=virt, len=size](void*){          │
// │         munmap(addr, len);                                          │
// │     })                                                              │
// │       │                                                             │
// │  Tensor directly reads NPU DDR through the IOMMU mapping.           │
// │  munmap is called when tensor's data pointer refcount reaches zero. │
// │                                                                     │
// │  Copies: 0                                                          │
// │  Caveat: Requires kernel driver to implement .mmap on the           │
// │          /dev/vastai_video* device node.                            │
// └─────────────────────────────────────────────────────────────────────┘
//
// ┌─────────────────────────────────────────────────────────────────────┐
// │  PATH C: DMA_BUF_FD (sg100, DMA-BUF-based zero-copy)                │
// │                                                                     │
// │  vmppDecReceiveFrame(memoryType=vmpp_MEM_DEVICE)                    │
// │       │                                                             │
// │       ▼                                                             │
// │  frame.sharedFD → DMA-BUF file descriptor                           │
// │  (OR: use DRM_IOCTL_PRIME_HANDLE_TO_FD on the GEM handle)           │
// │       │                                                             │
// │       ▼                                                             │
// │  mmap_dma_buf(fd, size):                                            │
// │    void* virt = mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0)   │
// │       │                                                             │
// │       ▼                                                             │
// │  torch::from_blob(virt, {height, strideY},                          │
// │     /*deleter=*/[fd, addr=virt, len=size](void*){                   │
// │         munmap(addr, len);                                          │
// │         close(fd);                                                  │
// │     })                                                              │
// │                                                                     │
// │  Copies: 0                                                          │
// │  Caveat: vmpp_MEM_SHARED mode only on sg100.                        │
// └─────────────────────────────────────────────────────────────────────┘
//
// Memory lifetime contract:
//   User calls get_frame()       → Tensor is valid
//   User calls release_frame()   → Tensor's backing memory may be recycled
//   User calls .clone() on tensor → Safe to hold beyond release_frame()

#endif  // NPU_CODEC_BRIDGE_NPU_DECODER_WRAPPER_H_
