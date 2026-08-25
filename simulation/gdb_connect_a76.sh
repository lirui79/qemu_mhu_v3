#!/bin/bash

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CFG_FILE=$(ps aux | grep '[c]ortex-r52-a76-vp' | grep -oP -- '--gs_luafile\s+\K\S+' | head -n 1)
if [ -z "${CFG_FILE}" ]; then
    echo "Failed to extract configuration file from running cortex-r52-a76-vp process"
    exit 1
fi

if [[ "${CFG_FILE}" == *"_linux.lua"* ]]; then
    ELF_FILE="${DIR}/../linux_a76/build/kernel/vmlinux"
else
    BIN_FILE=$(sed -n 's/.*EXEC_FILE_A76\s*=\s*"\([^"]*\)".*/\1/p' ${CFG_FILE})
    if [ -z "${BIN_FILE}" ]; then
        echo "Failed to extract executable file from conf.lua"
        exit 1
    fi
    ELF_FILE="${DIR}/${BIN_FILE%.bin}.elf"
fi

echo -e "\033[1;36mexec : ${ELF_FILE}\033[0m"

gdb-multiarch \
    -ex "target remote localhost:4322" \
    ${ELF_FILE}

