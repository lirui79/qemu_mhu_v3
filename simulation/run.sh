#!/usr/bin/env bash
set -euo pipefail
# 清理残留共享内存;/dev/shm 下可能有其他用户(root)遗留的文件导致权限不足,
# 此时不阻断仿真,让 VP 自行处理(见 conf.lua 的 shm 配置)。
rm -rf /dev/shm/* 2>/dev/null || true
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG_DIR="${DIR}/../log"
if ! mkdir -p "${LOG_DIR}" 2>/dev/null || ! touch "${LOG_DIR}/.wtest" 2>/dev/null; then
    # 原 log/ 目录不可写(例如之前用 root 跑过,目录/文件为 root 属主),
    # fallback 到用户可写目录,并通过 TS_LOG_DIR 传给 conf.lua
    LOG_DIR="${HOME}/.cache/qemu_mhu_v3/log"
    mkdir -p "${LOG_DIR}"
fi
rm -f "${LOG_DIR}/.wtest" 2>/dev/null || true
export TS_LOG_DIR="${LOG_DIR}"
# 删除并重建日志,避免沿用 root 属主的旧文件(截断会 Permission denied)。
rm -f "${LOG_DIR}/ts_core0.log" "${LOG_DIR}/ts_core1.log" \
      "${LOG_DIR}/ts_core0.clean.log" "${LOG_DIR}/ts_core1.clean.log" 2>/dev/null || true
: > "${LOG_DIR}/ts_core0.log" || true
: > "${LOG_DIR}/ts_core1.log" || true
: > "${LOG_DIR}/ts_core0.clean.log" || true
: > "${LOG_DIR}/ts_core1.clean.log" || true

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
CFG_FILE=conf.lua
#CFG_FILE=conf_sample.lua
export LD_LIBRARY_PATH="${DIR}/lib:${DIR}/lib/libqemu${HOST_LD:+:${HOST_LD}}"

# 关键:必须给 R52/A76 的 QEMU 实例显式配置 remote_argv.3 = ...gdb_port。
# 若缺失该参数,arm_gicv3 的 redistributor 共享内存路由未完整初始化,
# R52 CPU 读 GICR_TYPER 会 fall-through 到 0x0 读到向量表指令(0xE59FF018),
# getRedistID() 匹配失败 → R52 在 interrupt_init() 卡死(无日志/仅 GIC 垃圾)。
# 设 gdb_port=0 表示禁用 gdb server(不阻塞启动),但保留正确初始化路径。
GDB_PORT_R52="${GDB_PORT_R52:-0}"
GDB_PORT_A76="${GDB_PORT_A76:-0}"

"${DIR}/cortex-r52-a76-vp"  --gs_luafile "${DIR}/${CFG_FILE}" \
    --param 'platform.plugin_0.remote_argv.2="--param"' \
    --param "platform.plugin_0.remote_argv.3=remote_platform.cpu_0.gdb_port=${GDB_PORT_R52}" \
    --param 'platform.plugin_1.remote_argv.2="--param"' \
    --param "platform.plugin_1.remote_argv.3=remote_platform.cpu_0.gdb_port=${GDB_PORT_A76}" \
    "$@" || true
