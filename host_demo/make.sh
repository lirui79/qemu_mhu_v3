#!/usr/bin/env bash

make clean;make

cp build/bin/host_demo.elf ../simulation/fw/cortex-a76/cortex-a76.elf 
cp build/bin/host_demo.bin ../simulation/fw/cortex-a76/cortex-a76.bin 