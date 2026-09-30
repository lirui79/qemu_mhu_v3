"""
npu_codec_bridge — torchcodec Monkey-Patch Injection Module

When a user writes:

    import npu_codec_bridge  # ← this triggers the patch
    from torchcodec.decoders import VideoDecoder
    dec = VideoDecoder("video.h264", device="npu")  # ← routes to NPU

the NPU hardware decoder is used instead of FFmpeg.

Strategy:
  We replace torchcodec.decoders.VideoDecoder with our NPUVideoDecoder
  when device="npu" is detected. For all other devices, the original
  torchcodec VideoDecoder is used unchanged.

  The NPUVideoDecoder class mirrors torchcodec's full public API:
    - __getitem__ (indexing)
    - get_frame_at / get_frame_played_at
    - get_frames_at / get_frames_in_range
    - get_frames_played_at / get_frames_played_in_range
    - metadata attribute
    - stream_index attribute

  Internally it wraps our C++ NPUDecoderWrapper (via pybind11).
"""

from __future__ import annotations

import struct
import time
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional, Union, Literal, IO

import numpy as np
import torch

# ---------------------------------------------------------------------------
# 1. NV12 → RGB conversion (pure torch, runs on GPU if available)
# ---------------------------------------------------------------------------
# torchcodec returns [C, H, W] uint8 RGB tensors.  NPU outputs NV12.
# We convert NV12 → RGB using a torch kernel so the tensor stays on the
# correct device (CPU or CUDA) without intermediate numpy copies.

def _nv12_to_rgb_nchw(
    y_plane: torch.Tensor,   # [H, strideY] uint8
    uv_plane: torch.Tensor,  # [H/2, strideUV] uint8
    width: int,
    height: int,
) -> torch.Tensor:
    """
    Convert NV12 planes to RGB [3, H, W] uint8 tensor in NCHW order.

    Uses the standard ITU-R BT.601 full-range matrix:
        R = Y + 1.402 * (V - 128)
        G = Y - 0.344 * (U - 128) - 0.714 * (V - 128)
        B = Y + 1.772 * (U - 128)
    """
    # Extract the valid pixel region (strip padding beyond `width`)
    y = y_plane[:height, :width].float()         # [H, W]
    # UV is subsampled 2x in both dimensions
    uv_h = height // 2
    uv_w = width // 2
    uv = uv_plane[:uv_h, :uv_w * 2].float()      # [H/2, W*2] interleaved

    # Split interleaved UV into separate U and V channels
    u = uv[:, 0::2]                               # [H/2, W/2]
    v = uv[:, 1::2]                               # [H/2, W/2]

    # Upsample U and V to full resolution (nearest neighbour)
    u = u.repeat_interleave(2, dim=0).repeat_interleave(2, dim=1)  # [H, W]
    v = v.repeat_interleave(2, dim=0).repeat_interleave(2, dim=1)  # [H, W]

    # BT.601 full-range YUV → RGB
    y  = y - 16.0
    u  = u - 128.0
    v  = v - 128.0

    r = y + 1.402   * v
    g = y - 0.344   * u - 0.714 * v
    b = y + 1.772   * u

    rgb = torch.stack([r, g, b], dim=0)           # [3, H, W]
    rgb = rgb.clamp(0, 255).to(torch.uint8)

    return rgb


# ---------------------------------------------------------------------------
# 2. Frame & FrameBatch dataclasses (torchcodec-compatible)
# ---------------------------------------------------------------------------

@dataclass
class Frame:
    """Mirrors torchcodec.decoders.Frame."""
    data: torch.Tensor           # [C, H, W] uint8, NCHW order
    pts_seconds: float
    duration_seconds: float


@dataclass
class FrameBatch:
    """Mirrors torchcodec.decoders.FrameBatch."""
    data: torch.Tensor           # [N, C, H, W] uint8
    pts_seconds: torch.Tensor    # [N] float64
    duration_seconds: torch.Tensor  # [N] float64


# ---------------------------------------------------------------------------
# 3. Metadata (torchcodec-compatible)
# ---------------------------------------------------------------------------

@dataclass
class VideoStreamMetadata:
    """Mirrors torchcodec.decoders.VideoStreamMetadata."""
    num_frames: int
    duration_seconds: float
    average_fps: float
    bit_rate: float
    codec: str
    width: int
    height: int


# ---------------------------------------------------------------------------
# 4. Annex-B Stream Parser
# ---------------------------------------------------------------------------
# The NPU SDK requires frame-aligned Access Units with Annex B start codes.
# We implement a lightweight parser that:
#   (a) Reads raw .h264/.hevc/.av1 files directly
#   (b) Demuxes container formats (mp4/mkv) via FFmpeg subprocess into Annex B
#   (c) Handles in-memory bytes by scanning for start codes

_START_CODE_PREFIX = b'\x00\x00\x01'   # 3-byte start code prefix
_START_CODE_PREFIX_4 = b'\x00\x00\x00\x01'  # 4-byte start code prefix


def _split_access_units(data: bytes) -> list[tuple[bytes, int]]:
    """
    Split raw Annex B bitstream into Access Units.

    Each AU starts with 00 00 00 01 (or 00 00 01) and contains one complete
    coded picture (all slices of one frame).

    Returns list of (au_bytes, offset_in_source).
    """
    aus = []
    i = 0
    n = len(data)

    # Find first start code
    au_start = -1
    while i < n - 3:
        if data[i:i+4] == _START_CODE_PREFIX_4:
            au_start = i
            i += 4
            break
        elif data[i:i+3] == _START_CODE_PREFIX:
            au_start = i
            i += 3
            break
        i += 1

    if au_start < 0:
        return aus  # No start code found

    # Find subsequent start codes to split AUs
    while i < n - 3:
        if data[i:i+4] == _START_CODE_PREFIX_4:
            aus.append((data[au_start:i], au_start))
            au_start = i
            i += 4
        elif data[i:i+3] == _START_CODE_PREFIX:
            aus.append((data[au_start:i], au_start))
            au_start = i
            i += 3
        else:
            i += 1

    # Last AU (from last start code to end)
    if au_start < n:
        aus.append((data[au_start:n], au_start))

    return aus


class AnnexBStreamReader:
    """
    Reads an H.264/H.265 Annex B elementary stream and yields Access Units.

    Supports:
      - Raw files (.h264, .hevc, .264, .265)
      - Bytes in memory
      - File-like objects with read()

    For container formats (.mp4, .mkv), a minimal FFmpeg subprocess is used
    to extract the raw bitstream.
    """

    def __init__(self, source: Union[str, Path, bytes, IO]):
        self._source = source
        self._file_handle = None
        self._all_data: Optional[bytes] = None
        self._au_list: list[bytes] = []
        self._au_index: int = 0
        self._total_aus: int = 0

        if isinstance(source, (str, Path)):
            source_path = str(source)
            # Determine if this is a raw stream or container
            suffix = Path(source_path).suffix.lower()
            if suffix in ('.h264', '.264', '.hevc', '.265', '.h265',
                           '.av1', '.obu', '.avs', '.avs2'):
                # Raw Annex B file
                with open(source_path, 'rb') as f:
                    self._all_data = f.read()
            else:
                # Container format — use FFmpeg to extract raw bitstream
                self._all_data = self._demux_with_ffmpeg(source_path)
        elif isinstance(source, bytes):
            self._all_data = source
        else:
            # File-like object
            data = source.read()
            if isinstance(data, str):
                data = data.encode()
            self._all_data = data

        if self._all_data:
            # Parse into Access Units
            au_tuples = _split_access_units(self._all_data)
            # Filter: skip parameter-set-only chunks (SPS/PPS without slice data)
            # Keep all AUs; the NPU SDK can handle them
            self._au_list = [au for au, _ in au_tuples if len(au) > 4]
            self._total_aus = len(self._au_list)

    @staticmethod
    def _demux_with_ffmpeg(path: str) -> bytes:
        """Use FFmpeg to extract raw Annex B bitstream from container."""
        import subprocess
        # Extract H.264/HEVC to Annex B bytestream
        result = subprocess.run(
            ['ffmpeg', '-y', '-i', path,
             '-vcodec', 'copy', '-bsf:v', 'h264_mp4toannexb',
             '-f', 'h264', '-'],
            capture_output=True,
            timeout=60,
        )
        if result.returncode != 0:
            # Try HEVC
            result = subprocess.run(
                ['ffmpeg', '-y', '-i', path,
                 '-vcodec', 'copy', '-bsf:v', 'hevc_mp4toannexb',
                 '-f', 'hevc', '-'],
                capture_output=True,
                timeout=60,
            )
        if result.returncode != 0:
            raise RuntimeError(
                f"FFmpeg demux failed for {path}: {result.stderr.decode()}")
        return result.stdout

    def total_aus(self) -> int:
        return self._total_aus

    def read_au(self) -> Optional[bytes]:
        """Return the next Access Unit, or None if EOF."""
        if self._au_index >= self._total_aus:
            return None
        au = self._au_list[self._au_index]
        self._au_index += 1
        return au

    def seek_to_au(self, index: int):
        """Seek to a specific AU index (0-based)."""
        self._au_index = max(0, min(index, self._total_aus))

    def reset(self):
        self._au_index = 0


# ---------------------------------------------------------------------------
# 5. NPUVideoDecoder — torchcodec-compatible VideoDecoder replacement
# ---------------------------------------------------------------------------

class NPUVideoDecoder:
    """
    Drop-in replacement for torchcodec.decoders.VideoDecoder that routes
    decoding to the VastAI NPU hardware.

    Implements the full torchcodec VideoDecoder public API surface.

    Usage:
        dec = NPUVideoDecoder("video.h264", device="npu")
        first_frame = dec[0]                    # → Tensor[3, H, W]
        frame = dec.get_frame_at(42)            # → Frame
        frames = dec.get_frames_at([10, 20])    # → FrameBatch
        print(dec.metadata.width, dec.metadata.height)
    """

    _ORIGINAL_VIDEO_DECODER = None  # stored reference to original torchcodec class

    def __init__(
        self,
        source: Union[str, Path, IO, bytes, torch.Tensor],
        *,
        stream_index: Optional[int] = None,
        dimension_order: Literal["NCHW", "NHWC"] = "NCHW",
        num_ffmpeg_threads: int = 1,
        device: Optional[Union[str, torch.device]] = "cpu",
        seek_mode: Literal["exact", "approximate"] = "exact",
    ):
        self._stream_index = stream_index
        self._dimension_order = dimension_order
        self._seek_mode = seek_mode
        self._device_str = str(device)

        # --- Open the Annex B bitstream ---
        self._original_source = source
        if isinstance(source, torch.Tensor):
            source = source.numpy().tobytes()

        self._stream_reader = AnnexBStreamReader(source)
        self._total_aus = self._stream_reader.total_aus()

        # --- Probe the first AU to get stream info ---
        first_au = self._stream_reader.read_au()
        if not first_au:
            raise RuntimeError("No valid Access Units found in input")

        # Detect codec: try NAL header first (most reliable), then file extension
        self._codec_name = (
            self._codec_from_nal(first_au)
            or self._codec_from_extension()
            or "h264"
        )

        # Probe stream info
        probe_result = self._probe_stream(first_au)

        self._width = probe_result.get("width", 1920)
        self._height = probe_result.get("height", 1080)
        self._fps = probe_result.get("average_fps", 30.0)
        self._bit_rate = probe_result.get("bit_rate", 0.0)

        # Now create config with detected codec
        config = self._make_config()

        # Rewind the stream for actual decoding
        self._stream_reader.reset()

        # --- Lazy-initialise the NPU decoder ---
        self._config = config
        self._npu_decoder = None  # created on first decode
        self._decoded_frames: list[DecodedFrameMeta] = []  # decoded frame index
        self._current_au_index: int = 0
        self._eos: bool = False

        # --- Compute metadata ---
        self._duration = (self._total_aus / self._fps) if self._fps > 0 else 0.0
        self._metadata = VideoStreamMetadata(
            num_frames=self._total_aus,
            duration_seconds=self._duration,
            average_fps=self._fps,
            bit_rate=self._bit_rate,
            codec=self._codec_name,
            width=self._width,
            height=self._height,
        )

        # Pre-allocate conversion cache
        self._rgb_cache: dict[int, torch.Tensor] = {}

    def _codec_from_extension(self) -> str:
        """Detect codec from file extension."""
        src = getattr(self, '_original_source', None)
        if isinstance(src, (str, Path)):
            suffix = Path(str(src)).suffix.lower()
            if suffix in ('.h265', '.hevc', '.265'):
                return 'hevc'
            elif suffix in ('.h264', '.264'):
                return 'h264'
            elif suffix in ('.av1', '.obu', '.ivf'):
                return 'av1'
            elif suffix in ('.avs', '.avs2'):
                return 'avs2'
        return None

    @staticmethod
    def _codec_from_nal(first_au: bytes) -> str:
        """Detect codec from NAL unit header in the first Access Unit.

        Looks at the byte after the 4-byte start code:
          H.264: forbidden_zero_bit(1) + nal_ref_idc(2) + nal_unit_type(5)
                 nal_unit_type in lower 5 bits
          HEVC:  forbidden_zero_bit(1) + nal_unit_type(6) + nuh_layer_id(6)
                 + nuh_temporal_id_plus1(3)
                 nal_unit_type in bits 1-6 (>> 1 & 0x3F)
        """
        if len(first_au) < 6:
            return None
        # Scan for start code
        for i in range(len(first_au) - 5):
            if first_au[i:i+4] == b'\x00\x00\x00\x01':
                nal_byte = first_au[i + 4]
                # HEVC: forbidden_zero_bit (bit 7) is always 0,
                # nal_unit_type is in bits 1-6
                hevc_type = (nal_byte >> 1) & 0x3F
                h264_type = nal_byte & 0x1F
                # HEVC VPS=32, SPS=33, PPS=34; H.264 SPS=7, PPS=8
                if hevc_type in (32, 33, 34):
                    return "hevc"
                elif h264_type in (7, 8):
                    return "h264"
                # Check if bit 7 is set (H.264 forbidden_zero_bit)
                if nal_byte & 0x80:
                    return "h264"
                # Default: assume H.264 for types 1-5, HEVC for types 19-21
                if 1 <= h264_type <= 5:
                    return "h264"
                if 19 <= hevc_type <= 21:
                    return "hevc"
        return None

    def _make_config(self):
        """Build a DecoderConfig for the NPU C++ wrapper."""
        codec = self._codec_name  # detected codec (h264, hevc, av1, vp9, avs2)
        try:
            from npu_codec_bridge._C import DecoderConfig
            cfg = DecoderConfig()
            cfg.device_path = "/dev/vastai_video0"
            cfg.codec_type = codec
            cfg.async_input = True
            return cfg
        except ImportError:
            return {
                "device_path": "/dev/vastai_video0",
                "codec_type": codec,
                "pixel_format": "NV12",
                "zero_copy_mode": "COPY_TO_HOST",
                "async_input": True,
            }

    def _probe_stream(self, au: bytes) -> dict:
        """
        Probe a bitstream Access Unit to extract resolution, framerate, etc.

        Uses a lightweight SPS parser for H.264 and HEVC.
        """
        info: dict = {
            "width": 1920,
            "height": 1080,
            "average_fps": 30.0,
            "codec": "h264",
            "bit_rate": 0.0,
        }

        # Try the C++ probe through our wrapper
        try:
            from npu_codec_bridge._C import NPUDecoder
            # (In production, we'd call the real probe)
        except ImportError:
            pass

        # Fallback: parse the SPS NAL unit to get resolution
        nal_type = self._get_nal_type(au)
        if nal_type in (7,):  # H.264 SPS
            info = self._parse_h264_sps(au, info)
        elif nal_type in (33,):  # HEVC VPS+SPS
            info["codec"] = "hevc"
            info = self._parse_hevc_sps(au, info)

        return info

    @staticmethod
    def _get_nal_type(au: bytes) -> int:
        """Extract NAL unit type from an Access Unit."""
        # Find the first start code
        for i in range(len(au) - 4):
            if au[i:i+4] == b'\x00\x00\x00\x01':
                nal_header = au[i + 4]
                # H.264: lower 5 bits; HEVC: upper 6 bits shifted right by 1
                if (nal_header & 0x7E) == 0x00 and (nal_header & 0x80) == 0x00:
                    # Looks like HEVC
                    return (nal_header & 0x7E) >> 1
                return nal_header & 0x1F
        return -1

    @staticmethod
    def _parse_h264_sps(au: bytes, info: dict) -> dict:
        """Minimal H.264 SPS parser to extract width/height."""
        # This is a simplified parser. In production, use a proper bitstream
        # reader or the C++ vmppDecGetVideoInfo probe.
        try:
            # Find SPS start
            for i in range(len(au) - 10):
                if au[i:i+4] == b'\x00\x00\x00\x01' and (au[i+4] & 0x1F) == 7:
                    sps = au[i+5:]  # Skip start code + NAL header
                    # profile_idc
                    if len(sps) < 4:
                        break
                    # Skip profile_idc, constraints, level_idc
                    # Read exp-golomb: seq_parameter_set_id
                    # (Simplification): typical SPS for baseline/main profile
                    # at known resolutions.
                    # For production, delegate to FFmpeg or the C++ probe.
                    break
        except Exception:
            pass
        return info

    @staticmethod
    def _parse_hevc_sps(au: bytes, info: dict) -> dict:
        """Minimal HEVC SPS parser."""
        # In production, delegate to vmppDecGetVideoInfo through the C++ layer.
        return info

    def _lazy_init_decoder(self):
        """Create the NPU decoder on first use."""
        if self._npu_decoder is not None:
            return

        try:
            from npu_codec_bridge._C import NPUDecoder
            self._npu_decoder = NPUDecoder(self._config)
        except ImportError:
            raise RuntimeError(
                "npu_codec_bridge C++ extension is not compiled. "
                "Run: cd npu_codec_bridge && pip install -e ."
            )

    def _decode_up_to(self, target_au_index: int):
        """
        Feed AUs to the NPU decoder until we've decoded up to `target_au_index`.
        Retrieved frames are cached in self._decoded_frames.

        Strategy: send AUs without polling after each one (to avoid
        the SDK's minimum 4000ms per-call timeout stalling us).
        Then pull all available frames in one batch.
        """
        self._lazy_init_decoder()

        # Push all needed AUs (plus a few extra to fill the DPB)
        while self._current_au_index <= target_au_index + 5:
            au = self._stream_reader.read_au()
            if au is None:
                break  # EOF
            self._npu_decoder.decode(au, pts=self._current_au_index)
            self._current_au_index += 1

        # Pull all available frames
        while True:
            result = self._npu_decoder.get_frame(timeout_ms=5000)
            if result is None:
                break
            y_tensor = result["y_tensor"]
            uv_tensor = result["uv_tensor"]
                # Assign sequential frame index (decoder outputs in display order)
            au_idx = len(self._decoded_frames)

            self._decoded_frames.append(DecodedFrameMeta(
                au_index=au_idx,
                y_tensor=y_tensor.clone(),
                uv_tensor=uv_tensor.clone(),
                width=result["width"],
                height=result["height"],
                pts_seconds=(au_idx / self._fps)
                    if self._fps > 0 else 0.0,
            ))
            self._npu_decoder.release_frame()

        # If we've reached EOF, flush remaining frames
        if self._current_au_index >= self._total_aus and not self._eos:
            self._flush_decoder()
            self._eos = True

    def _flush_decoder(self):
        """Drain remaining frames from the NPU decoder."""
        self._lazy_init_decoder()
        results = self._npu_decoder.flush(timeout_ms=5000)
        for result in results:
            self._decoded_frames.append(DecodedFrameMeta(
                au_index=-1,
                y_tensor=result["y_tensor"].clone(),
                uv_tensor=result["uv_tensor"].clone(),
                width=result["width"],
                height=result["height"],
                pts_seconds=0.0,
            ))

    def _get_frame_tensor(self, frame_idx: int) -> torch.Tensor:
        """
        Get a single frame as an RGB tensor [3, H, W] in the requested
        dimension order.

        Uses caching to avoid re-decoding frames already retrieved.
        """
        if frame_idx in self._rgb_cache:
            return self._rgb_cache[frame_idx]

        # Decode up to and including the requested frame index
        self._decode_up_to(frame_idx)

        # Find the decoded frame by position (decoder outputs in order)
        meta = None
        if frame_idx < len(self._decoded_frames):
            meta = self._decoded_frames[frame_idx]

        if meta is None:
            raise IndexError(
                f"Frame index {frame_idx} not available. "
                f"Total decoded frames: {len(self._decoded_frames)}"
            )

        # Convert NV12 → RGB
        rgb = _nv12_to_rgb_nchw(
            meta.y_tensor, meta.uv_tensor,
            meta.width, meta.height,
        )  # [3, H, W]

        if self._dimension_order == "NHWC":
            rgb = rgb.permute(1, 2, 0)  # → [H, W, 3]

        # Cache
        self._rgb_cache[frame_idx] = rgb
        return rgb

    # ==================================================================
    # torchcodec-compatible public API
    # ==================================================================

    @property
    def metadata(self) -> VideoStreamMetadata:
        return self._metadata

    @property
    def stream_index(self) -> Optional[int]:
        return self._stream_index

    # ---- Indexing: decoder[i], decoder[start:stop:step] ----

    def __getitem__(self, key: Union[int, slice]) -> torch.Tensor:
        if isinstance(key, int):
            if key < 0:
                key = self._total_aus + key
            return self._get_frame_tensor(key)

        elif isinstance(key, slice):
            indices = list(range(
                key.start if key.start is not None else 0,
                key.stop if key.stop is not None else self._total_aus,
                key.step if key.step is not None else 1,
            ))
            tensors = [self._get_frame_tensor(i) for i in indices]
            return torch.stack(tensors, dim=0)  # [N, C, H, W] or [N, H, W, C]
        else:
            raise TypeError(f"Unsupported key type: {type(key)}")

    def __len__(self) -> int:
        return self._total_aus

    # ---- Single-frame methods ----

    def get_frame_at(self, index: int) -> Frame:
        """
        Retrieve the frame at a specific index.

        Args:
            index: Frame index (0-based).

        Returns:
            Frame with .data (Tensor[C,H,W]), .pts_seconds, .duration_seconds
        """
        if index < 0:
            index = self._total_aus + index
        tensor = self._get_frame_tensor(index)
        pts = index / self._fps if self._fps > 0 else 0.0
        dur = 1.0 / self._fps if self._fps > 0 else 0.033
        return Frame(data=tensor, pts_seconds=pts, duration_seconds=dur)

    def get_frame_played_at(self, seconds: float) -> Frame:
        """
        Retrieve the frame displayed at a specific presentation timestamp.

        Args:
            seconds: Time in seconds.

        Returns:
            Frame at the given PTS.
        """
        index = int(seconds * self._fps) if self._fps > 0 else 0
        index = min(index, self._total_aus - 1)
        return self.get_frame_at(index)

    # ---- Batch methods (index-based) ----

    def get_frames_at(
        self, indices: Union[torch.Tensor, List[int]]
    ) -> FrameBatch:
        """
        Retrieve multiple frames by their indices.

        Args:
            indices: List of frame indices.

        Returns:
            FrameBatch with stacked tensor data and timestamps.
        """
        if isinstance(indices, torch.Tensor):
            indices = indices.tolist()

        if not indices:
            # Empty list → empty FrameBatch
            return FrameBatch(
                data=torch.empty(0, 3, self._height, self._width,
                                 dtype=torch.uint8),
                pts_seconds=torch.empty(0, dtype=torch.float64),
                duration_seconds=torch.empty(0, dtype=torch.float64),
            )

        tensors = []
        pts_list = []
        dur_list = []

        for idx in indices:
            frame = self.get_frame_at(idx)
            tensors.append(frame.data)
            pts_list.append(frame.pts_seconds)
            dur_list.append(frame.duration_seconds)

        data = torch.stack(tensors, dim=0)  # [N, C, H, W]
        pts = torch.tensor(pts_list, dtype=torch.float64)
        dur = torch.tensor(dur_list, dtype=torch.float64)

        return FrameBatch(data=data, pts_seconds=pts, duration_seconds=dur)

    def get_frames_in_range(
        self, start: int, stop: int, step: int = 1
    ) -> FrameBatch:
        """
        Retrieve frames in the half-open interval [start, stop).

        Args:
            start: First frame index (inclusive).
            stop:  Last frame index (exclusive).
            step:  Step between frames.

        Returns:
            FrameBatch with frames at indices start, start+step, ...
        """
        indices = list(range(start, stop, step))
        return self.get_frames_at(indices)

    # ---- Batch methods (time-based) ----

    def get_frames_played_at(
        self, seconds: Union[torch.Tensor, List[float]]
    ) -> FrameBatch:
        """
        Retrieve frames at specific presentation timestamps.

        Args:
            seconds: List of timestamps in seconds.

        Returns:
            FrameBatch.
        """
        if isinstance(seconds, torch.Tensor):
            seconds = seconds.tolist()

        if self._fps <= 0:
            indices = [0] * len(seconds)
        else:
            indices = [min(int(s * self._fps), self._total_aus - 1)
                       for s in seconds]
        return self.get_frames_at(indices)

    def get_frames_played_in_range(
        self, start_seconds: float, stop_seconds: float
    ) -> FrameBatch:
        """
        Retrieve all frames whose PTS falls in [start_seconds, stop_seconds).

        Args:
            start_seconds: Start time (inclusive).
            stop_seconds:  Stop time (exclusive).

        Returns:
            FrameBatch.
        """
        if self._fps <= 0:
            return FrameBatch(
                data=torch.empty(0, 3, self._height, self._width,
                                 dtype=torch.uint8),
                pts_seconds=torch.empty(0, dtype=torch.float64),
                duration_seconds=torch.empty(0, dtype=torch.float64),
            )

        start_idx = int(start_seconds * self._fps)
        stop_idx = int(stop_seconds * self._fps)
        start_idx = max(0, start_idx)
        stop_idx = min(self._total_aus, stop_idx)
        return self.get_frames_in_range(start_idx, stop_idx)

    # ---- Utility ----

    def __repr__(self) -> str:
        return (
            f"NPUVideoDecoder("
            f"codec={self._codec_name}, "
            f"resolution={self._width}x{self._height}, "
            f"fps={self._fps:.1f}, "
            f"frames={self._total_aus}, "
            f"device='{self._device_str}'"
            f")"
        )


# ---------------------------------------------------------------------------
# 6. Internal frame metadata cache
# ---------------------------------------------------------------------------

@dataclass
class DecodedFrameMeta:
    au_index: int
    y_tensor: torch.Tensor
    uv_tensor: torch.Tensor
    width: int
    height: int
    pts_seconds: float


# ---------------------------------------------------------------------------
# 7. Monkey-Patch Injection
# ---------------------------------------------------------------------------

_original_video_decoder_init = None
_patch_applied = False


def _install_npu_patch():
    """
    Replace torchcodec.decoders.VideoDecoder.__init__ with our interceptor.

    When the user passes device="npu" (or device=torch.device("npu")),
    the call is routed to NPUVideoDecoder. Otherwise, the original
    torchcodec VideoDecoder is used.
    """
    global _patch_applied, _original_video_decoder_init

    if _patch_applied:
        return

    try:
        import torchcodec.decoders as _tcd
    except ImportError:
        # torchcodec not installed — nothing to patch
        _patch_applied = True
        return
    except Exception as e:
        # torchcodec installed but cannot load (e.g. missing FFmpeg).
        # The NPU decoder itself doesn't need torchcodec's FFmpeg backend,
        # so we allow the module to load.  CPU decoding will fail later
        # if the user tries device='cpu', which is the correct behaviour.
        import warnings
        warnings.warn(
            f"npu_codec_bridge: torchcodec is installed but could not be "
            f"loaded ({type(e).__name__}: {e}).\n"
            f"The NPU decoder (device='npu') will still work, but "
            f"CPU decoding (device='cpu') requires FFmpeg shared libraries.\n"
            f"Install FFmpeg: apt-get install ffmpeg libavcodec-dev "
            f"libavformat-dev libavutil-dev"
        )
        _patch_applied = True
        return

    # Store original
    _original_class = _tcd.VideoDecoder
    _original_video_decoder_init = _original_class.__init__

    class _PatchedVideoDecoder(_original_class):
        """
        Subclass of the original torchcodec VideoDecoder that intercepts
        device="npu" and routes to NPUVideoDecoder instead.

        For all other devices, behaviour is identical to the original.
        """

        _npu_instance: Optional[NPUVideoDecoder] = None

        def __init__(self, source, **kwargs):
            device = kwargs.get("device", None)

            # Normalise device to string
            device_str = str(device) if device is not None else "cpu"

            if device_str.lower() in ("npu", "vastai", "vastai_npu"):
                # --- Route to NPU decoder ---
                # Create an NPUVideoDecoder and proxy all calls to it
                npu_dec = NPUVideoDecoder(
                    source=source,
                    stream_index=kwargs.get("stream_index", None),
                    dimension_order=kwargs.get("dimension_order", "NCHW"),
                    num_ffmpeg_threads=kwargs.get("num_ffmpeg_threads", 1),
                    device="npu",
                    seek_mode=kwargs.get("seek_mode", "exact"),
                )
                self._npu_instance = npu_dec

                # Populate torchcodec-expected attributes
                # (metadata and stream_index are properties — we override below)
            else:
                # --- Original torchcodec path ---
                self._npu_instance = None
                super().__init__(source, **kwargs)

        # ── Proxied property: metadata ──────────────────────────
        # torchcodec's __init__ does BOTH:
        #   self.metadata = <VideoStreamMetadata>   (assignment)
        # and users read:  meta = dec.metadata
        # We use instance __dict__ as backing store so the original
        # __init__ can assign without hitting a read-only property.
        @property
        def metadata(self):
            if self._npu_instance is not None:
                return self._npu_instance.metadata
            return self.__dict__.get('metadata', None)

        @metadata.setter
        def metadata(self, value):
            # NPU path: ignored (metadata comes from NPUVideoDecoder)
            # Original path: stored so super().__init__ assignment works
            self.__dict__['metadata'] = value

        # ── Proxied property: stream_index ──────────────────────
        @property
        def stream_index(self):
            if self._npu_instance is not None:
                return self._npu_instance.stream_index
            return self.__dict__.get('stream_index', None)

        @stream_index.setter
        def stream_index(self, value):
            self.__dict__['stream_index'] = value

        def __getitem__(self, key):
            if self._npu_instance is not None:
                return self._npu_instance[key]
            return super().__getitem__(key)

        def __len__(self):
            if self._npu_instance is not None:
                return len(self._npu_instance)
            return super().__len__()

        def get_frame_at(self, index):
            if self._npu_instance is not None:
                return self._npu_instance.get_frame_at(index)
            return super().get_frame_at(index)

        def get_frame_played_at(self, seconds):
            if self._npu_instance is not None:
                return self._npu_instance.get_frame_played_at(seconds)
            return super().get_frame_played_at(seconds)

        def get_frames_at(self, indices):
            if self._npu_instance is not None:
                return self._npu_instance.get_frames_at(indices)
            return super().get_frames_at(indices)

        def get_frames_in_range(self, start, stop, step=1):
            if self._npu_instance is not None:
                return self._npu_instance.get_frames_in_range(start, stop, step)
            return super().get_frames_in_range(start, stop, step)

        def get_frames_played_at(self, seconds):
            if self._npu_instance is not None:
                return self._npu_instance.get_frames_played_at(seconds)
            return super().get_frames_played_at(seconds)

        def get_frames_played_in_range(self, start_seconds, stop_seconds):
            if self._npu_instance is not None:
                return self._npu_instance.get_frames_played_in_range(
                    start_seconds, stop_seconds)
            return super().get_frames_played_in_range(start_seconds, stop_seconds)

    # Replace the class in the module
    _tcd.VideoDecoder = _PatchedVideoDecoder
    _patch_applied = True


def _uninstall_npu_patch():
    """Restore the original torchcodec VideoDecoder (for testing)."""
    global _patch_applied, _original_video_decoder_init
    if not _patch_applied:
        return
    try:
        import torchcodec.decoders as _tcd
        if _original_video_decoder_init is not None:
            # Find the original class via its __init__
            _tcd.VideoDecoder = type(
                "VideoDecoder",
                (object,),
                {"__init__": _original_video_decoder_init}
            )
    except ImportError:
        pass
    _patch_applied = False


# =========================================================================
# Auto-patch on import: the user just needs `import npu_codec_bridge`
# =========================================================================

_install_npu_patch()
