#!/usr/bin/env python3
"""
02_performance_benchmark.py — CPU vs NPU decoding throughput comparison.

Decodes the first N frames of a video using both device='cpu' (original
torchcodec / FFmpeg) and device='npu' (VastAI NPU hardware), then prints
a side-by-side latency and throughput comparison.

┌─────────────────────────────────────────────────────────────────┐
│  IMPORTANT: In STUB mode (development without NPU hardware),    │
│  the NPU numbers represent the Python/C++ bridge overhead      │
│  only — the stub returns pre-generated random frames and        │
│  does not go through real NPU DMA transfers.                    │
│                                                                 │
│  For real NPU benchmarks, build with:                           │
│    NPU_LIVE_MODE=1 pip install -e .                             │
│  and run on a machine with VastAI NPU hardware.                 │
└─────────────────────────────────────────────────────────────────┘

Usage:
    python 02_performance_benchmark.py [path/to/video.mp4] [--frames 100]

Requirements:
    pip install torchcodec
"""

import argparse
import os
import sys
import time

# =========================================================================
# The magic import — must come before torchcodec
# =========================================================================
try:
    import npu_codec_bridge  # noqa: F401
except ImportError:
    print("ERROR: npu_codec_bridge not installed.", file=sys.stderr)
    print("Run: cd npu_codec_bridge && pip install -e .", file=sys.stderr)
    sys.exit(1)

from npu_codec_bridge import _C

from torchcodec.decoders import VideoDecoder
import torch


def _benchmark_nv12_raw(video_path: str, num_frames: int) -> dict:
    """
    Measure pure NPU hardware decode throughput — NV12 output, no RGB conversion.

    Creates a fresh NPUVideoDecoder, decodes all frames in one batch
    (_decode_up_to sends all AUs then pulls all frames at once),
    and returns FPS + per-frame latency of the raw NV12 pipeline.
    """
    from npu_codec_bridge import NPUVideoDecoder

    n_dec = NPUVideoDecoder(video_path, device="npu")
    total = len(n_dec)
    n = min(num_frames, total - 4)  # reserve DPB tail
    if n < 2:
        return None

    # Single batch: decode frames 0..n-1 in one shot
    t0 = time.perf_counter()
    n_dec._decode_up_to(n - 1)       # sends all AUs, pulls all frames (NV12)
    t1 = time.perf_counter()
    wall = t1 - t0

    n_decoded = len(n_dec._decoded_frames)
    avg_us = (wall / max(n_decoded, 1)) * 1_000_000

    # Show NV12 plane dims from last frame
    if n_dec._decoded_frames:
        fm = n_dec._decoded_frames[-1]
        y_shape  = list(fm.y_tensor.shape)
        uv_shape = list(fm.uv_tensor.shape)
        y_dtype  = str(fm.y_tensor.dtype)
    else:
        y_shape = uv_shape = []
        y_dtype = "?"

    return {
        "label": f"NPU NV12 (raw decode, skip RGB)",
        "n_frames": n_decoded,
        "wall_seconds": wall,
        "fps": n_decoded / wall if wall > 0 else 0,
        "avg_us": avg_us,
        "p50_us": 0,
        "p95_us": 0,
        "p99_us": 0,
        "min_us": 0,
        "max_us": 0,
        "nv12_y": y_shape,
        "nv12_uv": uv_shape,
        "nv12_dtype": y_dtype,
        "detail": f"batch decode {n_decoded} frames in {wall*1000:.0f}ms"
    }


def benchmark_decoder(dec, num_frames: int, label: str) -> dict:
    """
    Time `num_frames` frame retrievals from a decoder.

    Uses both __getitem__ (raw tensor) and get_frame_at (with metadata)
    to exercise the full API surface.

    Returns a dict with timing statistics.
    """
    n = min(num_frames, len(dec))
    if n == 0:
        return {"label": label, "n_frames": 0, "error": "No frames to decode"}

    # ---- Warm-up: decode first frame (may include init overhead) ----
    _ = dec[0]

    # ---- Benchmark: __getitem__ (raw tensor, no metadata) ----
    latencies_us = []
    t_start = time.perf_counter()

    for i in range(n):
        t0 = time.perf_counter()
        _ = dec[i]
        t1 = time.perf_counter()
        latencies_us.append((t1 - t0) * 1_000_000)

    t_end = time.perf_counter()
    wall_seconds = t_end - t_start

    latencies_us.sort()
    p50 = latencies_us[len(latencies_us) // 2]
    p95 = latencies_us[int(len(latencies_us) * 0.95)]
    p99 = latencies_us[int(len(latencies_us) * 0.99)]

    return {
        "label": label,
        "n_frames": n,
        "wall_seconds": wall_seconds,
        "fps": n / wall_seconds if wall_seconds > 0 else 0.0,
        "avg_us": sum(latencies_us) / len(latencies_us),
        "p50_us": p50,
        "p95_us": p95,
        "p99_us": p99,
        "min_us": latencies_us[0],
        "max_us": latencies_us[-1],
    }


def print_results(r: dict):
    """Pretty-print benchmark results."""
    if "error" in r:
        print(f"  {r['label']}: {r['error']}")
        return

    print(f"  {'─' * 55}")
    print(f"  {r['label']}")
    print(f"  {'─' * 55}")
    print(f"    Frames decoded  : {r['n_frames']}")
    print(f"    Wall time       : {r['wall_seconds']:.3f}s")
    print(f"    Throughput      : {r['fps']:.1f} fps")
    if r.get('avg_us', 0) > 0:
        print(f"    Latency (avg)   : {r['avg_us']:.0f} µs/frame")
    if r.get('p50_us', 0) > 0:
        print(f"    Latency (p50)   : {r['p50_us']:.0f} µs")
        print(f"    Latency (p95)   : {r['p95_us']:.0f} µs")
        print(f"    Latency (p99)   : {r['p99_us']:.0f} µs")
        print(f"    Latency (min)   : {r['min_us']:.0f} µs")
        print(f"    Latency (max)   : {r['max_us']:.0f} µs")
    if r.get('detail'):
        print(f"    Detail          : {r['detail']}")
    if r.get('nv12_y'):
        print(f"    NV12 Y plane    : {r['nv12_y']}, dtype={r['nv12_dtype']}")
        print(f"    NV12 UV plane   : {r['nv12_uv']}")


def main():
    parser = argparse.ArgumentParser(
        description="CPU vs NPU Decoding Performance Benchmark"
    )
    parser.add_argument(
        "video", nargs="?", default=None,
        help="Path to video file. Defaults to repo's fixed_hevc.mp4."
    )
    parser.add_argument(
        "--frames", "-n", type=int, default=100,
        help="Number of frames to decode (default: 100)"
    )
    parser.add_argument(
        "--skip-cpu", action="store_true",
        help="Skip CPU benchmark (NPU only)"
    )
    args = parser.parse_args()

    # Fallback to repo test video
    if args.video is None:
        repo_root = os.path.abspath(
            os.path.join(os.path.dirname(__file__), "..", "..")
        )
        args.video = os.path.join(repo_root, "fixed_hevc.mp4")
        if not os.path.exists(args.video):
            print("ERROR: No video file provided.")
            sys.exit(1)

    print(f"{'=' * 60}")
    print(f"  Decoding Performance Benchmark")
    print(f"{'=' * 60}")
    print(f"  Video:   {args.video}")
    print(f"  Frames:  {args.frames}")
    print(f"  Mode:    {'STUB' if _C.__mode__ == 'stub' else 'LIVE'}")
    print()

    results = {}

    # ==================================================================
    # NPU Benchmark — RGB path (via torchcodec API)
    # ==================================================================
    print("[1/3] Benchmarking NPU decoder — RGB path (torchcodec API)...")
    npu_dec = VideoDecoder(args.video, device="npu")

    # Print metadata
    meta = npu_dec.metadata
    print(f"      Resolution: {meta.width}x{meta.height}, "
          f"{meta.num_frames} frames @ {meta.average_fps:.1f} fps")

    results["npu_rgb"] = benchmark_decoder(npu_dec, args.frames,
                                            "NPU RGB (decode + NV12→RGB)")
    print_results(results["npu_rgb"])

    # ==================================================================
    # NPU Benchmark — raw NV12 path (skip RGB, decode only)
    # ==================================================================
    print("\n[2/3] Benchmarking NPU decoder — raw NV12 (decode only)...")

    results["npu_nv12"] = _benchmark_nv12_raw(args.video, args.frames)
    if results["npu_nv12"]:
        print_results(results["npu_nv12"])

    # ==================================================================
    # CPU Benchmark
    # ==================================================================
    if not args.skip_cpu:
        print("\n[3/3] Benchmarking CPU decoder (device='cpu')...")
        try:
            cpu_dec = VideoDecoder(args.video, device="cpu")
            meta = cpu_dec.metadata
            print(f"      Resolution: {meta.width}x{meta.height}, "
                  f"{meta.num_frames} frames @ {meta.average_fps:.1f} fps")

            results["cpu"] = benchmark_decoder(cpu_dec, args.frames,
                                                "CPU (torchcodec/FFmpeg)")
            print_results(results["cpu"])
        except Exception as e:
            print(f"      CPU decoder failed: {e}")
            results["cpu"] = {"label": "CPU", "error": str(e)}

    # ==================================================================
    # Comparison Summary
    # ==================================================================
    print(f"\n{'=' * 60}")
    print(f"  Comparison Summary")
    print(f"{'=' * 60}")

    # Show NPU RGB vs NPU NV12 vs CPU comparison
    npu_rgb  = results.get("npu_rgb", {})
    npu_nv12 = results.get("npu_nv12", {})
    cpu_r    = results.get("cpu", {})

    npu_rgb_fps  = npu_rgb.get("fps", 0)
    npu_nv12_fps = npu_nv12.get("fps", 0)
    cpu_fps      = cpu_r.get("fps", 0)

    if npu_rgb_fps > 0:
        print(f"  {'Metric':<26} {'NPU+RGB':>10} {'NPU NV12':>10} {'CPU':>10}")
        print(f"  {'─' * 60}")
        print(f"  {'Throughput (fps)':<26} {npu_rgb_fps:>10.1f} "
              f"{npu_nv12_fps:>10.1f} {cpu_fps:>10.1f}")
    if npu_rgb.get("avg_us", 0) > 0:
        rgb_us  = npu_rgb["avg_us"]
        nv12_us = npu_nv12.get("avg_us", 0)
        cpu_us  = cpu_r.get("avg_us", 0)
        print(f"  {'Latency avg (µs/frame)':<26} {rgb_us:>10.0f} "
              f"{nv12_us:>10.0f} {cpu_us:>10.0f}")
        if nv12_us > 0:
            print(f"  {'RGB overhead (µs/frame)':<26} {'':>10} "
                  f"{rgb_us - nv12_us:>10.0f} {'':>10}")
            print(f"  {'RGB slowdown':<26} {'':>10} "
                  f"{rgb_us / nv12_us:>9.1f}x {'':>10}")

    mode = _C.__mode__
    if mode == "live":
        print(f"\n  ✓ NPU NV12 shows raw hardware decode throughput.")
        print(f"  ✓ NPU+RGB includes Python _nv12_to_rgb_nchw() overhead.")
        print(f"  ✓ For transcoding pipelines, use raw NV12 to maximize FPS.")
    else:
        print(f"\n  Note: STUB mode — NPU numbers are bridge overhead only.")


if __name__ == "__main__":
    main()
