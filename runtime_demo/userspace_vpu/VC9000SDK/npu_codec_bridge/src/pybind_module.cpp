/*
 * npu_codec_bridge — Pybind11 module
 *
 * Two build modes:
 *   NPU_STUB_MODE=1  → Mock decoder, no hardware needed, returns random tensors
 *   NPU_LIVE_MODE=1  → Real VastAI NPU SDK decoder
 */

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <torch/extension.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <random>

#include "npu_decoder_wrapper.h"

namespace py = pybind11;

// =========================================================================
// STUB Implementation (no NPU hardware required)
// =========================================================================
#ifdef NPU_STUB_MODE

class StubNPUDecoder {
public:
    explicit StubNPUDecoder(const DecoderConfig& config)
        : config_(config), rng_(std::random_device{}())
    {
        // Simulate probing: use the config dimensions
        width_  = 1920;
        height_ = 1080;
    }

    bool decode(const uint8_t* data, size_t size, int64_t pts) {
        (void)data;
        // Simulate decoding: produce one frame per decode call
        std::lock_guard<std::mutex> lock(mutex_);
        pending_frames_++;
        return true;
    }

    std::optional<py::dict> get_frame(uint32_t timeoutMs = 500) {
        (void)timeoutMs;
        std::lock_guard<std::mutex> lock(mutex_);

        if (pending_frames_ <= 0) {
            return std::nullopt;
        }
        pending_frames_--;

        // Generate a random NV12 frame
        int stride_y  = ((width_ + 63) / 64) * 64;   // 64-byte aligned
        int stride_uv = stride_y;
        int y_size    = stride_y * height_;
        int uv_size   = stride_uv * (height_ / 2);

        // Create random Y plane
        auto y_options = torch::TensorOptions().dtype(torch::kUInt8).device(torch::kCPU);
        auto y_tensor  = torch::randint(16, 235, {height_, stride_y}, y_options)
                              .to(torch::kUInt8);

        // Create random UV plane (simulated interleaved UV)
        auto uv_tensor = torch::randint(16, 240, {height_ / 2, stride_uv}, y_options)
                              .to(torch::kUInt8);

        // Store tensors so they survive until release_frame
        cached_y_  = y_tensor;
        cached_uv_ = uv_tensor;

        py::dict result;
        result["y_tensor"]   = y_tensor;
        result["uv_tensor"]  = uv_tensor;
        result["pts"]        = static_cast<int64_t>(frame_count_);
        result["width"]      = width_;
        result["height"]     = height_;
        result["crop_width"] = width_;
        result["crop_height"]= height_;
        result["crop_x"]     = 0;
        result["crop_y"]     = 0;
        result["frame_type"] = std::string(frame_count_ == 0 ? "I" : (frame_count_ % 30 == 0 ? "I" : (frame_count_ % 5 == 0 ? "P" : "B")));

        frame_count_++;
        return result;
    }

    void release_frame() {
        std::lock_guard<std::mutex> lock(mutex_);
        cached_y_  = torch::Tensor{};
        cached_uv_ = torch::Tensor{};
    }

    std::vector<py::dict> flush(uint32_t timeoutMs = 500) {
        (void)timeoutMs;
        std::vector<py::dict> frames;
        std::lock_guard<std::mutex> lock(mutex_);

        while (pending_frames_ > 0) {
            pending_frames_--;
            auto opt = get_frame(0);
            if (opt) {
                frames.push_back(*opt);
            }
        }
        return frames;
    }

    int64_t seek(double targetTimestampMs) {
        (void)targetTimestampMs;
        return 0;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_frames_ = 0;
        frame_count_    = 0;
    }

    py::dict probe(const uint8_t* data, size_t size) {
        (void)data;
        (void)size;
        py::dict info;
        info["width"]      = 1920;
        info["height"]     = 1080;
        info["average_fps"]= 30.0;
        info["codec"]      = std::string("h264");
        info["bit_rate"]   = 0.0;
        return info;
    }

    int32_t idle_dpb_count() { return 8; }
    std::string state()     { return "STUB_RUNNING"; }

    void set_sei_callback(py::function cb) { sei_cb_ = cb; }

private:
    DecoderConfig     config_;
    std::mt19937       rng_;
    std::mutex         mutex_;
    uint32_t           width_{1920};
    uint32_t           height_{1080};
    int                pending_frames_{0};
    int                frame_count_{0};
    torch::Tensor      cached_y_;
    torch::Tensor      cached_uv_;
    py::function       sei_cb_;
};

#else
// =========================================================================
// LIVE Implementation (real NPU SDK)
// =========================================================================

class LiveNPUDecoder {
public:
    explicit LiveNPUDecoder(const DecoderConfig& config) {
        try {
            wrapper_ = std::make_unique<NPUDecoderWrapper>(config);
        } catch (const std::exception& e) {
            throw std::runtime_error(
                std::string("Failed to initialize NPU decoder: ") + e.what() +
                "\nCheck: 1) /dev/vastai_video* exists and is accessible\n"
                "       2) libvaccrt.so is in LD_LIBRARY_PATH\n"
                "       3) NPU driver is loaded (lsmod | grep vastai)\n"
                "       4) No other process is using the device (sudo fuser -v /dev/vastai_video*)"
            );
        }
    }

    bool decode(const uint8_t* data, size_t size, int64_t pts) {
        return wrapper_->decode(data, size, pts);
    }

    std::optional<py::dict> get_frame(uint32_t timeoutMs = 500) {
        auto frame = wrapper_->get_frame(timeoutMs);
        if (!frame) return std::nullopt;
        last_frame_.emplace(std::move(*frame));
        return frame_to_dict(*last_frame_);
    }

    void release_frame() {
        if (last_frame_.has_value()) {
            wrapper_->release_frame(*last_frame_);
            last_frame_.reset();
        }
    }

    std::vector<py::dict> flush(uint32_t timeoutMs = 500) {
        auto frames = wrapper_->flush(timeoutMs);
        std::vector<py::dict> result;
        result.reserve(frames.size());
        for (auto& f : frames) {
            result.push_back(frame_to_dict(f));
            wrapper_->release_frame(f);
        }
        return result;
    }

    int64_t seek(double ts) { return wrapper_->seek(ts); }
    void reset() { wrapper_->reset(); }

    py::dict probe(const uint8_t* data, size_t size) {
        auto info = wrapper_->probe_stream(data, size);
        py::dict d;
        d["width"]       = info.width;
        d["height"]      = info.height;
        d["cropWidth"]   = info.cropWidth;
        d["cropHeight"]  = info.cropHeight;
        d["average_fps"] = info.fps;
        d["pixelFormat"] = static_cast<int>(info.pixelFormat);
        return d;
    }

    int32_t idle_dpb_count() { return wrapper_->idle_dpb_count(); }
    std::string state() { return wrapper_->state_string(); }

    void set_sei_callback(py::function cb) {
        wrapper_->set_sei_callback(
            [cb](int type, const uint8_t* data, uint32_t size) {
                py::gil_scoped_acquire gil;
                cb(type, py::bytes(reinterpret_cast<const char*>(data), size));
            }
        );
    }

private:
    static py::dict frame_to_dict(const DecodedFrame& f) {
        py::dict d;
        d["y_tensor"]    = f.y_tensor;
        d["uv_tensor"]   = f.uv_tensor;
        d["pts"]         = f.pts;
        d["width"]       = f.width;
        d["height"]      = f.height;
        d["crop_width"]  = f.crop_width;
        d["crop_height"] = f.crop_height;
        d["crop_x"]      = f.crop_x;
        d["crop_y"]      = f.crop_y;
        const char* ftype = (f.frame_type == 0) ? "I" : ((f.frame_type == 1) ? "P" : "B");
        d["frame_type"]  = std::string(ftype);
        return d;
    }

    std::unique_ptr<NPUDecoderWrapper> wrapper_;
    std::optional<DecodedFrame>        last_frame_;
};

#endif // NPU_STUB_MODE vs NPU_LIVE_MODE

// =========================================================================
// Python-facing PyNPUDecoder (thin GIL-handling wrapper, shared by both modes)
// =========================================================================

class PyNPUDecoder {
public:
    explicit PyNPUDecoder(const DecoderConfig& config) {
#ifdef NPU_STUB_MODE
        impl_ = std::make_unique<StubNPUDecoder>(config);
#else
        impl_ = std::make_unique<LiveNPUDecoder>(config);
#endif
    }

    bool decode(py::buffer data, int64_t pts = -1) {
        py::buffer_info info = data.request();
        return impl_->decode(
            static_cast<const uint8_t*>(info.ptr), info.size, pts);
    }

    std::optional<py::dict> get_frame(uint32_t timeoutMs = 500) {
        auto frame = impl_->get_frame(timeoutMs);
        if (frame) {
            current_frame_ = *frame;
        }
        return frame;
    }

    void release_frame() {
        impl_->release_frame();
        current_frame_.reset();
    }

    std::vector<py::dict> flush(uint32_t timeoutMs = 500) {
        return impl_->flush(timeoutMs);
    }

    int64_t seek(double targetTimestampMs) {
        return impl_->seek(targetTimestampMs);
    }

    void reset() { impl_->reset(); }

    py::dict probe(py::buffer data) {
        py::buffer_info info = data.request();
        return impl_->probe(
            static_cast<const uint8_t*>(info.ptr), info.size);
    }

    int32_t idle_dpb_count() { return impl_->idle_dpb_count(); }
    std::string state()      { return impl_->state(); }

    void set_sei_callback(py::function cb) {
        impl_->set_sei_callback(cb);
    }

private:
#ifdef NPU_STUB_MODE
    std::unique_ptr<StubNPUDecoder> impl_;
#else
    std::unique_ptr<LiveNPUDecoder> impl_;
#endif
    std::optional<py::dict> current_frame_;
};

// =========================================================================
// Module definition
// =========================================================================

PYBIND11_MODULE(_C, m) {
    m.doc() = "NPU Codec Bridge C++ extension (pybind11)";

// Version tag
#ifdef NPU_STUB_MODE
    m.attr("__mode__") = "stub";
    m.attr("__version__") = "0.1.0-stub";
#else
    m.attr("__mode__") = "live";
    m.attr("__version__") = "0.1.0-live";
#endif

    // DecoderConfig
    py::class_<DecoderConfig>(m, "DecoderConfig")
        .def(py::init<>())
        .def_readwrite("device_path",         &DecoderConfig::devicePath)
        .def_readwrite("codec_type",          &DecoderConfig::codecType)
        .def_readwrite("pixel_format",        &DecoderConfig::pixelFormat)
        .def_readwrite("memory_mode",         &DecoderConfig::memoryMode)
        .def_readwrite("zero_copy_mode",      &DecoderConfig::zeroCopyMode)
        .def_readwrite("max_width",           &DecoderConfig::maxWidth)
        .def_readwrite("max_height",          &DecoderConfig::maxHeight)
        .def_readwrite("stream_buffer_size",  &DecoderConfig::streamBufferSize)
        .def_readwrite("extra_buffer_number", &DecoderConfig::extraBufferNumber)
        .def_readwrite("output_align",        &DecoderConfig::outputAlign)
        .def_readwrite("enable_sei_parser",   &DecoderConfig::enableSEIParser)
        .def_readwrite("no_output_reordering",&DecoderConfig::noOutputReordering)
        .def_readwrite("buf_slim_mode",       &DecoderConfig::bufSlimMode)
        .def_readwrite("log_level",           &DecoderConfig::logLevel)
        .def_readwrite("core_mode",           &DecoderConfig::coreMode)
        .def_readwrite("async_input",         &DecoderConfig::asyncInput);

    // NPUDecoder
    py::class_<PyNPUDecoder>(m, "NPUDecoder")
        .def(py::init<const DecoderConfig&>(),
             py::arg("config") = DecoderConfig{},
             "Create an NPU video decoder instance.")
        .def("decode",           &PyNPUDecoder::decode,
             py::arg("data"), py::arg("pts") = -1,
             "Push a compressed access unit to the decoder.")
        .def("get_frame",        &PyNPUDecoder::get_frame,
             py::arg("timeout_ms") = 500,
             "Pull a decoded frame as NV12 tensors. Returns None if no frame ready.")
        .def("release_frame",    &PyNPUDecoder::release_frame,
             "Release the last frame back to the buffer pool.")
        .def("flush",            &PyNPUDecoder::flush,
             py::arg("timeout_ms") = 500,
             "Drain all pending frames from the decoder.")
        .def("seek",             &PyNPUDecoder::seek,
             py::arg("target_timestamp_ms"),
             "Seek to a target timestamp.")
        .def("reset",            &PyNPUDecoder::reset,
             "Hard reset the decoder.")
        .def("probe",            &PyNPUDecoder::probe,
             py::arg("data"),
             "Probe a bitstream prefix for stream info.")
        .def("idle_dpb_count",   &PyNPUDecoder::idle_dpb_count,
             "Number of free DPB buffers.")
        .def("state",            &PyNPUDecoder::state,
             "Current decoder state string.")
        .def("set_sei_callback", &PyNPUDecoder::set_sei_callback,
             py::arg("callback"),
             "Register a callback for SEI messages.");
}
