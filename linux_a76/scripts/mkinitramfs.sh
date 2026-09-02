#!/usr/bin/env bash
set -euo pipefail

SCRIPT_PATH="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RPATH="$(cd "${SCRIPT_PATH}/../.." && pwd)"

echo "RPATH=${RPATH}"
echo "SCRIPT_PATH=${SCRIPT_PATH}"


HOST_LINUX_DRIVER="${RPATH}/runtime_demo"
VPU_DRIVER_DIR="${HOST_LINUX_DRIVER}/driver_vpu/vcodec"
MEM_DRIVER_DIR="${HOST_LINUX_DRIVER}/driver_vpu/memalloc"

echo "HOST_LINUX_DRIVER=${HOST_LINUX_DRIVER}"
echo "MEM_DRIVER_DIR=${MEM_DRIVER_DIR}"
echo "VPU_DRIVER_DIR=${VPU_DRIVER_DIR}"

BIN_DIR="${RPATH}/linux_a76/bins"

BS_DIR="${RPATH}/runtime_demo/userspace_vpu/VC9000D/data"
TBCFG="${RPATH}/runtime_demo/userspace_vpu/VC9000D/binary_0x1ff0/tb.cfg"
YUV_DIR="${RPATH}/runtime_demo/userspace_vpu/VC9000E/data"

echo "BIN_DIR=${BIN_DIR}"
echo "BS_DIR=${BS_DIR}"
echo "TBCFG=${TBCFG}"
echo "YUV_DIR=${YUV_DIR}"

H264_LAUNCHER="${BIN_DIR}/h264_testenc"
HEVC_LAUNCHER="${BIN_DIR}/hevc_testenc"
DEC_LAUNCHER="${BIN_DIR}/g2dec"
DEC_SO="${BIN_DIR}/lib_vcd.so"

VPU_MODULE="${VPU_DRIVER_DIR}/vcodec.ko"
MEM_MODULE="${MEM_DRIVER_DIR}/memalloc.ko"

echo "VPU_MODULE=${VPU_MODULE}"
echo "MEM_MODULE=${MEM_MODULE}"

if [[ $# -lt 4 || $# -gt 6 ]]; then
	echo "usage: $0 INIT LAUNCH_KERNEL MODULE OUTPUT [OVERLAY_DIR] [AUTORUN]" >&2
	exit 2
fi

init=$1
launcher=$2
module=$3
output=$4
overlay=${5:-}
autorun=${6:-0}
root=$(mktemp -d)
trap 'rm -rf "${root}"' EXIT

if [[ "${init}" == *.tar.gz ]]; then
	# Extract rootfs tarball as base
	tar -xzf "${init}" --strip-components=1 -C "${root}"
else
	# Install init
	install -D -m 0755 "${init}" "${root}/init"
	mkdir -p "${root}/dev" "${root}/proc" "${root}/sys" "${root}/etc"
fi

# Install launcher and module on top
install -D -m 0755 "${launcher}" "${root}/bin/launch_kernel"
install -D -m 0644 "${module}" "${root}/lib/modules/r52_gpu.ko"

install -D -m 0755 "${H264_LAUNCHER}" "${root}/bin/h264_testenc"
install -D -m 0755 "${HEVC_LAUNCHER}" "${root}/bin/hevc_testenc"
install -D -m 0755 "${DEC_LAUNCHER}" "${root}/bin/g2dec"

if [[ -f "${DEC_SO}" ]]; then
	install -D -m 0644 "${DEC_SO}" "${root}/lib/lib_vcd.so"
fi

install -D -m 0644 "${VPU_MODULE}" "${root}/lib/modules/vcodec.ko"
install -D -m 0644 "${MEM_MODULE}" "${root}/lib/modules/memalloc.ko"

cp "${TBCFG}" "${root}/bin/"
chmod 0666 "${root}/bin/tb.cfg"

cp  "${BS_DIR}/akiyo_352x288_300_IBBBP.h264" "${root}/home/"
cp  "${BS_DIR}/sample_640x360.hevc" "${root}/home/"
cp  "${YUV_DIR}/akiyo_352x288_10.yuv" "${root}/home/"
chmod 0666 "${root}/home/akiyo_352x288_300_IBBBP.h264"
chmod 0666 "${root}/home/sample_640x360.hevc"
chmod 0666 "${root}/home/akiyo_352x288_10.yuv"


if [[ "${autorun}" == "1" || "${autorun}" == "y" || "${autorun}" == "yes" ]]; then
	: > "${root}/etc/r52_gpu_autorun"
fi
if [[ -n "${overlay}" ]]; then
	test -d "${overlay}" || {
		echo "error: overlay dir not found: ${overlay}" >&2
		exit 1
	}
	cp -a "${overlay}/." "${root}/"
fi

mkdir -p "$(dirname "${output}")"
(
	cd "${root}"
	find . -print0 | cpio --null -o --format=newc --quiet
) > "${output}"
