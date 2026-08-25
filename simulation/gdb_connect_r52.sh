#!/bin/bash

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CFG_FILE=$(grep -E '^CFG_FILE=' ${DIR}/run_gdb.sh | head -n 1 | cut -d '=' -f 2)
if [ -z "${CFG_FILE}" ]; then
    echo "Failed to extract configuration file from run_gdb.sh"
    exit 1
fi

BIN_FILE=$(sed -n 's/.*EXEC_FILE_R52\s*=\s*"\([^"]*\)".*/\1/p' ${DIR}/${CFG_FILE})
if [ -z "${BIN_FILE}" ]; then
    echo "Failed to extract executable file from conf.lua"
    exit 1
fi

ELF_FILE="${DIR}/${BIN_FILE%.bin}.elf"

echo -e "\033[1;36mexec : ${ELF_FILE}\033[0m"
echo -e "\033[1;36m tip : use \"thread\" command to switch between cores\033[0m"

gdb-multiarch \
    -ex "set architecture arm" \
    -ex "target remote localhost:4321" \
    ${ELF_FILE}

