#!/usr/bin/env bash
set -euo pipefail

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
