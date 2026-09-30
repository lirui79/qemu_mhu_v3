"""
npu_codec_bridge — Build script

Two modes:
  pip install -e .                        → STUB mode (no NPU hardware needed)
  NPU_LIVE_MODE=1 pip install -e .        → LIVE mode (links real NPU SDK)

The stub mode creates a fully functional Python package with a mock C++
decoder that returns random NV12 frames. All torchcodec monkey-patching
and API surface is identical.
"""

import os
import subprocess
import sys
from pathlib import Path

import setuptools
from setuptools import setup, find_packages
from setuptools.command.build_ext import build_ext


class CMakeBuildExt(build_ext):
    """Custom build_ext that invokes CMake to build the _C extension."""

    def run(self):
        for ext in self.extensions:
            self.build_cmake(ext)

    def build_cmake(self, ext):
        extdir = Path(self.get_ext_fullpath(ext.name)).parent
        # For editable installs, extdir is in the source tree already.
        # We need the actual package directory where the .so will be placed.
        package_dir = Path(__file__).parent / "npu_codec_bridge"
        sourcedir = Path(__file__).parent.resolve()
        build_temp = (Path(self.build_temp) / "cmake_build").resolve()

        # Determine build mode
        npu_live = os.environ.get("NPU_LIVE_MODE", "0") == "1"
        build_type = os.environ.get("CMAKE_BUILD_TYPE", "Release")

        # Configure cmake prefix path: must include both Torch and pybind11.
        # When using --no-build-isolation, these are found in the system packages.
        # When using build isolation (default pip), provide a clear error.
        cmake_prefix_parts = []

        # -- pybind11 --
        try:
            import pybind11
            pybind11_cmake_dir = pybind11.get_cmake_dir()
        except ImportError:
            pybind11_cmake_dir = None

        # -- Torch --
        try:
            import torch
            cmake_prefix_parts.append(torch.utils.cmake_prefix_path)
        except ImportError:
            raise RuntimeError(
                "Cannot find PyTorch installation.\n"
                "Re-run with --no-build-isolation:\n"
                "  pip install --no-build-isolation -e .\n"
                "Or install PyTorch first:\n"
                "  pip install torch"
            )

        cmake_prefix = ";".join(cmake_prefix_parts)

        cmake_args = [
            f"-DCMAKE_BUILD_TYPE={build_type}",
            f"-DNPU_LIVE_MODE={'ON' if npu_live else 'OFF'}",
            f"-DPYTHON_EXECUTABLE={sys.executable}",
            f"-DCMAKE_PREFIX_PATH={cmake_prefix}",
            "-DCMAKE_VERBOSE_MAKEFILE=OFF",
        ]
        if pybind11_cmake_dir:
            cmake_args.append(f"-Dpybind11_DIR={pybind11_cmake_dir}")

        build_args = ["--config", build_type]
        # Parallel build
        n_jobs = os.environ.get("CMAKE_BUILD_PARALLEL_LEVEL", str(os.cpu_count() or 4))
        build_args += ["--", f"-j{n_jobs}"]

        build_temp.mkdir(parents=True, exist_ok=True)

        print(f"\n{'='*60}")
        print(f"  NPU Codec Bridge — CMake Build")
        print(f"  Mode:       {'LIVE (real NPU SDK)' if npu_live else 'STUB (mock)'}")
        print(f"  Build type: {build_type}")
        print(f"  Source:     {sourcedir}")
        print(f"  Output:     {extdir}/npu_codec_bridge")
        print(f"{'='*60}\n")

        # Configure
        subprocess.check_call(
            ["cmake", str(sourcedir)] + cmake_args,
            cwd=str(build_temp),
        )

        # Build
        subprocess.check_call(
            ["cmake", "--build", "."] + build_args,
            cwd=str(build_temp),
        )

        # Copy the built .so to the package directory
        package_dir.mkdir(parents=True, exist_ok=True)

        for so_file in build_temp.glob("_C.*.so"):
            dest = package_dir / so_file.name
            if not dest.exists() or so_file.stat().st_mtime > dest.stat().st_mtime:
                import shutil
                shutil.copy2(str(so_file), str(dest))
                print(f"  Copied: {so_file.name} → {dest}")


# =========================================================================
# Package metadata
# =========================================================================

setup(
    name="npu_codec_bridge",
    version="0.1.0",
    description="NPU hardware video decoder bridge for torchcodec",
    license="BSD-2-Clause",
    packages=find_packages(),
    python_requires=">=3.9",
    install_requires=[
        "torch>=2.0.0",
    ],
    extras_require={
        "dev": [
            "pytest>=7.0",
            "torchcodec>=0.1.0",
        ],
    },
    ext_modules=[
        # Placeholder — the actual build is done by CMakeBuildExt
        setuptools.Extension("npu_codec_bridge._C", sources=[]),
    ],
    cmdclass={
        "build_ext": CMakeBuildExt,
    },
)
