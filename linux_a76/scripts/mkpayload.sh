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

check_size() {
	local file=$1 limit=$2 label=$3
	local size
	size=$(stat -c '%s' "${file}")
	if (( size > limit )); then
		echo "${label} is too large: ${size} > ${limit}" >&2
		exit 1
	fi
}

check_size "${boot}" $((0x10000)) "boot shim"
check_size "${dtb}" $((0xf0000)) "DTB"
check_size "${image}" $((0x3f00000)) "Linux Image"
check_size "${initramfs}" $((0x1000000)) "initramfs"

mkdir -p "$(dirname "${output}")"
truncate -s $((0x5100000)) "${output}"
dd if="${boot}" of="${output}" bs=4K seek=$((0x0 / 4096)) conv=notrunc status=none
dd if="${dtb}" of="${output}" bs=4K seek=$((0x10000 / 4096)) conv=notrunc status=none
dd if="${image}" of="${output}" bs=4K seek=$((0x100000 / 4096)) conv=notrunc status=none
dd if="${initramfs}" of="${output}" bs=4K seek=$((0x4000000 / 4096)) conv=notrunc status=none
