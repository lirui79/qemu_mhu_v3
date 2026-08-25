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

# Save terminal settings so they can be restored after the simulator exits
# (the simulator may leave the terminal in raw/no-echo mode when killed)
SAVED_STTY="$(stty -g 2>/dev/null)" || true
restore_terminal() {
	[[ -n "${SAVED_STTY:-}" ]] && stty "${SAVED_STTY}" 2>/dev/null || true
}
trap 'restore_terminal; cleanup_logs' EXIT

GDB_PORT_R52="${1:-4321}"
GDB_PORT_A76="${2:-4322}"

# Drop CUDA/toolkit dirs that ship a too-old libOpenCL.so.1 (no OPENCL_3.0).
sanitize_ld_path() {
    local in="${1:-}" out="" p
    IFS=':' read -r -a parts <<< "${in}"
    for p in "${parts[@]}"; do
        [[ -n "${p}" ]] || continue
        case "${p}" in
            */cuda*|*cuda*/lib*|*cuda*/targets/*) continue ;;
        esac
        out="${out:+${out}:}${p}"
    done
    printf '%s' "${out}"
}
HOST_LD="$(sanitize_ld_path "${LD_LIBRARY_PATH:-}")"
CFG_FILE=conf_linux.lua
#CFG_FILE=conf_sample.lua
export LD_LIBRARY_PATH="${DIR}/lib:${DIR}/lib/libqemu${HOST_LD:+:${HOST_LD}}"
exec "${DIR}/cortex-r52-a76-vp"  --gs_luafile "${DIR}/${CFG_FILE}" \
    --param 'platform.plugin_0.remote_argv.2="--param"' \
    --param "platform.plugin_0.remote_argv.3=remote_platform.cpu_0.gdb_port=${GDB_PORT_R52}" \
    --param 'platform.plugin_1.remote_argv.2="--param"' \
    --param "platform.plugin_1.remote_argv.3=remote_platform.cpu_0.gdb_port=${GDB_PORT_A76}" \
    "${@:3}" || true
