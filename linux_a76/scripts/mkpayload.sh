#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 5 ]]; then
	echo "usage: $0 BOOT DTB IMAGE INITRAMFS OUTPUT" >&2
	exit 2
fi

boot=$1
dtb=$2
image=$3
initramfs=$4
output=$5

# Payload 内部布局（偏移量相对 payload 镜像；仿真器把整包加载到 DDR_BASE + PAYLOAD_LOAD_OFF）
#   0x0000000  boot shim    -> 0x80100000
#   0x0010000  DTB          -> 0x80110000
#   0x0100000  Linux Image  -> 0x80200000
#   0x4000000  initramfs    -> 0x84100000
DDR_BASE=$((0x80000000))          # simulation/conf.lua: ddr_0
PAYLOAD_LOAD_OFF=$((0x100000))    # simulation/conf.lua: ddr_0.load.offset
BOOT_OFF=$((0x0))
DTB_OFF=$((0x10000))
IMAGE_OFF=$((0x100000))
INITRAMFS_OFF=$((0x4000000))

# initramfs 窗口大小以 DTB 的 linux,initrd-start/end 为准（内核只认 DTB 里的窗口），
# 这里反推出来做校验，避免 DTS 与打包脚本两处写死却不一致。
dtc_bin=${DTC:-dtc}
dts_dump=$("${dtc_bin}" -I dtb -O dts "${dtb}" 2>/dev/null || true)
initrd_prop() {
	printf '%s\n' "${dts_dump}" |
		sed -n "s/.*linux,initrd-${1} = <[^>]*\(0x[0-9a-fA-F]*\)>.*/\1/p" | head -1
}
initrd_start=$(initrd_prop start)
initrd_end=$(initrd_prop end)
if [[ -z "${initrd_start}" || -z "${initrd_end}" ]]; then
	echo "error: ${dtb} has no linux,initrd-start/end" >&2
	exit 1
fi
initrd_start=$((initrd_start))
initrd_end=$((initrd_end))

expected_start=$((DDR_BASE + PAYLOAD_LOAD_OFF + INITRAMFS_OFF))
if (( initrd_start != expected_start )); then
	echo "error: DTB initrd-start 0x$(printf '%x' "${initrd_start}") != 0x$(printf '%x' "${expected_start}")" >&2
	echo "       (DDR_BASE + payload load offset + initramfs offset)" >&2
	exit 1
fi

limit=$((initrd_end - initrd_start))
if [[ -n "${INITRAMFS_LIMIT:-}" ]]; then
	if (( INITRAMFS_LIMIT > limit )); then
		echo "error: INITRAMFS_LIMIT ($((INITRAMFS_LIMIT / 1048576)) MB) exceeds the DTB initrd window" >&2
		echo "       ($((limit / 1048576)) MB); update linux,initrd-end in dts/cortex-r52-a76-vp.dts first" >&2
		exit 1
	fi
	limit=${INITRAMFS_LIMIT}
fi
IMAGE_SIZE=$((INITRAMFS_OFF + limit))

check_size() {
	local file=$1 limit=$2 label=$3
	local size
	size=$(stat -c '%s' "${file}")
	if (( size > limit )); then
		echo "${label} is too large: ${size} > ${limit}" >&2
		echo "hint: raise linux,initrd-end in dts/cortex-r52-a76-vp.dts (and keep the" >&2
		echo "      window below r52_memalloc @ 0x8F000000), or shrink the initramfs" >&2
		exit 1
	fi
}

check_size "${boot}" $((0x10000)) "boot shim"
check_size "${dtb}" $((0xf0000)) "DTB"
check_size "${image}" $((0x3f00000)) "Linux Image"
check_size "${initramfs}" "${limit}" "initramfs"

mkdir -p "$(dirname "${output}")"
truncate -s "${IMAGE_SIZE}" "${output}"
dd if="${boot}" of="${output}" bs=4K seek=$((BOOT_OFF / 4096)) conv=notrunc status=none
dd if="${dtb}" of="${output}" bs=4K seek=$((DTB_OFF / 4096)) conv=notrunc status=none
dd if="${image}" of="${output}" bs=4K seek=$((IMAGE_OFF / 4096)) conv=notrunc status=none
dd if="${initramfs}" of="${output}" bs=4K seek=$((INITRAMFS_OFF / 4096)) conv=notrunc status=none

echo "payload layout: boot@${BOOT_OFF} dtb@${DTB_OFF} image@${IMAGE_OFF} initramfs@${INITRAMFS_OFF}"
echo "  initrd window 0x$(printf '%x' "${initrd_start}")..0x$(printf '%x' "${initrd_end}") ($((limit / 1048576)) MB), initramfs $(stat -c '%s' "${initramfs}") bytes"
