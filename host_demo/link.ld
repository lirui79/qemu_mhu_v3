/*
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * AARCH64 linker script for Cortex-A76 bare‑metal firmware.
 * Firmware is loaded at 0x80100000 (DDR_BASE + 0x100000).
 */
ENTRY(_start)
MEMORY {
  rom (rx)  : ORIGIN = 0x80100000, LENGTH = 0x400000
  ram (rwx) : ORIGIN = 0x80500000, LENGTH = 0x400000
}
SECTIONS
{
  .vectors : {
    *arch/startup.o(.text.boot)
    *(.text._start)
  } > rom

  .text : {
    *(.text)
    *(.text.*)
    *(.rodata .rodata.*)
    *(.note.gnu.build-id)
  } > rom

  .data : {
    *(.data .data.*)
  } > ram

  .bss : {
    . = ALIGN (16);
    __bss_start = . ;
    *(.bss .bss.*)
    *(COMMON)
    . = ALIGN (16);
    __bss_end = . ;
  } > ram

  .stack : {
    . = ALIGN (16);
    . += 0x200000;
    _stack_top = .;
  } > ram
}
