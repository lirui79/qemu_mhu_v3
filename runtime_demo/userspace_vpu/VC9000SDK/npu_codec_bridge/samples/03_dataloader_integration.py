#!/usr/bin/env python3
"""
03_dataloader_integration.py — NPU-Accelerated Video Decoding in a PyTorch DataLoader.

This is the practical pattern that AI/ML training pipelines will use.
We implement a standard `torch.utils.data.IterableDataset` that wraps the
NPU hardware decoder, making it a drop-in replacement in any existing
training loop that uses PyTorch DataLoaders.

┌─────────────────────────────────────────────────────────────────┐
│  ARCHITECTURE:                                                   │
│                                                                  │
│  Video File(s)                                                   │
│      │                                                           │
│      ▼                                                           │
│  NPUDecoderVideoDataset (IterableDataset)                        │
│      │  - Wraps VideoDecoder(device='npu')                        │
│      │  - Each __iter__ yields (frame_tensor, metadata_dict)     │
│      │  - Supports start/stop frame ranges, stride, shuffling    │
│      ▼                                                           │
│  torch.utils.data.DataLoader                                     │
│      │  - num_workers=0 (NPU device is process-exclusive)        │
│      │  - batch_size, collate_fn, pin_memory                     │
│      ▼                                                           │
│  Training Loop                                                   │
│      for batch in dataloader:                                    │
│          output = model(batch.to(device))                        │
│          loss.backward()                                         │
│                                                                  │
│  IMPORTANT: num_workers MUST be 0 when using the NPU decoder     │
│  because /dev/vastai_video* is a hardware device that cannot     │
│  be shared across processes. For multi-GPU/multi-NPU setups,     │
│  create one decoder per process using worker_init_fn.            │
└─────────────────────────────────────────────────────────────────┘

Usage:
    python 03_dataloader_integration.py [path/to/video.mp4]

    # Realistic training-pipeline usage:
    python 03_dataloader_integration.py /data/videos/train_clip.mp4
"""

import argparse
import math
import os
import sys
import time
from typing import Iterator, Optional, Dict, List, Union

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
import torch.utils.data


# =========================================================================
# IterableDataset: NPU-Accelerated Video Decoder
# =========================================================================

class NPUDecoderVideoDataset(torch.utils.data.IterableDataset):
    """
    An IterableDataset that decodes video frames on-the-fly using the
    VastAI NPU hardware decoder.

    Yields (frame_tensor, metadata_dict) tuples where:
      - frame_tensor: torch.Tensor of shape [C, H, W], dtype uint8
      - metadata_dict: dict with keys ['frame_index', 'pts_seconds',
                        'duration_seconds', 'source_path']

    Parameters
    ----------
    video_path : str
        Path to a video file (.mp4, .h264, .hevc).
    start_frame : int
        First frame index to include (inclusive).
    end_frame : int or None
        Last frame index to include (exclusive). None = decode to end.
    stride : int
        Decode every `stride`-th frame (1 = every frame, 2 = every other).
    shuffle : bool
        If True, shuffle frame indices within [start_frame, end_frame).
        Note: This destroys temporal order. Use for training, not for
        tasks that require frame sequence.
    transforms : callable or None
        Optional transform applied to each frame tensor before yielding.
        Signature: transform(tensor) -> tensor.
    device : str
        Decoder device. 'npu' for hardware acceleration, 'cpu' for fallback.
    """

    def __init__(
        self,
        video_path: str,
        start_frame: int = 0,
        end_frame: Optional[int] = None,
        stride: int = 1,
        shuffle: bool = False,
        transforms: Optional[callable] = None,
        device: str = "npu",
    ):
        super().__init__()
        self.video_path = video_path
        self.start_frame = start_frame
        self.end_frame = end_frame
        self.stride = max(1, stride)
        self.shuffle = shuffle
        self.transforms = transforms
        self.device = device

        # ── Create the NPU decoder ONCE (not in __iter__) ─────
        # The decoder is created in the main process. Each worker in
        # a DataLoader with num_workers>0 would re-create it via
        # worker_init_fn.  For NPU, we recommend num_workers=0.
        self._decoder = VideoDecoder(video_path, device=device)

        # Derive frame range
        total = len(self._decoder)
        self._total_frames = total
        if self.end_frame is None:
            self._end_actual = total
        else:
            self._end_actual = min(self.end_frame, total)

        # Build index list
        self._indices = list(
            range(self.start_frame, self._end_actual, self.stride)
        )

    # ---- Properties (readable from training loop) ----

    @property
    def total_frames(self) -> int:
        """Total frames in the source video."""
        return self._total_frames

    @property
    def num_samples(self) -> int:
        """Number of frames this dataset will yield."""
        return len(self._indices)

    @property
    def metadata(self):
        """Access the underlying decoder's VideoStreamMetadata."""
        return self._decoder.metadata

    # ---- Core iteration ----

    def __iter__(self) -> Iterator:
        """
        Yield (frame_tensor, metadata_dict) tuples.

        In a DataLoader with batch_size > 1, the framework will
        collate these into batches automatically.
        """
        indices = self._indices.copy()
        if self.shuffle:
            import random
            random.shuffle(indices)

        worker_info = torch.utils.data.get_worker_info()
        worker_id = worker_info.id if worker_info is not None else 0

        for idx in indices:
            # ── Decode one frame through the NPU ──────────────
            frame = self._decoder.get_frame_at(idx)
            tensor = frame.data  # [C, H, W] uint8

            # ── Apply transforms (normalisation, augmentation) ─
            if self.transforms is not None:
                tensor = self.transforms(tensor)

            yield tensor, {
                "frame_index": idx,
                "pts_seconds": frame.pts_seconds,
                "duration_seconds": frame.duration_seconds,
                "source_path": self.video_path,
                "worker_id": worker_id,
            }

    def __len__(self) -> int:
        return self.num_samples

    def __repr__(self) -> str:
        return (
            f"NPUDecoderVideoDataset("
            f"video={os.path.basename(self.video_path)!r}, "
            f"frames=[{self.start_frame}:{self._end_actual}:{self.stride}], "
            f"n={self.num_samples}, "
            f"device={self.device!r}, "
            f"shuffle={self.shuffle})"
        )


# =========================================================================
# Example transforms (for training pipelines)
# =========================================================================

class TrainingTransforms:
    """Example transform pipeline for video training.

    Converts uint8 [0,255] → float32 [0,1], optionally resizes,
    and normalises with ImageNet statistics.
    """

    def __init__(
        self,
        resize: Optional[tuple] = None,  # (H, W)
        mean: tuple = (0.485, 0.456, 0.406),
        std: tuple = (0.229, 0.224, 0.225),
    ):
        self.resize = resize
        self.mean = torch.tensor(mean, dtype=torch.float32).view(3, 1, 1)
        self.std = torch.tensor(std, dtype=torch.float32).view(3, 1, 1)

    def __call__(self, tensor: torch.Tensor) -> torch.Tensor:
        # uint8 [0,255] → float32 [0,1]
        x = tensor.float() / 255.0

        # Optional resize
        if self.resize is not None:
            x = torch.nn.functional.interpolate(
                x.unsqueeze(0),
                size=self.resize,
                mode='bilinear',
                align_corners=False,
            ).squeeze(0)

        # Normalise
        x = (x - self.mean) / self.std
        return x


# =========================================================================
# Collate function for DataLoader
# =========================================================================

def video_collate_fn(batch: List[tuple]) -> dict:
    """
    Custom collate for (tensor, metadata_dict) tuples.

    Stacks tensors into [B, C, H, W] and collects metadata into a dict
    of lists.  This preserves per-frame metadata alongside the batch tensor.
    """
    tensors = [item[0] for item in batch]
    metas = [item[1] for item in batch]

    # Stack frame tensors (all must have same shape)
    batched_tensors = torch.stack(tensors, dim=0)

    # Collect metadata
    batched_meta = {
        "frame_index": [m["frame_index"] for m in metas],
        "pts_seconds": [m["pts_seconds"] for m in metas],
        "duration_seconds": [m["duration_seconds"] for m in metas],
        "source_path": metas[0]["source_path"],
    }

    return {"data": batched_tensors, "meta": batched_meta}


# =========================================================================
# Main — demonstrates the full pipeline
# =========================================================================

def main():
    parser = argparse.ArgumentParser(
        description="NPU-Accelerated Video DataLoader Demo"
    )
    parser.add_argument(
        "video", nargs="?", default=None,
        help="Path to video file. Defaults to repo's fixed_hevc.mp4."
    )
    parser.add_argument(
        "--batch-size", type=int, default=4,
        help="Batch size for DataLoader (default: 4)"
    )
    parser.add_argument(
        "--num-frames", type=int, default=32,
        help="Number of frames to decode (default: 32)"
    )
    parser.add_argument(
        "--stride", type=int, default=1,
        help="Frame stride: decode every Nth frame (default: 1)"
    )
    parser.add_argument(
        "--shuffle", action="store_true",
        help="Shuffle frame order (destroys temporal order)"
    )
    parser.add_argument(
        "--cpu", action="store_true",
        help="Use CPU decoder instead of NPU (for comparison)"
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

    device_str = "cpu" if args.cpu else "npu"
    npu_stub = _C.__mode__ == "stub"

    print(f"{'=' * 60}")
    print(f"  NPU-Accelerated Video DataLoader Demo")
    print(f"{'=' * 60}")
    print(f"  Video:    {args.video}")
    print(f"  Mode:     {'STUB (bridge overhead only)' if npu_stub else 'LIVE (real NPU)'}")
    print(f"  Device:   {device_str}")
    print(f"  Batch:    {args.batch_size}")
    print(f"  Frames:   {args.num_frames}  (stride {args.stride})")
    print()

    # ==================================================================
    # Step 1: Create the dataset
    # ==================================================================
    print("[1] Creating NPUDecoderVideoDataset...")

    # Optional: add training transforms
    transforms = TrainingTransforms(
        resize=(224, 224),  # ResNet input size
    )

    dataset = NPUDecoderVideoDataset(
        video_path=args.video,
        start_frame=0,
        end_frame=args.num_frames,
        stride=args.stride,
        shuffle=args.shuffle,
        transforms=transforms,  # comment this out for raw uint8 frames
        device=device_str,
    )

    print(f"    {dataset}")
    meta = dataset.metadata
    print(f"    Source: {meta.width}x{meta.height}, "
          f"{meta.average_fps:.1f}fps, {meta.codec}")

    # ==================================================================
    # Step 2: Create DataLoader
    # ==================================================================
    print(f"\n[2] Creating DataLoader (batch_size={args.batch_size})...")

    dataloader = torch.utils.data.DataLoader(
        dataset,
        batch_size=args.batch_size,
        shuffle=False,          # dataset handles shuffling internally
        num_workers=0,          # ★ MUST be 0 for NPU ★
        collate_fn=video_collate_fn,
        pin_memory=False,       # NPU memory is already device-accessible
    )

    # ==================================================================
    # Step 3: Iterate (simulates a training epoch)
    # ==================================================================
    print(f"\n[3] Iterating through DataLoader (simulating training epoch)...")
    print()

    batch_count = 0
    total_frames = 0
    t_start = time.perf_counter()

    for batch in dataloader:
        data = batch["data"]      # [B, C, H, W] float32 (after transforms)
        meta = batch["meta"]

        batch_count += 1
        total_frames += data.shape[0]

        print(f"    Batch {batch_count:3d}: "
              f"data={list(data.shape)}, "
              f"range={data.min():.3f}..{data.max():.3f}, "
              f"indices={meta['frame_index']}")

    t_end = time.perf_counter()
    elapsed = t_end - t_start

    # ==================================================================
    # Summary
    # ==================================================================
    print(f"\n{'=' * 60}")
    print(f"  Pipeline Summary")
    print(f"{'=' * 60}")
    print(f"  Batches        : {batch_count}")
    print(f"  Total frames   : {total_frames}")
    print(f"  Elapsed        : {elapsed:.3f}s")
    print(f"  Throughput     : {total_frames / elapsed:.1f} fps" if elapsed > 0 else "")
    print(f"  Avg/batch      : {1000 * elapsed / max(batch_count, 1):.1f} ms")

    print(f"""
  ✅ NPU decoder successfully integrated into PyTorch DataLoader
  ✅ Drop-in replacement for any existing IterableDataset pipeline
  ✅ num_workers=0 is required for NPU (hardware device is exclusive)

  To use in your own training loop, copy the NPUDecoderVideoDataset
  class and replace your current video dataset. The ONLY changes:

      import npu_codec_bridge
      from torchcodec.decoders import VideoDecoder

      dataset = NPUDecoderVideoDataset(
          "training_clip.mp4", device="npu", transforms=my_transforms
      )
      dataloader = DataLoader(dataset, batch_size=32, num_workers=0)
""")


if __name__ == "__main__":
    main()
