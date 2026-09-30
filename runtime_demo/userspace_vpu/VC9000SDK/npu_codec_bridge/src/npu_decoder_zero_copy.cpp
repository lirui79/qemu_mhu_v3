/*
 * npu_codec_bridge — Zero-Copy Memory Mapping Implementation
 *
 * This file implements the three memory paths from NPU device memory
 * to torch::Tensor. It is the most critical part of the bridge.
 *
 * Architecture:
 *
 *   NPU DDR (physical address space)
 *      │
 *      ├──[PATH A]──► SDK DMA copies to Host bounce buffer
 *      │              torch::from_blob wraps the host pointer
 *      │
 *      ├──[PATH B]──► mmap(/dev/vastai_video*, offset=phys_addr)
 *      │              torch::from_blob wraps the mmap'd pointer
 *      │
 *      └──[PATH C]──► DRM_IOCTL_PRIME_HANDLE_TO_FD exports DMA-BUF fd
 *                     mmap(fd) maps the DMA-BUF
 *                     torch::from_blob wraps the mmap'd pointer
 */

#include "npu_decoder_wrapper.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cstring>
#include <stdexcept>
#include <unordered_map>

// Linux DRM / DMA-BUF headers
#include <drm.h>
#include <drm_mode.h>

// ---------------------------------------------------------------------------
// Platform Detection
// ---------------------------------------------------------------------------

static bool is_sg100() {
    // sg100 supports DMA-BUF fd export; sv100 may not.
    // Detect by probing the driver or checking a specific capability.
    // For now: assume the caller sets the zero-copy mode appropriately.
    return true;  // placeholder
}

// ---------------------------------------------------------------------------
// PATH B: mmap Device Physical Memory
// ---------------------------------------------------------------------------

/**
 * The VastAI driver exposes NPU DDR through /dev/vastai_video*.
 * When the driver supports .mmap, the physical bus address can be
 * translated to a file offset and mmap'd.
 *
 * Offset calculation:
 *   The driver typically maps its PCIe BAR space linearly:
 *     mmap_offset = busAddress - npu_ddr_base
 *   npu_ddr_base is obtained from the driver (e.g., via an ioctl or sysfs).
 */
static uint64_t g_npu_ddr_base = 0;  // populated at init time

static uint64_t query_npu_ddr_base(int deviceFd) {
    // Attempt to get the NPU DDR base address via:
    // 1. DRM ioctl (driver-specific)
    // 2. sysfs: /sys/class/vastai_video/.../ddr_base
    // 3. vaccrt_get_video_reserver_ddr() → addr_ext_t.soc_addr
    //
    // For the wrapper, we use method 3 (runtime library).

    // addr_ext_t addrExt;
    // vaccrt_get_video_reserver_ddr(devId, &addrExt);
    // return addrExt.soc_addr;

    (void)deviceFd;
    return g_npu_ddr_base;  // populated at runtime init
}

std::vector<NPUDecoderWrapper::MappedPlane>
NPUDecoderWrapper::map_device_memory(
    const vmppDevAddr* busAddresses,
    const uint32_t*     strides,
    uint32_t            width,
    uint32_t            height,
    PixelFormat         format)
{
    std::vector<MappedPlane> planes;
    const uint64_t ddrBase = g_npu_ddr_base;

    if (ddrBase == 0) {
        throw std::runtime_error(
            "NPU DDR base address unknown — cannot mmap device memory. "
            "Use COPY_TO_HOST mode or ensure the driver exposes DDR base.");
    }

    // Calculate plane sizes based on pixel format
    uint32_t y_size  = strides[0] * height;
    uint32_t uv_size = 0;

    switch (format) {
    case PixelFormat::NV12:
    case PixelFormat::NV21:
        // Y: stride[0] * height, UV: stride[1] * height/2
        uv_size = strides[1] * (height / 2);
        break;
    case PixelFormat::P010:
    case PixelFormat::I010:
        y_size  = strides[0] * height * 2;      // 10-bit: 2 bytes/pixel
        uv_size = strides[1] * height;           // interleaved UV, 16-bit
        break;
    default:
        throw std::runtime_error("Unsupported pixel format for zero-copy");
    }

    // --- Map Y plane ---
    {
        uint64_t physAddr = static_cast<uint64_t>(busAddresses[0]);
        uint64_t offset   = physAddr - ddrBase;
        size_t   length   = y_size;

        // Align offset to page boundary
        uint64_t pageOffset = offset % sysconf(_SC_PAGESIZE);
        offset -= pageOffset;
        length += pageOffset;

        void* virt = mmap(nullptr, length, PROT_READ | PROT_WRITE,
                          MAP_SHARED, deviceFd_, static_cast<off_t>(offset));

        if (virt == MAP_FAILED) {
            throw std::runtime_error(
                "mmap failed for Y plane at phys=0x" +
                std::to_string(physAddr) + ": " + strerror(errno));
        }

        // Adjust virtual pointer by the page misalignment
        void* dataPtr = static_cast<uint8_t*>(virt) + pageOffset;

        planes.push_back({dataPtr, physAddr, y_size});

        // Register for cleanup
        std::lock_guard<std::mutex> lock(mapped_mutex_);
        mapped_regions_.push_back({virt, length});
    }

    // --- Map UV plane ---
    if (busAddresses[1] != 0 && uv_size > 0) {
        uint64_t physAddr = static_cast<uint64_t>(busAddresses[1]);
        uint64_t offset   = physAddr - ddrBase;
        size_t   length   = uv_size;

        uint64_t pageOffset = offset % sysconf(_SC_PAGESIZE);
        offset -= pageOffset;
        length += pageOffset;

        void* virt = mmap(nullptr, length, PROT_READ | PROT_WRITE,
                          MAP_SHARED, deviceFd_, static_cast<off_t>(offset));

        if (virt == MAP_FAILED) {
            // Clean up Y plane mapping
            if (!planes.empty()) {
                munmap(planes[0].virtualAddr, planes[0].size);
            }
            throw std::runtime_error(
                "mmap failed for UV plane at phys=0x" +
                std::to_string(physAddr) + ": " + strerror(errno));
        }

        void* dataPtr = static_cast<uint8_t*>(virt) + pageOffset;
        planes.push_back({dataPtr, physAddr, uv_size});

        std::lock_guard<std::mutex> lock(mapped_mutex_);
        mapped_regions_.push_back({virt, length});
    }

    return planes;
}


// ---------------------------------------------------------------------------
// PATH C: DMA-BUF fd export + mmap (sg100 only)
// ---------------------------------------------------------------------------

/**
 * On sg100, the driver can export a GEM buffer handle as a DMA-BUF fd.
 * We use DRM_IOCTL_PRIME_HANDLE_TO_FD to get the fd, then mmap it.
 *
 * The GEM handle is obtained from the driver (via a driver-specific ioctl
 * or via the vmppFrame.sharedFD field when using vmpp_MEM_SHARED mode).
 */

#ifndef DRM_IOCTL_PRIME_HANDLE_TO_FD
struct drm_prime_handle {
    uint32_t handle;
    uint32_t flags;
    int32_t  fd;
};
#define DRM_IOCTL_PRIME_HANDLE_TO_FD \
    _IOWR('d', 0x2d, struct drm_prime_handle)
#endif

std::optional<int> NPUDecoderWrapper::export_dma_buf_fd(vmppDevAddr busAddress) {
    (void)busAddress;
    // On sg100, obtain the GEM handle from the driver for the given
    // bus address, then call:
    //
    //   struct drm_prime_handle prime;
    //   prime.handle = gemHandle;
    //   prime.flags  = DRM_CLOEXEC | DRM_RDWR;
    //   ioctl(deviceFd_, DRM_IOCTL_PRIME_HANDLE_TO_FD, &prime);
    //   return prime.fd;
    //
    // This requires a driver-specific ioctl to translate busAddress → GEM handle.
    return std::nullopt;
}

void* NPUDecoderWrapper::mmap_dma_buf(int fd, size_t size) {
    void* virt = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, 0);
    if (virt == MAP_FAILED) {
        throw std::runtime_error(
            "mmap DMA-BUF fd " + std::to_string(fd) + " failed: " + strerror(errno));
    }
    return virt;
}


// ---------------------------------------------------------------------------
// Unified Frame Wrapping: NPU memory → torch::Tensor
// ---------------------------------------------------------------------------

/**
 * wrap_frame() is the central function that converts a decoded SDK frame
 * into a DecodedFrame with torch Tensors.
 *
 * The path taken depends on config_.zeroCopyMode:
 *
 *   COPY_TO_HOST:
 *     - SDK has already DMA'd data to host (frame.data[0], frame.data[1])
 *     - torch::from_blob wraps these host pointers
 *     - Deleter is a no-op (SDK owns the memory; user must call release_frame)
 *
 *   MMAP_DEVICE:
 *     - SDK output is at frame.busAddress[0], frame.busAddress[1]
 *     - map_device_memory() mmaps these physical addresses
 *     - torch::from_blob wraps the mmap'd pointers
 *     - Deleter calls munmap when tensor is freed
 *
 *   DMA_BUF_FD:
 *     - export dma-buf fds, mmap them, wrap with from_blob
 *     - Deleter calls munmap + close(fd)
 */
DecodedFrame NPUDecoderWrapper::wrap_frame(void* sdkFrame, bool ownsDeviceMemory) {
    // --- Step 1: Extract SDK frame fields ---
    // (in the real impl, this reads from the SDK's vmppFrame struct)

    // Fields we extract from the SDK frame:
    uint8_t*     hostData[3]   = {nullptr, nullptr, nullptr};  // data[0], data[1], data[2]
    vmppDevAddr  devAddr[3]    = {0, 0, 0};                    // busAddress[0], busAddress[1]
    uint32_t     stride[3]     = {0, 0, 0};
    uint32_t     width         = 0;
    uint32_t     height        = 0;
    uint32_t     dataSize      = 0;
    PixelFormat  pixFmt        = PixelFormat::NV12;
    int          frameType     = 0;
    int64_t      pts           = 0;
    uint32_t     cropW         = 0, cropH = 0, cropX = 0, cropY = 0;

    // (Real implementation populates these from the vmppFrame*)
    (void)sdkFrame;
    (void)ownsDeviceMemory;

    DecodedFrame result;

    switch (config_.zeroCopyMode) {

    // ---- PATH A: SDK copies to host, we wrap without extra copy ----
    case ZeroCopyMode::COPY_TO_HOST: {
        if (!hostData[0]) {
            throw std::runtime_error("Host data pointer is null in COPY_TO_HOST mode");
        }

        // Y plane tensor: shares hostData[0]
        // Stride-aware: tensor shape is [height, strideY], but logical width is width
        result.y_tensor = torch::from_blob(
            hostData[0],
            {static_cast<int64_t>(height), static_cast<int64_t>(stride[0])},
            torch::kUInt8  // or kUInt16 for P010
        );

        // UV plane tensor: shares hostData[1]
        if (hostData[1]) {
            result.uv_tensor = torch::from_blob(
                hostData[1],
                {static_cast<int64_t>(height / 2), static_cast<int64_t>(stride[1])},
                torch::kUInt8
            );
        }

        // NOTE: no custom deleter — SDK manages the hostData buffers.
        // The user MUST call release_frame() before SDK recycles them.
        break;
    }

    // ---- PATH B: True zero-copy via mmap of device physical memory ----
    case ZeroCopyMode::MMAP_DEVICE: {
        if (devAddr[0] == 0) {
            throw std::runtime_error(
                "Device bus address is null in MMAP_DEVICE mode. "
                "Make sure to set memoryType=vmpp_MEM_DEVICE in DecoderConfig.");
        }

        auto planes = map_device_memory(devAddr, stride, width, height, pixFmt);

        if (planes.size() >= 1) {
            // Each MappedPlane has its own virtual address from a separate mmap call.
            // We create a tensor with a deleter that munmaps the region.
            void* yVirt     = planes[0].virtualAddr;
            size_t ySize    = planes[0].size;
            int    deviceFd = deviceFd_;  // capture for deleter

            result.y_tensor = torch::from_blob(
                yVirt,
                {static_cast<int64_t>(height), static_cast<int64_t>(stride[0])},
                // Custom deleter: munmap when tensor data is no longer referenced
                [yVirt, ySize, deviceFd](void*) {
                    munmap(yVirt, ySize);
                    (void)deviceFd;
                },
                torch::kUInt8
            );
        }

        if (planes.size() >= 2) {
            void* uvVirt  = planes[1].virtualAddr;
            size_t uvSize = planes[1].size;

            result.uv_tensor = torch::from_blob(
                uvVirt,
                {static_cast<int64_t>(height / 2), static_cast<int64_t>(stride[1])},
                [uvVirt, uvSize](void*) {
                    munmap(uvVirt, uvSize);
                },
                torch::kUInt8
            );
        }
        break;
    }

    // ---- PATH C: DMA-BUF fd export (sg100) ----
    case ZeroCopyMode::DMA_BUF_FD: {
        if (!is_sg100()) {
            throw std::runtime_error(
                "DMA_BUF_FD mode is only supported on sg100 platform");
        }

        // Export DMA-BUF fd for each plane
        auto yFd = export_dma_buf_fd(devAddr[0]);
        if (!yFd) {
            throw std::runtime_error("Failed to export DMA-BUF fd for Y plane");
        }

        size_t ySize = stride[0] * height;
        void* yVirt = mmap_dma_buf(*yFd, ySize);
        int   yFdVal = *yFd;

        result.y_tensor = torch::from_blob(
            yVirt,
            {static_cast<int64_t>(height), static_cast<int64_t>(stride[0])},
            [yVirt, ySize, yFdVal](void*) {
                munmap(yVirt, ySize);
                close(yFdVal);
            },
            torch::kUInt8
        );

        if (devAddr[1] != 0) {
            auto uvFd = export_dma_buf_fd(devAddr[1]);
            if (uvFd) {
                size_t uvSize = stride[1] * (height / 2);
                void* uvVirt = mmap_dma_buf(*uvFd, uvSize);
                int   uvFdVal = *uvFd;

                result.uv_tensor = torch::from_blob(
                    uvVirt,
                    {static_cast<int64_t>(height / 2), static_cast<int64_t>(stride[1])},
                    [uvVirt, uvSize, uvFdVal](void*) {
                        munmap(uvVirt, uvSize);
                        close(uvFdVal);
                    },
                    torch::kUInt8
                );
            }
        }
        break;
    }
    } // switch

    // --- Populate metadata ---
    result.pts         = pts;
    result.width       = width;
    result.height      = height;
    result.stride_y    = stride[0];
    result.stride_uv   = stride[1];
    result.crop_width  = cropW;
    result.crop_height = cropH;
    result.crop_x      = cropX;
    result.crop_y      = cropY;
    result.pixel_format = pixFmt;
    result.frame_type  = frameType;
    result.data_size   = dataSize;

    return result;
}
