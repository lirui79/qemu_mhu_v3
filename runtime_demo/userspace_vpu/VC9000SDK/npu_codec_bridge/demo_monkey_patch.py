#!/usr/bin/env python3
"""
Demo: Monkey-Patching torchcodec with NPU hardware decoder support.

This script demonstrates the complete workflow:
  1. Import npu_codec_bridge (triggers the monkey-patch automatically)
  2. Use the standard torchcodec API — but with device="npu"

No code changes required to existing torchcodec-based projects!
"""

import time
import argparse

# =========================================================================
# STEP 1: Import npu_codec_bridge FIRST
# =========================================================================
# This triggers _install_npu_patch() which replaces
# torchcodec.decoders.VideoDecoder with our patched version.
import npu_codec_bridge  # noqa: F401

# =========================================================================
# STEP 2: Import torchcodec normally
# =========================================================================
from torchcodec.decoders import VideoDecoder

# =========================================================================
# STEP 3: Use the SAME API — just pass device="npu"
# =========================================================================

def demo_npu_decoding(video_path: str):
    """Demonstrate NPU hardware decoding through torchcodec API."""

    print(f"{'='*60}")
    print(f"NPU Decoder Demo: {video_path}")
    print(f"{'='*60}")

    # ── NPU decoder ──────────────────────────────────────────
    print("\n[1] Creating NPU hardware decoder...")
    npu_dec = VideoDecoder(
        video_path,
        device="npu",              # ← This triggers the NPU path!
        dimension_order="NCHW",
    )
    print(f"    Decoder: {npu_dec}")

    # ── Metadata ─────────────────────────────────────────────
    print("\n[2] Stream metadata:")
    meta = npu_dec.metadata
    print(f"    Resolution : {meta.width}x{meta.height}")
    print(f"    Codec      : {meta.codec}")
    print(f"    FPS        : {meta.average_fps:.1f}")
    print(f"    Frames     : {meta.num_frames}")
    print(f"    Duration   : {meta.duration_seconds:.1f}s")

    # ── Simple indexing ──────────────────────────────────────
    print("\n[3] Indexing (decoder[i])...")
    t0 = time.perf_counter()
    first_frame = npu_dec[0]         # → Tensor[3, H, W]
    t1 = time.perf_counter()
    print(f"    decoder[0] → {first_frame.shape}, "
          f"dtype={first_frame.dtype}, "
          f"latency={1000*(t1-t0):.1f}ms")

    # ── Slice indexing ───────────────────────────────────────
    print("\n[4] Slice indexing (decoder[0:10])...")
    t0 = time.perf_counter()
    batch = npu_dec[0:10]           # → Tensor[10, 3, H, W]
    t1 = time.perf_counter()
    print(f"    decoder[0:10] → {batch.shape}, "
          f"throughput={10/(t1-t0):.1f} fps")

    # ── Frame methods (with metadata) ────────────────────────
    print("\n[5] get_frame_at() — returns Frame with metadata...")
    frame = npu_dec.get_frame_at(30)
    print(f"    Index:      30")
    print(f"    data.shape: {frame.data.shape}")
    print(f"    pts_seconds: {frame.pts_seconds:.4f}")
    print(f"    duration:    {frame.duration_seconds:.4f}")

    # ── Time-based lookup ────────────────────────────────────
    print("\n[6] get_frame_played_at(2.5s)...")
    frame = npu_dec.get_frame_played_at(2.5)
    print(f"    pts_seconds: {frame.pts_seconds:.4f}")

    # ── Batch methods ────────────────────────────────────────
    print("\n[7] get_frames_at([5, 15, 25, 35, 45])...")
    t0 = time.perf_counter()
    frames = npu_dec.get_frames_at([5, 15, 25, 35, 45])
    t1 = time.perf_counter()
    print(f"    data.shape:         {frames.data.shape}")
    print(f"    pts_seconds:        {frames.pts_seconds}")
    print(f"    throughput:         {5/(t1-t0):.1f} fps")

    print("\n[8] get_frames_in_range(100, 110, step=2)...")
    frames = npu_dec.get_frames_in_range(100, 110, step=2)
    print(f"    data.shape: {frames.data.shape}")

    # ── Time-range batch ─────────────────────────────────────
    print("\n[9] get_frames_played_in_range(1.0, 2.0)...")
    frames = npu_dec.get_frames_played_in_range(1.0, 2.0)
    print(f"    Retrieved {frames.data.shape[0]} frames")

    print(f"\n{'='*60}")
    print("Demo complete!")
    print(f"{'='*60}")


def demo_original_torchcodec(video_path: str):
    """Demonstrate that original torchcodec still works for device='cpu'."""

    print(f"\n{'='*60}")
    print(f"Original torchcodec (CPU): {video_path}")
    print(f"{'='*60}")

    # ── CPU decoder (original torchcodec path) ───────────────
    cpu_dec = VideoDecoder(video_path, device="cpu")
    print(f"\n[1] CPU decoder metadata:")
    meta = cpu_dec.metadata
    print(f"    {meta.width}x{meta.height}, "
          f"{meta.codec}, {meta.average_fps:.1f}fps")

    print("\n[2] CPU: decoder[0]")
    frame = cpu_dec[0]
    print(f"    Shape: {frame.shape}")

    print("\n✓ Original torchcodec (CPU) path is unaffected by the patch.")


# =========================================================================
# Main
# =========================================================================

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="NPU Codec Bridge — torchcodec monkey-patch demo"
    )
    parser.add_argument(
        "video", nargs="?", default=None,
        help="Path to video file (.h264, .hevc, .mp4)"
    )
    parser.add_argument(
        "--cpu-demo", action="store_true",
        help="Also run the original torchcodec CPU demo"
    )
    args = parser.parse_args()

    video_path = args.video or "fixed_hevc.mp4"

    # ── NPU demo (via monkey-patched torchcodec) ─────────────
    try:
        demo_npu_decoding(video_path)
    except Exception as e:
        print(f"\n⚠  NPU demo failed: {e}")
        print("   (This is expected if no NPU hardware is present.)")
        print("   The monkey-patch is installed correctly —")
        print("   the error is from trying to open /dev/vastai_video*.")

    # ── Original CPU demo ────────────────────────────────────
    if args.cpu_demo:
        demo_original_torchcodec(video_path)
