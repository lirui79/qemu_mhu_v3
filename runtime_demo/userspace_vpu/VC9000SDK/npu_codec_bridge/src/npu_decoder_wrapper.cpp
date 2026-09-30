/*
 * npu_codec_bridge — NPUDecoderWrapper implementation (LIVE mode)
 *
 * This file implements the full NPU decoder lifecycle using the
 * VastAI VMPP SDK.  It is only compiled in LIVE mode (NPU_LIVE_MODE=ON).
 */

#include "npu_decoder_wrapper.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>

#include <cstring>
#include <stdexcept>
#include <thread>

// The VastAI SDK headers (only available in LIVE mode)
extern "C" {
#include "vmpp_dec_api.h"
#include "vmpp_dec_defs.h"
}

// ---------------------------------------------------------------------------
// Constructor — full SDK lifecycle
// ---------------------------------------------------------------------------

NPUDecoderWrapper::NPUDecoderWrapper(const DecoderConfig& config)
    : config_(config)
{
    open_device();
    load_runtime();
    init_runtime();
    init_decoder();
    create_channel();
    start_channel();
}

NPUDecoderWrapper::~NPUDecoderWrapper()
{
    // Best-effort cleanup — order matters for SDK stability
    if (channel_) {
        running_ = false;
        if (input_thread_ && input_thread_->joinable()) {
            input_thread_->join();
        }
        // Release any pending frame
        if (last_sdk_frame_) {
            delete static_cast<vmppFrame*>(last_sdk_frame_);
            last_sdk_frame_ = nullptr;
        }
        // Drain pending frames before stopping
        flush(100);
        vmppDecStop(channel_);
        vmppDecDestroyChannel(&channel_);
        vmppDeInitDecoder();
    }
    if (vaccrt_deinit_) {
        using DeInitFn = uint32_t (*)(uint32_t);
        reinterpret_cast<DeInitFn>(vaccrt_deinit_)(0);
    }
    if (runtimeHandle_) {
        dlclose(runtimeHandle_);
    }
    if (deviceFd_ >= 0) {
        close(deviceFd_);
    }
}

// ---------------------------------------------------------------------------
// Step 1: open /dev/vastai_video*
// ---------------------------------------------------------------------------

void NPUDecoderWrapper::open_device()
{
    deviceFd_ = open(config_.devicePath.c_str(), O_RDWR);
    if (deviceFd_ < 0) {
        throw std::runtime_error(
            std::string("Cannot open ") + config_.devicePath + ": " +
            strerror(errno) +
            "\nCheck: ls -la /dev/vastai_video*"
        );
    }
}

// ---------------------------------------------------------------------------
// Step 2: dlopen libvaccrt.so
// ---------------------------------------------------------------------------

void NPUDecoderWrapper::load_runtime()
{
    runtimeHandle_ = dlopen("libvaccrt.so", RTLD_LAZY);
    if (!runtimeHandle_) {
        throw std::runtime_error(
            std::string("Cannot dlopen libvaccrt.so: ") + dlerror() +
            "\nCheck: 1) Is libvaccrt.so in LD_LIBRARY_PATH?\n"
            "        2) Try: find / -name libvaccrt.so 2>/dev/null\n"
            "        3) Then: export LD_LIBRARY_PATH=<found_dir>:$LD_LIBRARY_PATH"
        );
    }

    #define DLSYM(var, name) \
        do { \
            *(void**)(&var) = dlsym(runtimeHandle_, name); \
            char* err = dlerror(); \
            if (err) throw std::runtime_error( \
                std::string("dlsym(") + name + ") failed: " + err); \
        } while(0)

    DLSYM(vaccrt_init_,    "vaccrt_init");
    DLSYM(vaccrt_deinit_,  "vaccrt_deinit");
    DLSYM(vaccrt_malloc64_, "vaccrt_malloc64");
    DLSYM(vaccrt_free64_,  "vaccrt_free64");
    DLSYM(vaccrt_get_video_reserver_ddr_, "vaccrt_get_video_reserver_ddr");

    #undef DLSYM
}

// ---------------------------------------------------------------------------
// Step 3: vaccrt_init(dieId)
// ---------------------------------------------------------------------------

void NPUDecoderWrapper::init_runtime()
{
    // Extract die ID from device path: /dev/vastai_video<N>
    int dieId = 0;
    const char* dev = config_.devicePath.c_str();
    const char* prefix = "/dev/vastai_video";
    size_t plen = strlen(prefix);
    if (strncmp(dev, prefix, plen) == 0) {
        dieId = atoi(dev + plen);
    }

    using InitFn = uint32_t (*)(uint32_t);
    uint32_t ret = reinterpret_cast<InitFn>(vaccrt_init_)(static_cast<uint32_t>(dieId));
    if (ret != 0) {
        throw std::runtime_error(
            "vaccrt_init(" + std::to_string(dieId) + ") failed with code " +
            std::to_string(ret) +
            "\nCheck: Is the NPU driver loaded? (lsmod | grep vastai)"
        );
    }
}

// ---------------------------------------------------------------------------
// Step 4: vmppInitDecoder
// ---------------------------------------------------------------------------

void NPUDecoderWrapper::init_decoder()
{
    vmppConfiguration cfg;
    memset(&cfg, 0, sizeof(cfg));

    // Wire up the runtime instance
    cfg.runtimeInst.runtimeHandle = runtimeHandle_;
    cfg.runtimeInst.init           = vaccrt_init_;
    cfg.runtimeInst.DeInit         = vaccrt_deinit_;
    cfg.runtimeInst.mallocVideo    = vaccrt_malloc64_;
    cfg.runtimeInst.freeVideo      = vaccrt_free64_;
    cfg.runtimeInst.getVideoReserverDDR = vaccrt_get_video_reserver_ddr_;

    // Log context
    cfg.logCtx.enableCustomLog = 0;
    cfg.logCtx.logLevel = static_cast<vmppLogLevel>(config_.logLevel);

    vmppResult ret = vmppInitDecoder(&cfg);
    if (ret != vmpp_RSLT_OK) {
        throw std::runtime_error(
            "vmppInitDecoder failed with code " + std::to_string(ret)
        );
    }
}

// ---------------------------------------------------------------------------
// Step 5-6: vmppDecCreateChannel
// ---------------------------------------------------------------------------

void NPUDecoderWrapper::create_channel()
{
    vmppDecChannelParameters params;
    memset(&params, 0, sizeof(params));

    params.device       = static_cast<vmppDevice>(deviceFd_);
    params.sourceMode   = vmpp_SRC_FRAME;
    params.decodeMode   = vmpp_DEC_NORMAL;
    params.pixelFormat  = vmpp_PIX_FMT_NV12;
    params.maxWidth     = config_.maxWidth;
    params.maxHeight    = config_.maxHeight;
    params.streamBufferSize  = config_.streamBufferSize;
    params.extraBufferNumber = config_.extraBufferNumber;
    params.enProfiling  = 0;
    params.coreMode     = static_cast<vmppCoreMode>(config_.coreMode);
    params.memoryMode   = vmpp_DEC_MEM_NORMAL;
    params.outputAlign  = config_.outputAlign;
    params.apiMode      = config_.asyncInput ? vmpp_DEC_API_MODE_PARALLEL
                                             : vmpp_DEC_API_MODE_SERIAL;
    params.enSEIParser  = config_.enableSEIParser ? 1 : 0;
    params.noOutputReordering = config_.noOutputReordering ? 1 : 0;
    params.bufSlimMode  = config_.bufSlimMode ? 1 : 0;

    // Map codec type string → SDK enum
    const std::string& c = config_.codecType;
    if (c == "hevc" || c == "h265")       params.codecType = vmpp_CODEC_DEC_HEVC;
    else if (c == "av1")                  params.codecType = vmpp_CODEC_DEC_AV1;
    else if (c == "vp9")                  params.codecType = vmpp_CODEC_DEC_VP9;
    else if (c == "avs2")                 params.codecType = vmpp_CODEC_DEC_AVS2;
    else if (c == "jpeg")                 params.codecType = vmpp_CODEC_DEC_JPEG;
    else                                  params.codecType = vmpp_CODEC_DEC_H264;

    vmppResult ret = vmppDecCreateChannel(&channel_, &params);
    if (ret != vmpp_RSLT_OK || !channel_) {
        throw std::runtime_error(
            "vmppDecCreateChannel failed with code " + std::to_string(ret) +
            "\nCheck: 1) Is the device already in use? (sudo fuser -v /dev/vastai_video*)"
            "\n       2) Increase extraBufferNumber and recompile"
        );
    }

    // Start background input thread if async mode
    if (config_.asyncInput) {
        running_ = true;
        input_thread_ = std::make_unique<std::thread>(
            &NPUDecoderWrapper::input_thread_loop, this
        );
    }
}

// ---------------------------------------------------------------------------
// Step 7: vmppDecStart
// ---------------------------------------------------------------------------

void NPUDecoderWrapper::start_channel()
{
    vmppResult ret = vmppDecStart(channel_);
    if (ret < 0) {
        throw std::runtime_error(
            "vmppDecStart failed with code " + std::to_string(ret)
        );
    }
}

// ---------------------------------------------------------------------------
// Background input thread (async mode)
// ---------------------------------------------------------------------------

void NPUDecoderWrapper::input_thread_loop()
{
    // In async mode, the application calls decode() from another thread,
    // and we push data to the SDK here.  For simplicity, data is queued
    // via a lock-free approach.
    // The actual push happens in decode().
    (void)this;  // placeholder
}

// ---------------------------------------------------------------------------
// decode() — push an Access Unit to the SDK
// ---------------------------------------------------------------------------

bool NPUDecoderWrapper::decode(const uint8_t* data, size_t size, int64_t pts)
{
    if (!channel_) return false;

    vmppStream stream;
    memset(&stream, 0, sizeof(stream));
    stream.stream = const_cast<uint8_t*>(data);
    stream.len    = static_cast<uint32_t>(size);
    stream.pts    = pts;

    // SDK minimum timeout is 4000ms (VMPP_MIN_TIMEOUT_MS)
    static constexpr uint32_t kSendTimeout = 4000;
    vmppResult ret = vmppDecSendStream(channel_, &stream, kSendTimeout);
    if (ret < 0) {
        return false;
    }
    if (ret == vmpp_RSLT_DEC_INPUT_AGAIN) {
        return false;  // caller should retry
    }
    return true;
}

// ---------------------------------------------------------------------------
// get_frame() — pull a decoded frame from the SDK
// ---------------------------------------------------------------------------

std::optional<DecodedFrame> NPUDecoderWrapper::get_frame(uint32_t timeoutMs)
{
    if (!channel_) return std::nullopt;

    vmppFrame frame;
    memset(&frame, 0, sizeof(frame));

    vmppDecOutputOptions opt;
    opt.memoryType = vmpp_MEM_HOST;
    opt.enableCrop = 0;

    uint32_t effectiveTimeout = (timeoutMs < 4000) ? 4000 : timeoutMs;
    vmppResult ret = vmppDecReceiveFrame(channel_, &frame, &opt, effectiveTimeout);
    if (ret != vmpp_RSLT_OK) {
        return std::nullopt;  // no frame ready, or EOS
    }

    DecodedFrame result;
    result.y_tensor = torch::from_blob(
        frame.data[0],
        {static_cast<int64_t>(frame.height),
         static_cast<int64_t>(frame.stride[0])},
        torch::kUInt8
    );

    if (frame.data[1]) {
        result.uv_tensor = torch::from_blob(
            frame.data[1],
            {static_cast<int64_t>(frame.height / 2),
             static_cast<int64_t>(frame.stride[1])},
            torch::kUInt8
        );
    }

    result.pts          = frame.pts;
    result.width        = frame.width;
    result.height       = frame.height;
    result.stride_y     = frame.stride[0];
    result.stride_uv    = frame.stride[1];
    result.crop_width   = frame.cropInfo.width;
    result.crop_height  = frame.cropInfo.height;
    result.crop_x       = frame.cropInfo.xOffset;
    result.crop_y       = frame.cropInfo.yOffset;
    result.frame_type   = static_cast<int>(frame.frameType);
    result.data_size    = frame.dataSize;

    // Store SDK frame for release_frame (deep copy to heap)
    if (!last_sdk_frame_) {
        last_sdk_frame_ = new vmppFrame;
    }
    memcpy(last_sdk_frame_, &frame, sizeof(vmppFrame));

    return result;
}

// ---------------------------------------------------------------------------
// release_frame()
// ---------------------------------------------------------------------------

void NPUDecoderWrapper::release_frame(const DecodedFrame& /*frame*/)
{
    if (!channel_ || !last_sdk_frame_) return;

    vmppFrame* sdkFrame = static_cast<vmppFrame*>(last_sdk_frame_);
    if (sdkFrame->data[0]) {
        vmppDecReleaseFrame(channel_, sdkFrame, 500);
    }
    delete sdkFrame;
    last_sdk_frame_ = nullptr;
    active_frame_.reset();
}

// ---------------------------------------------------------------------------
// flush() — drain all pending frames
// ---------------------------------------------------------------------------

std::vector<DecodedFrame> NPUDecoderWrapper::flush(uint32_t timeoutMs)
{
    std::vector<DecodedFrame> frames;
    while (true) {
        auto frame = get_frame(timeoutMs);
        if (!frame) break;
        frames.push_back(std::move(*frame));
    }
    return frames;
}

// ---------------------------------------------------------------------------
// seek / reset
// ---------------------------------------------------------------------------

int64_t NPUDecoderWrapper::seek(double targetTimestampMs)
{
    (void)targetTimestampMs;
    // SDK has no native seek — hard reset then skip frames
    reset();
    return 0;
}

void NPUDecoderWrapper::reset()
{
    if (channel_) {
        running_ = false;
        if (input_thread_) {
            input_thread_->join();
            input_thread_.reset();
        }
        vmppDecStop(channel_);
        vmppDecDestroyChannel(&channel_);
    }
    create_channel();
    start_channel();
}

// ---------------------------------------------------------------------------
// probe_stream()
// ---------------------------------------------------------------------------

NPUDecoderWrapper::StreamInfo
NPUDecoderWrapper::probe_stream(const uint8_t* data, size_t size)
{
    StreamInfo info;
    info.width  = config_.maxWidth / 2;   // placeholder until real probe
    info.height = config_.maxHeight / 2;
    info.fps    = 30.0f;
    return info;
}

int32_t NPUDecoderWrapper::idle_dpb_count()
{
    if (!channel_) return 0;
    return vmppDecGetIdleDpbBufferCount(channel_);
}

std::string NPUDecoderWrapper::state_string()
{
    if (!channel_) return "UNINITIALIZED";
    vmppDecStatus status;
    memset(&status, 0, sizeof(status));
    vmppDecGetStatus(channel_, &status);
    switch (status.state) {
        case vmpp_ST_NONE:    return "NONE";
        case vmpp_ST_READY:   return "READY";
        case vmpp_ST_RUNNING: return "RUNNING";
        case vmpp_ST_ERROR:   return "ERROR";
        default:              return "UNKNOWN";
    }
}
