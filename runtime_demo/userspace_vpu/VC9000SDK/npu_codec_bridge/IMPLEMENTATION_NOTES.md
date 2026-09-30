/*
 * npu_codec_bridge — Implementation Notes & Developer Guide
 *
 * This document explains the critical design decisions, memory model,
 * and thread safety contract for the NPUDecoderWrapper.
 */

================================================================================
1. COMPLETE LIFECYCLE (matches the SDK lifecycle from the analysis report)
================================================================================

  Python side:
    dec = NPUDecoder(config)
    dec.decode(au_data, pts=0)
    frame = dec.get_frame()
    # ... use frame["y_tensor"], frame["uv_tensor"] ...
    dec.release_frame()
    dec.flush()
    # destructor calls reset() + cleanup

  C++ side (inside NPUDecoderWrapper constructor):
    deviceFd_ = open("/dev/vastai_video0", O_RDWR)           // Step 1
    dlopen("libvaccrt.so") + dlsym(vaccrt_init, ...)          // Step 2
    vaccrt_init(dieId)                                        // Step 3
    vmppInitDecoder(&cfg)                                     // Step 4
    vmppDecGetAvailableChannels(...)                          // Step 5 (validation)
    vmppDecCreateChannel(&channel_, &chParams)                // Step 6
    vmppDecStart(channel_)                                    // Step 7
    [if asyncInput] spawn input_thread                        // Step 8

  C++ side (destructor):
    running_ = false
    [if asyncInput] input_thread_.join()
    vmppDecStop(channel_)                                     // Step 9
    vmppDecDestroyChannel(&channel_)                          // Step 10
    vmppDeInitDecoder()                                       // Step 11
    vaccrt_deinit(dieId)
    dlclose(runtimeHandle_)
    close(deviceFd_)                                          // Step 12


================================================================================
2. THREADING MODEL
================================================================================

  When config.asyncInput == true (default, SDK's PARALLEL API mode):

    ┌─────────────────────┐       ┌──────────────────────┐
    │  Caller Thread      │       │  input_thread_       │
    │  (Python GIL)       │       │  (no GIL)            │
    ├─────────────────────┤       ├──────────────────────┤
    │  decode() ──────────┼──►    │  vmppDecSendStream() │
    │                     │       │  (blocks if queue    │
    │                     │       │   full)              │
    │  get_frame() ───────┼──►    │                      │
    │  (polls or blocks)  │       │                      │
    │                     │       │                      │
    │  release_frame() ───┼──►    │                      │
    └─────────────────────┘       └──────────────────────┘

  When config.asyncInput == false (SDK's SERIAL API mode):
    - decode() calls vmppDecSendStream directly
    - get_frame() calls vmppDecReceiveFrame in a loop until no more frames
    - Single-threaded, simpler, lower performance


================================================================================
3. MEMORY LIFETIME CONTRACT (critical for correctness)
================================================================================

  The torch::from_blob tensors share memory with either:
    (a) SDK-internal host bounce buffers  (COPY_TO_HOST), or
    (b) mmap'd NPU device memory          (MMAP_DEVICE / DMA_BUF_FD)

  THE RULE: tensors are valid ONLY between get_frame() and release_frame().

  Correct:
    frame = dec.get_frame()
    y = frame["y_tensor"].clone()     # deep copy — safe to hold
    uv = frame["uv_tensor"].clone()   # deep copy — safe to hold
    dec.release_frame()               # SDK can now recycle buffers
    # y, uv are independently owned

  ALSO correct (if you process the tensor synchronously):
    frame = dec.get_frame()
    result = my_kernel(frame["y_tensor"])  # GPU kernel finishes before release
    torch.cuda.synchronize()
    dec.release_frame()

  WRONG (will cause use-after-free / corrupted data):
    frame = dec.get_frame()
    tensor_ref = frame["y_tensor"]    # aliases SDK buffer
    dec.release_frame()               # SDK recycles buffer
    print(tensor_ref[0, 0])           # USE AFTER FREE!


================================================================================
4. NV12 TENSOR LAYOUT
================================================================================

  The SDK outputs NV12 (semi-planar YUV 4:2:0).
  Each decoded plane is returned as a 2D torch tensor:

  y_tensor:
    shape:   [height, stride_y]
    dtype:   uint8  (uint16 for P010)
    stride:  [stride_y, 1]  (row-major)
    The valid pixel region is [0:height, 0:width].
    Pixels beyond column `width` are padding (alignment).

  uv_tensor:
    shape:   [height/2, stride_uv]
    dtype:   uint8  (uint16 for P010)
    stride:  [stride_uv, 1]
    UV pairs are interleaved: U₀V₀U₁V₁U₂V₂...
    The valid UV region is [0:height/2, 0:(crop_width aligned to 2)].

  For FFmpeg/swscale integration, you'll need either:
    (a) AV_PIX_FMT_NV12 — FFmpeg natively supports NV12
    (b) A small CUDA/CPU kernel to convert NV12 → YUV420P if needed


================================================================================
5. ZERO-COPY PATH: WHEN IT WORKS (AND WHEN IT DOESN'T)
================================================================================

  COPY_TO_HOST (always works):
    ✓ Works on all platforms (sv100, sg100)
    ✓ Simple, no driver dependencies beyond basic SDK
    ✗ One DMA copy from NPU DDR → Host (typically < 1ms for 4K)
    ✓ Still "zero-copy" from host → PyTorch (torch::from_blob)

  MMAP_DEVICE (needs driver mmap support):
    ✓ True zero-copy: NPU DDR → Tensor with no intermediate copy
    ✓ Best for high-throughput transcoding
    ✗ Requires the kernel driver to implement .mmap on /dev/vastai_video*
    ✗ Requires knowing the NPU DDR base address
    ✗ Cache coherency: may need explicit cache flush/invalidate
      (the driver should handle this via DMA API)

  DMA_BUF_FD (sg100 only):
    ✓ Standard Linux DMA-BUF protocol
    ✓ Interoperable with GPU drivers (VA-API, DRM)
    ✗ Only available on sg100 platform
    ✗ Requires GEM handle → DMA-BUF fd conversion

  RECOMMENDATION:
    Start with COPY_TO_HOST — it's the simplest and most reliable path.
    The DMA copy overhead for NV12 4K is ~24MB (4096×2160×1.5 = 12.6MB
    for 8-bit, 25MB for 10-bit), which at PCIe Gen3 x8 (~8GB/s) is ~3ms.
    Only invest in MMAP_DEVICE if you measure this as a bottleneck.


================================================================================
6. SEEK IMPLEMENTATION
================================================================================

  The SDK has NO native seek support. Our seek() implementation:

  1. If seeking FORWARD by less than ~1 second:
     → Fast path: decode+discard frames until target PTS is reached.

  2. If seeking BACKWARD or far forward:
     → Hard path:
       a. vmppDecStop(channel_)
       b. vmppDecDestroyChannel(&channel_)
       c. Reposition input stream to nearest I-frame before target.
          (Caller must provide an index of I-frame positions.)
       d. vmppDecCreateChannel(&channel_, &params)
       e. vmppDecStart(channel_)
       f. Decode without retrieving frames until target PTS is reached.
       g. Resume normal decode+get_frame loop.

  This is expensive (~50-100ms for channel re-creation) but functionally
  correct. For video players, consider caching the last few seconds of
  decoded frames to avoid decoder restart on small backward seeks.


================================================================================
7. BUILD / DEPENDENCY SUMMARY
================================================================================

  Required:
    - libvmpp-dec.so          (VastAI decoder SDK)
    - libvaccrt.so            (VastAI runtime library)
    - /dev/vastai_video*      (DRM device nodes)
    - libtorch (PyTorch C++)  (for torch::Tensor)
    - pybind11                (for Python bindings)

  Optional (for zero-copy paths):
    - Kernel driver with mmap support on /dev/vastai_video*
    - libdrm (for DRM_IOCTL_PRIME_HANDLE_TO_FD on sg100)
