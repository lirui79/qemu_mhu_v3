#!/usr/bin/env python3
"""
01_basic_usage.py — "Magic" one-line NPU hardware decoder integration.

This is the simplest possible script demonstrating that, after a single
`import npu_codec_bridge`, the standard torchcodec API accepts `device='npu'`
and routes decoding to the VastAI NPU.

┌─────────────────────────────────────────────────────────────────┐
│  KEY INSIGHT: Zero modifications to torchcodec are required.   │
│  The ONLY change in user code is:                               │
│                                                                 │
│     import npu_codec_bridge          # ← add this line          │
│     from torchcodec.decoders import VideoDecoder                │
│     dec = VideoDecoder("video.mp4", device="npu")  # ← 'npu'   │
│                                                                 │
│  Everything else — get_frame_at, metadata, indexing — is the   │
│  standard torchcodec API you already know.                      │
└─────────────────────────────────────────────────────────────────┘

Usage:
    python 01_basic_usage.py [path/to/video.h264]

Requirements:
    pip install torchcodec Pillow
    # The npu_codec_bridge C++ extension must be compiled:
    #   cd npu_codec_bridge && pip install -e .
"""

import argparse
import os
import sys


# =========================================================================
# STEP 1: Import npu_codec_bridge FIRST
# =========================================================================
# This single import triggers the monkey-patch that intercepts
# torchcodec.decoders.VideoDecoder(..., device='npu').
#
# No other changes are needed — your existing torchcodec code works
# unchanged; simply add this import and pass device='npu'.
# =========================================================================
try:
    import npu_codec_bridge  # noqa: F401   ← MAGIC HAPPENS HERE
except ImportError:
    print("ERROR: npu_codec_bridge not installed.", file=sys.stderr)
    print("Run: cd npu_codec_bridge && pip install -e .", file=sys.stderr)
    sys.exit(1)


# =========================================================================
# STEP 2: Import torchcodec normally
# =========================================================================
from torchcodec.decoders import VideoDecoder

# PIL for saving frames as images
try:
    from PIL import Image
    HAS_PIL = True
except ImportError:
    HAS_PIL = False
    print("WARNING: Pillow not installed. Install with: pip install Pillow")
    print("         Frame images will not be saved.\n")


def print_separator(title: str):
    print(f"\n{'─' * 60}")
    print(f"  {title}")
    print(f"{'─' * 60}")


def save_frame_as_image(tensor, path: str):
    """Save a [C, H, W] uint8 RGB tensor as a PNG image."""
    if not HAS_PIL:
        # Fallback: save as raw .pt file
        import torch
        pt_path = path.replace('.png', '.pt')
        torch.save(tensor, pt_path)
        print(f"  (PIL not available — saved tensor to {pt_path})")
        return

    # CHW → HWC for PIL
    arr = tensor.permute(1, 2, 0).cpu().numpy()
    img = Image.fromarray(arr, mode="RGB")
    img.save(path, "PNG")
    print(f"  Saved: {path}  ({os.path.getsize(path):,} bytes)")


def main():
    parser = argparse.ArgumentParser(
        description="NPU Codec Bridge — Basic Usage Demo"
    )
    parser.add_argument(
        "video", nargs="?", default=None,
        help="Path to video file (.mp4, .h264, .hevc). "
             "Defaults to the repo's fixed_hevc.mp4."
    )
    parser.add_argument(
        "--frame", type=int, default=0,
        help="Frame index to decode and save (default: 0)"
    )
    parser.add_argument(
        "--output", type=str, default="frame_npu.png",
        help="Output image path (default: frame_npu.png)"
    )
    args = parser.parse_args()

    # Fallback to repo test video if no argument given
    if args.video is None:
        repo_root = os.path.abspath(
            os.path.join(os.path.dirname(__file__), "..", "..")
        )
        args.video = os.path.join(repo_root, "fixed_hevc.mp4")
        if not os.path.exists(args.video):
            print("ERROR: No video file provided and default not found.")
            print("Usage: python 01_basic_usage.py /path/to/video.mp4")
            sys.exit(1)

    # Detect build mode
    from npu_codec_bridge import _C
    is_stub = _C.__mode__ == "stub"

    print(f"Video: {args.video}")
    print(f"Frame index: {args.frame}")
    if is_stub:
        print()
        print("  ╔═══════════════════════════════════════════════════════════╗")
        print("  ║  ⚠  STUB MODE — 当前运行在 STUB 模式（Mock 解码器）     ║")
        print("  ║                                                         ║")
        print("  ║  STUB 解码器返回随机生成的 NV12 帧数据，经 RGB 转换后   ║")
        print("  ║  会显示为彩色雪花图像。这是符合预期的行为。              ║")
        print("  ║                                                         ║")
        print("  ║  要获得真实视频帧，需要使用 NPU 硬件并切换到 LIVE 模式：  ║")
        print("  ║    NPU_LIVE_MODE=1 pip install --no-build-isolation -e .║")
        print("  ╚═══════════════════════════════════════════════════════════╝")

    # ==================================================================
    # NPU DECODER
    # ==================================================================
    print_separator("NPU Hardware Decoder (device='npu')")

    # ── This is the ONLY change vs standard torchcodec usage ─────
    npu_dec = VideoDecoder(args.video, device="npu")
    # ─────────────────────────────────────────────────────────────

    # ---- Metadata ----
    meta = npu_dec.metadata
    print(f"  Codec      : {meta.codec}")
    print(f"  Resolution : {meta.width} x {meta.height}")
    print(f"  FPS        : {meta.average_fps:.2f}")
    print(f"  Duration   : {meta.duration_seconds:.2f}s")
    print(f"  Frames     : {meta.num_frames}")

    # ---- Decode a single frame ----
    idx = min(args.frame, len(npu_dec) - 1)
    print(f"\n  Decoding frame {idx}...")
    frame = npu_dec.get_frame_at(idx)

    print(f"  Frame data:")
    print(f"    shape          : {list(frame.data.shape)}  (C, H, W)")
    print(f"    dtype          : {frame.data.dtype}")
    print(f"    device         : {frame.data.device}")
    print(f"    pts_seconds    : {frame.pts_seconds:.4f}")
    print(f"    duration_seconds: {frame.duration_seconds:.4f}")

    # ---- Pixel statistics ----
    import torch
    data = frame.data.float()
    print(f"    pixel min/mean/max: "
          f"{data.min():.0f} / {data.mean():.1f} / {data.max():.0f}")

    # ---- Save as image ----
    save_frame_as_image(frame.data, args.output)

    # ==================================================================
    # CPU DECODER (for comparison)
    # ==================================================================
    print_separator("CPU Decoder (device='cpu') — for comparison")

    try:
        cpu_dec = VideoDecoder(args.video, device="cpu")
        cpu_meta = cpu_dec.metadata
        print(f"  Codec      : {cpu_meta.codec}")
        print(f"  Resolution : {cpu_meta.width} x {cpu_meta.height}")
        print(f"  FPS        : {cpu_meta.average_fps:.2f}")

        cpu_frame = cpu_dec.get_frame_at(idx)
        print(f"  Frame {idx}: shape {list(cpu_frame.data.shape)}, "
              f"dtype {cpu_frame.data.dtype}")

        # Save CPU frame for side-by-side comparison
        save_frame_as_image(cpu_frame.data, "frame_cpu.png")
    except RuntimeError as e:
        print(f"  CPU decoder unavailable: {e}")

    # ==================================================================
    # Summary
    # ==================================================================
    print_separator("Summary")
    print(f"""
  ✅ NPU decoder works through standard torchcodec API
  ✅ Only change: add `import npu_codec_bridge` at the top
  ✅ Then pass `device='npu'` to VideoDecoder
  ✅ All other torchcodec API methods work unchanged

  To integrate into your own project, simply add these two lines
  BEFORE any torchcodec import:

      import npu_codec_bridge
      from torchcodec.decoders import VideoDecoder

      dec = VideoDecoder("your_video.mp4", device="npu")
""")


if __name__ == "__main__":
    main()
