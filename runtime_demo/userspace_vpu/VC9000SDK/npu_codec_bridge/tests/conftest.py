"""
Shared pytest fixtures for npu_codec_bridge tests.

IMPORTANT: This conftest.py applies the npu_codec_bridge monkey-patch
before any test imports torchcodec. This mirrors real-world usage where
`import npu_codec_bridge` comes before `import torchcodec.decoders`.
"""

import os
import struct
import subprocess
import pytest

# ── Apply the monkey-patch FIRST (before torchcodec is imported) ──
import npu_codec_bridge  # noqa: F401  — triggers _install_npu_patch()

# Detect build mode
from npu_codec_bridge._C import __mode__ as _BUILD_MODE
IS_LIVE = _BUILD_MODE == "live"

# Path to the project's test bitstream files (from vastai_video_sdk repo)
REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
UT_RES = os.path.join(REPO_ROOT, "ut", "res")
DEC_RES = os.path.join(UT_RES, "dec")

# Cached valid test stream (generated once on first use)
_VALID_H264_CACHE = None


# ---------------------------------------------------------------------------
# Valid H.264 generator (for LIVE mode tests that need real bitstreams)
# ---------------------------------------------------------------------------

def _valid_test_data(num_frames=30) -> bytes:
    """Return NPU-decodable test data, cached after first generation.

    Uses /tmp/test_live_30f.h265 if available (pre-generated from
    fixed_hevc.mp4), otherwise falls back to FFmpeg libx265 generation.
    """
    global _VALID_H264_CACHE
    if _VALID_H264_CACHE is not None:
        return _VALID_H264_CACHE[:]

    # Preferred: use the pre-verified NPU-compatible test file
    verified_path = "/tmp/test_live_30f.h265"
    if os.path.exists(verified_path):
        with open(verified_path, "rb") as f:
            _VALID_H264_CACHE = f.read()
        return _VALID_H264_CACHE

    # Try generating with FFmpeg + libx265
    try:
        result = subprocess.run([
            "ffmpeg", "-y",
            "-f", "lavfi", "-i", "testsrc2=size=320x240:rate=30:duration=1",
            "-vframes", str(num_frames),
            "-c:v", "libx265", "-preset", "ultrafast",
            "-bsf:v", "hevc_mp4toannexb",
            "-f", "hevc", "-"
        ], capture_output=True, timeout=30)
        if result.returncode == 0 and len(result.stdout) > 1000:
            _VALID_H264_CACHE = result.stdout
            return result.stdout
    except Exception:
        pass

    # Fallback: read the first real HEVC test file
    for candidate in [
        os.path.join(DEC_RES, "cdzj.hevc"),
        "/tmp/test_live_30f.h265",
    ]:
        if os.path.exists(candidate):
            with open(candidate, "rb") as f:
                _VALID_H264_CACHE = f.read()
            return _VALID_H264_CACHE

    raise RuntimeError("Cannot find valid NPU-decodable test stream")


# ---------------------------------------------------------------------------
# Synthetic bitstream generators (for STUB mode — fast, no I/O)
# ---------------------------------------------------------------------------

def _make_annexb_au(nal_type: int, payload_len: int = 50) -> bytes:
    """Create a single Annex-B Access Unit with a 4-byte start code."""
    header = b'\x00\x00\x00\x01'
    nal_header = bytes([nal_type & 0x1F])
    payload = bytes([(i % 251 + 4) for i in range(payload_len)])
    return header + nal_header + payload


def synthetic_h264_stream(num_frames: int = 30) -> bytes:
    """Generate minimal synthetic H.264 Annex-B bitstream (STUB mode only).

    In LIVE mode, use valid_h264_stream() instead — the NPU hardware
    cannot decode garbage NAL payloads.
    """
    parts = []
    for i in range(num_frames):
        if i == 0:
            parts.append(_make_annexb_au(7, payload_len=30))   # SPS
            parts.append(_make_annexb_au(8, payload_len=10))   # PPS
            parts.append(_make_annexb_au(5, payload_len=200))  # IDR slice
        elif i % 15 == 0:
            parts.append(_make_annexb_au(5, payload_len=180))  # IDR
        else:
            parts.append(_make_annexb_au(1, payload_len=150))  # P slice
    return b''.join(parts)


# ---------------------------------------------------------------------------
# Test stream fixtures (adapt to STUB vs LIVE mode)
# ---------------------------------------------------------------------------

@pytest.fixture
def synthetic_stream():
    """A 30-frame H.264 Annex-B byte string.

    In STUB mode: synthetic (fast, no I/O).
    In LIVE mode: valid bitstream (hardware-decodable).
    """
    if IS_LIVE:
        return _valid_test_data(num_frames=30)
    return synthetic_h264_stream(num_frames=30)


@pytest.fixture
def short_synthetic_stream():
    """A short (3-5 frame) H.264 Annex-B byte string."""
    if IS_LIVE:
        return _valid_test_data(num_frames=5)
    return synthetic_h264_stream(num_frames=5)


@pytest.fixture
def real_h264_path():
    """Path to a real H.264 test file."""
    for path in [
        os.path.join(DEC_RES, "cdzj.h264"),
        os.path.join(UT_RES, "short-186.h264"),
        os.path.join(DEC_RES, "stream.h264"),
    ]:
        if os.path.exists(path):
            return path
    return None


@pytest.fixture
def real_hevc_path():
    """Path to a real HEVC test file."""
    for path in [
        os.path.join(DEC_RES, "cdzj.hevc"),
        os.path.join(UT_RES, "1920x1088_121.hevc"),
    ]:
        if os.path.exists(path):
            return path
    return None


# ---------------------------------------------------------------------------
# NPU decoder fixtures
# ---------------------------------------------------------------------------

@pytest.fixture
def patch_is_active():
    """Ensure the monkey-patch has been applied."""
    return True


@pytest.fixture
def npy_decoder(synthetic_stream):
    """Create an NPUVideoDecoder from a test stream.

    In STUB mode: uses synthetic data.
    In LIVE mode: uses valid H.264 from FFmpeg or a real test file.
    """
    from npu_codec_bridge import NPUVideoDecoder
    return NPUVideoDecoder(synthetic_stream, device="npu")


# ---------------------------------------------------------------------------
# LIVE-mode-only helper: check if hardware is available
# ---------------------------------------------------------------------------

def require_npu_hardware():
    """Return True if NPU hardware is accessible, else skip."""
    if not IS_LIVE:
        return False
    if not os.path.exists("/dev/vastai_video0"):
        pytest.skip("NPU device /dev/vastai_video0 not found")
    return True
