/*
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * AARCH64 linker script for Cortex-A76 bare-metal firmware.
 * Firmware is loaded at 0x80100000 (DDR_BASE + 0x100000).
 */

ENTRY(_start)

MEMORY {
  rom (rx)  : ORIGIN = 0x80100000, LENGTH = 0x400000
  ram (rwx) : ORIGIN = 0x80500000, LENGTH = 0x400000
}

SECTIONS
{
    . = 0x80100000;
    __boot_start = .;

    .text : {
        KEEP(*(.text.boot))
        KEEP(*(.text.vector_table))
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    . = ALIGN(8);
    __bss_start = .;
    .bss (NOLOAD) : {
        *(.bss*)
        *(COMMON)
    }
    . = ALIGN(8);
    __bss_end = .;

    . = ALIGN(16);
    . = . + 0x20000;            /* 128KB startup/EL1 exception stack */
    __stack_top = .;

    __end = .;
}
