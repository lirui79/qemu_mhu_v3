/*
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * AARCH64 bare-metal startup for Cortex-A76.
 *
 * Boot flow: EL3 -> EL2 -> EL1 with MMU/caches off.
 *
 * Memory layout at 0x80100000:
 *   0x000 - 0x7FF:  AARCH64 exception vector table (2 KB, 0x800 aligned)
 *   0x800 - ...   :  Boot code (EL3 -> EL2 -> EL1 init)
 *
 * rvbar is set to 0x80100000.  The first instruction at that address
 * is an immediate branch past the vector table into the boot code.
 * VBAR_EL3 is programmed to 0x80100000 for proper vector delivery.
 */

.section .vectors, "ax"
.global _start
.global _exception_vector
.global _entry

/* ===================================================================
 * _start: entry point (RVBAR = 0x80100000).
 * Branches immediately into the boot code past the vector table.
 * =================================================================== */
.align 12
_start:
    b    _entry

/* ===================================================================
 * Exception vector table at VBAR = 0x80100000 (2-KiB aligned).
 * Each entry = 32 bytes (branch to handler).
 * =================================================================== */
.align 11
_exception_vector:

/* ---- Current EL with SP0 ---- */
  .align 7
  b    el3_sync_sp0
  .align 7
  b    el3_irq_sp0
  .align 7
  b    el3_fiq_sp0
  .align 7
  b    el3_serr_sp0

/* ---- Current EL with SPx ---- */
  .align 7
  b    el3_sync_spx
  .align 7
  b    el3_irq_aarch64       /* IRQ at current EL (EL1) — uses same handler as lower-EL IRQ */
  .align 7
  b    el3_fiq_spx
  .align 7
  b    el3_serr_spx

/* ---- Lower EL using AARCH64 ---- */
  .align 7
  b    el3_sync_aarch64
  .align 7
  b    el3_irq_aarch64       /* IRQ from lower EL (AARCH64) */
  .align 7
  b    el3_fiq_aarch64
  .align 7
  b    el3_serr_aarch64

/* ---- Lower EL using AARCH32 ---- */
  .align 7
  b    el3_sync_aarch32
  .align 7
  b    el3_irq_aarch32
  .align 7
  b    el3_fiq_aarch32
  .align 7
  b    el3_serr_aarch32

/* ===================================================================
 * _entry: real boot code (starts right after 2-KiB vector table)
 * =================================================================== */
_entry:
    /* Identify current EL */
    mrs    x0, CurrentEL
    and    x0, x0, #0xC
    cmp    x0, #0xC           /* EL3 = 0xC */
    b.eq   el3_entry
    cmp    x0, #0x8           /* EL2 = 0x8 */
    b.eq   el2_entry
    cmp    x0, #0x4           /* EL1 = 0x4 */
    b.eq   el1_entry
    b      _panic

/* ===================================================================
 * EL3 entry — configure and drop to EL2
 * =================================================================== */
el3_entry:
    /* Set VBAR_EL3 to our vector table */
    adr    x0, _exception_vector
    msr    VBAR_EL3, x0

    /* SCR_EL3: no trap for SMC, HVC, etc. */
    mov    x0, #0
    msr    SCR_EL3, x0

    /* SCTLR_EL3: standard reset value */
    mov    x0, #0
    msr    SCTLR_EL3, x0

    /* Set SP_EL3 */
    ldr    x0, =_el3_stack_top
    mov    sp, x0

    /* Setup EL2: return address = el2_entry */
    adr    x1, el2_entry
    msr    ELR_EL3, x1
    /* SPSR_EL3: EL2h mode, DAIF masked */
    mov    x0, #0x3C9
    msr    SPSR_EL3, x0
    eret

/* ===================================================================
 * EL2 entry — configure and drop to EL1
 * =================================================================== */
el2_entry:
    mrs    x0, CurrentEL
    and    x0, x0, #0xC
    cmp    x0, #0x8
    b.ne   el2_entry

    /* Set VBAR_EL2 */
    adr    x0, _exception_vector
    msr    VBAR_EL2, x0

    /* HCR_EL2: everything routed to EL2 except... simplify: stage-2 off */
    mov    x0, #0
    msr    HCR_EL2, x0

    /* SCTLR_EL2: MMU off, caches off */
    mov    x0, #0
    msr    SCTLR_EL2, x0
    isb

    /* CNTHCTL_EL2: allow EL1 to access physical timer */
    mov    x0, #3
    msr    CNTHCTL_EL2, x0

    /* Set SP_EL2 */
    ldr    x0, =_el2_stack_top
    mov    sp, x0

    /* Setup EL1: return address = el1_entry */
    adr    x1, el1_entry
    msr    ELR_EL2, x1
    /* SPSR_EL2: EL1h mode */
    mov    x0, #0x3C5
    msr    SPSR_EL2, x0
    eret

/* ===================================================================
 * EL1 entry — set up stack, clear BSS, call C code
 * =================================================================== */
el1_entry:
    /* Set VBAR_EL1 */
    adr    x0, _exception_vector
    msr    VBAR_EL1, x0

    /* SCTLR_EL1: MMU off, I-cache off, D-cache off, little-endian */
    mov    x0, #0
    msr    SCTLR_EL1, x0
    isb

    /* Set SP_EL1 */
    ldr    x0, =_stack_top
    mov    sp, x0

    /* Clear BSS */
    ldr    x0, =__bss_start
    ldr    x1, =__bss_end
    sub    x1, x1, x0
    cbz    x1, 1f
    mov    x2, #0
0:  str    x2, [x0], #8
    subs   x1, x1, #8
    b.gt   0b
1:

    /* Enable interrupts and call main() */
    msr    DAIFClr, #0xF       /* clear DAIF masks */
    bl     main

_panic:
    wfi
    b      _panic

/* ===================================================================
 * Stub exception handlers — spin forever
 * =================================================================== */
el3_sync_sp0:      b _panic
el3_irq_sp0:       b _panic
el3_fiq_sp0:       b _panic
el3_serr_sp0:      b _panic
el3_sync_spx:      b _panic
el3_irq_spx:       b _panic
el3_fiq_spx:       b _panic
el3_serr_spx:      b _panic
el3_sync_aarch64:  b _panic
el3_fiq_aarch64:   b _panic
el3_serr_aarch64:  b _panic
el3_sync_aarch32:  b _panic
el3_irq_aarch32:   b _panic
el3_fiq_aarch32:   b _panic
el3_serr_aarch32:  b _panic

/* ---- EL1 IRQ handler (called from vector table) ---- */
.global arm64_irq_handler
el3_irq_aarch64:
    stp    x0, x1, [sp, #-16]!
    stp    x2, x3, [sp, #-16]!
    stp    x4, x5, [sp, #-16]!
    stp    x6, x7, [sp, #-16]!
    stp    x8, x9, [sp, #-16]!
    stp    x10, x11, [sp, #-16]!
    stp    x12, x13, [sp, #-16]!
    stp    x14, x15, [sp, #-16]!
    stp    x16, x17, [sp, #-16]!
    stp    x18, x19, [sp, #-16]!
    stp    x20, x21, [sp, #-16]!
    stp    x22, x23, [sp, #-16]!
    stp    x24, x25, [sp, #-16]!
    stp    x26, x27, [sp, #-16]!
    stp    x28, x29, [sp, #-16]!
    stp    x30, xzr, [sp, #-16]!

    bl     irq_handler

    ldp    x30, xzr, [sp], #16
    ldp    x28, x29, [sp], #16
    ldp    x26, x27, [sp], #16
    ldp    x24, x25, [sp], #16
    ldp    x22, x23, [sp], #16
    ldp    x20, x21, [sp], #16
    ldp    x18, x19, [sp], #16
    ldp    x16, x17, [sp], #16
    ldp    x14, x15, [sp], #16
    ldp    x12, x13, [sp], #16
    ldp    x10, x11, [sp], #16
    ldp    x8, x9, [sp], #16
    ldp    x6, x7, [sp], #16
    ldp    x4, x5, [sp], #16
    ldp    x2, x3, [sp], #16
    ldp    x0, x1, [sp], #16
    eret

// ------------------------------------------------------------

  //uint32_t get_cpu_id(void);
  .type get_cpu_id, "function"
  .cfi_startproc
  .global get_cpu_id
get_cpu_id:
  MRS      x0, MPIDR_EL1
  UBFX     x1, x0, #32, #8
  BFI      w0, w1, #24, #8
  RET
  .cfi_endproc
