#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

KERNEL_SRC="${KERNEL_SRC:-${1:-}}"
KERNEL_BUILD="${KERNEL_BUILD:-${ROOT}/linux_a76/build/kernel}"
INIT_AUTORUN="${INIT_AUTORUN:-0}"

if [[ "${KERNEL_SRC}" -eq "" ]]; then
KERNEL_SRC=${ROOT}/linux-6.8
fi

echo "KERNEL_SRC=${KERNEL_SRC}"

if [[ ! -f "${KERNEL_SRC}/Makefile" ]]; then
	echo "error: KERNEL_SRC is not a Linux source tree: ${KERNEL_SRC:-<empty>}" >&2
	echo "usage: KERNEL_SRC=/path/to/linux-6.8 $0" >&2
	echo "   or: $0 /path/to/linux-6.8" >&2
	exit 1
fi

cd "${ROOT}"

make -C task_scheduler clean all

if [[ ! -f "${KERNEL_BUILD}/.config" ]]; then
	make -C linux_a76 KERNEL_SRC="${KERNEL_SRC}" KERNEL_BUILD="${KERNEL_BUILD}" kernel-config
fi

make -C linux_a76 KERNEL_SRC="${KERNEL_SRC}" KERNEL_BUILD="${KERNEL_BUILD}" \
	INIT_AUTORUN="${INIT_AUTORUN}" all

exec ./simulation/run_linux_gdb.sh
