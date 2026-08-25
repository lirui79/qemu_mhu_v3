# Cortex-R52 + Cortex-A76 MHU Virtual Platform (Runtime Package)

Prebuilt runtime for the dual-core R52/A76 VP with MHU-DavarAE inter-processor
communication (doorbell + FIFO/data windows). No source code is included.

## Contents

| Path | Description |
|------|-------------|
| `cortex-r52-a76-vp` | Main virtual platform executable |
| `remote_cpu_r52_mhu` | R52 remote CPU process (spawned automatically) |
| `remote_cpu_a76_mhu` | A76 remote CPU process (spawned automatically) |
| `conf.lua` | Platform configuration |
| `fw/cortex-r52/` | R52 firmware `.bin` / `.elf` |
| `fw/cortex-a76/` | A76 firmware `.bin` / `.elf` |
| `addr_map.xlsx` | Address / register map |
| `inter.md` | Interrupt + MHU address summary |
| `lib/` | Runtime `.so` (plugins + build-tree deps; real file copies) |
| `lib/libqemu/` | Bundled QEMU backend |

## Requirements

- Linux x86_64 or WSL2 (Ubuntu 22.04+ recommended)
- Standard system libraries: `libstdc++`, `libz`, glib, etc.

## Run

```bash
cd usr_tool/cortex-r52-a76-mhu
./run.sh
```

Success path prints `MHU TEST PASSED` (doorbell R52↔A76).

## Debug with GDB

**Terminal 1** — start VP (R52 port 4321, A76 port 4322):

```bash
./run_gdb.sh
# or: ./run_gdb.sh 4321 4322
```

**Terminal 2** — R52:

```bash
arm-none-eabi-gdb fw/cortex-r52/cortex-r52.elf
(gdb) set architecture arm
(gdb) target remote localhost:4321
(gdb) break c_entry
(gdb) continue
```

**Terminal 3** — A76:

```bash
aarch64-elf-gdb fw/cortex-a76/cortex-a76.elf
# or: gdb-multiarch fw/cortex-a76/cortex-a76.elf
(gdb) target remote localhost:4322
(gdb) break main
(gdb) continue
```

## Notes

- Always run from this directory (`run.sh` sets `LD_LIBRARY_PATH` to `lib/` + `lib/libqemu/`).
- Both `remote_cpu_*_mhu` binaries must stay alongside `cortex-r52-a76-vp`.
- All libraries under `lib/` are **dereferenced copies** (no symlinks).
