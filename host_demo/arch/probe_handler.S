/*
 * probe_handler.S - Safe exception handler for GICR memory probing
 *
 * Provides a minimal AArch64 exception vector table (2KB aligned)
 * where the EL1h sync handler catches data aborts and signals
 * the fault via x18 instead of halting.
 *
 * Protocol:
 *   1. Save original VBAR_EL1 and x18
 *   2. Set VBAR_EL1 = probe_vector_table
 *   3. Before each probe access:  "mov x18, xzr"
 *   4. Perform LDR/STR to candidate address
 *   5. After access: read x18
 *      - x18 == 0 → no fault, data valid
 *      - x18 == 1 → data abort caught, instruction skipped
 *   6. Restore VBAR_EL1 and x18
 */

.section .text.probe_handler, "ax"
.balign 0x800                     /* 2KB alignment required by VBAR_EL1 */
.global probe_vector_table
.type probe_vector_table, %object

probe_vector_table:

/*------------------------------------------------------------------------------
 * Entry 0: Current EL with SP0 - Synchronous (offset 0x000)
 *----------------------------------------------------------------------------*/
.balign 0x80
    eret
.fill 31, 4, 0xd503201f          /* NOP padding to 128 bytes = 32 instructions */

/*------------------------------------------------------------------------------
 * Entry 1: Current EL with SP0 - IRQ (offset 0x080)
 *----------------------------------------------------------------------------*/
.balign 0x80
    eret
.fill 31, 4, 0xd503201f

/*------------------------------------------------------------------------------
 * Entry 2: Current EL with SP0 - FIQ (offset 0x100)
 *----------------------------------------------------------------------------*/
.balign 0x80
    eret
.fill 31, 4, 0xd503201f

/*------------------------------------------------------------------------------
 * Entry 3: Current EL with SP0 - SError (offset 0x180)
 *----------------------------------------------------------------------------*/
.balign 0x80
    eret
.fill 31, 4, 0xd503201f

/*------------------------------------------------------------------------------
 * Entry 4: Current EL with SPx - Synchronous (offset 0x200)
 *
 * Custom handler: catches data aborts and signals via x18.
 *   x18 == 0 → no fault happened
 *   x18 == 1 → data abort caught, faulting instruction was skipped
 *
 * If the exception is NOT a data abort, we simply eret.
 * (The only other sync exception during probing would be a bug.)
 *----------------------------------------------------------------------------*/
.balign 0x80
    stp x9,  x10, [sp, #-16]!     /* Save scratch regs to stack */
    mrs x9, esr_el1               /* Exception Syndrome Register */
    lsr x9, x9, #26               /* Extract Exception Class [31:26] */
    cmp x9, #0x24                 /* EC = Data Abort from current EL */
    b.eq 1f
    cmp x9, #0x25                 /* EC = Data Abort from lower EL */
    b.eq 1f
    ldp x9,  x10, [sp], #16      /* Restore scratch regs */
    eret                           /* Unknown exception: just return */
1:
    mov x18, #1                    /* Signal fault to probe code */
    mrs x9, elr_el1               /* Get return address (faulting insn) */
    add x9, x9, #4                /* Skip the faulting instruction */
    msr elr_el1, x9
    isb
    ldp x9,  x10, [sp], #16      /* Restore scratch regs */
    eret
.fill 16, 4, 0xd503201f          /* NOP pad: 16 code + 16 fill = 32 instr = 128 bytes */

/*------------------------------------------------------------------------------
 * Entries 5-15: Remaining handlers (offset 0x280 - 0x780)
 *   SPx: IRQ, FIQ, SError
 *   Lower EL AArch64: Sync, IRQ, FIQ, SError
 *   Lower EL AArch32: Sync, IRQ, FIQ, SError
 *----------------------------------------------------------------------------*/
.rept 11
.balign 0x80
    eret
.fill 31, 4, 0xd503201f
.endr

.global probe_vector_table_end
.type probe_vector_table_end, %object
probe_vector_table_end:
