#!/usr/bin/env bash

make clean;make

cp build/bin/task_sched.bin ../simulation/fw/cortex-r52/cortex-r52.bin 

cp build/bin/task_sched.elf ../simulation/fw/cortex-r52/cortex-r52.elf 
