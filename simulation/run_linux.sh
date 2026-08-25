#!/usr/bin/env bash
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG_DIR="${DIR}/../log"
mkdir -p "${LOG_DIR}"
: > "${LOG_DIR}/ts_core0.log"
: > "${LOG_DIR}/ts_core1.log"
: > "${LOG_DIR}/ts_core0.clean.log"
: > "${LOG_DIR}/ts_core1.clean.log"

cleanup_logs() {
	tr -d '\000' < "${LOG_DIR}/ts_core0.log" > "${LOG_DIR}/ts_core0.clean.log" 2>/dev/null || true
	tr -d '\000' < "${LOG_DIR}/ts_core1.log" > "${LOG_DIR}/ts_core1.clean.log" 2>/dev/null || true
}

sanitize_ld_path() {
	local input="${1:-}" output="" path
	IFS=':' read -r -a paths <<< "${input}"
	for path in "${paths[@]}"; do
		[[ -n "${path}" ]] || continue
		case "${path}" in
			*/cuda*|*cuda*/lib*|*cuda*/targets/*) continue ;;
		esac
		output="${output:+${output}:}${path}"
	done
	printf '%s' "${output}"
}

HOST_LD="$(sanitize_ld_path "${LD_LIBRARY_PATH:-}")"
export LD_LIBRARY_PATH="${DIR}/lib:${DIR}/lib/libqemu${HOST_LD:+:${HOST_LD}}"

test -f "${DIR}/../linux_a76/build/linux_payload.bin" || {
	echo "error: build linux_a76/build/linux_payload.bin first" >&2
	exit 1
}

# Save terminal settings so they can be restored after the simulator exits
# (the simulator may leave the terminal in raw/no-echo mode when killed)
SAVED_STTY="$(stty -g 2>/dev/null)" || true
restore_terminal() {
	[[ -n "${SAVED_STTY:-}" ]] && stty "${SAVED_STTY}" 2>/dev/null || true
}
trap 'restore_terminal; cleanup_logs' EXIT

CFG_FILE=conf_linux.lua
"${DIR}/cortex-r52-a76-vp" \
	--gs_luafile "${DIR}/${CFG_FILE}" "$@" || true
