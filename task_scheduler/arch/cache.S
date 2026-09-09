//----------------------------------------------------------------
// Cortex-R52 data cache maintenance
//
//   flush_dcache_all / flush_dcache_range:     clean and invalidate
//   invalidate_dcache_all / invalidate_dcache_range: invalidate only
//
// Whole-cache operations are done by Set/Way (there is no
// "invalidate all" CP15 instruction on ARMv8-R AArch32), range
// operations by MVA, e.g. to make DMA buffers consistent.
//----------------------------------------------------------------

    .text

//----------------------------------------------------------------
// Apply a Set/Way operation to every data or unified cache line.
// r0: 0 = clean and invalidate (DCCISW), otherwise invalidate (DCISW)
//----------------------------------------------------------------
    .type   dcache_op_all, "function"
    .align  2
dcache_op_all:
    PUSH    {r4-r11}                // save callee-saved registers
    MOV     r8, r0                  // save op
    DSB                             // complete all prior memory accesses

    MRC     p15, 1, r0, c0, c0, 1   // read CLIDR
    MOV     r10, #0                 // current cache level << 1 (L1 first)

level_loop:
    ADD     r2, r10, r10, LSR #1    // 3 x level index = CLIDR bit position
    MOV     r1, r0, LSR r2
    AND     r1, r1, #7              // cache type at this level
    CMP     r1, #2
    BLT     level_next              // no cache, or instruction cache only

    MCR     p15, 2, r10, c0, c0, 0  // select this level in CSSELR
    ISB
    MRC     p15, 1, r1, c0, c0, 0   // read CCSIDR
    AND     r2, r1, #7
    ADD     r2, r2, #4              // r2 = line size, log2(bytes)
    LDR     r4, =0x3FF
    AND     r4, r4, r1, LSR #3      // r4 = number of ways - 1
    CLZ     r5, r4                  // r5 = bit position of the way field
    LDR     r7, =0x7FFF
    AND     r7, r7, r1, LSR #13     // r7 = number of sets - 1

set_loop:
    MOV     r9, r4                  // r9 = working copy of ways

way_loop:
    ORR     r11, r10, r9, LSL r5    // factor in level and way
    ORR     r11, r11, r7, LSL r2    // factor in set
    CMP     r8, #0
    MCREQ   p15, 0, r11, c7, c14, 2 // clean and invalidate (DCCISW)
    MCRNE   p15, 0, r11, c7, c6, 2  // invalidate only (DCISW)
    SUBS    r9, r9, #1              // decrement way
    BGE     way_loop
    SUBS    r7, r7, #1              // decrement set
    BGE     set_loop

level_next:
    ADD     r10, r10, #2            // next cache level
    CMP     r10, #14                // 7 levels
    BLT     level_loop

    MOV     r0, #0
    MCR     p15, 2, r0, c0, c0, 0   // restore CSSELR to L1
    DSB
    ISB
    POP     {r4-r11}
    BX      lr

//----------------------------------------------------------------
// Flush (clean and invalidate) the entire D-cache
//----------------------------------------------------------------
    .global flush_dcache_all
    .type   flush_dcache_all, "function"
    .cfi_startproc
    .align  2
flush_dcache_all:
    PUSH    {lr}
    MOV     r0, #0
    BL      dcache_op_all
    POP     {pc}
    .cfi_endproc

//----------------------------------------------------------------
// Invalidate the entire D-cache
//----------------------------------------------------------------
    .global invalidate_dcache_all
    .type   invalidate_dcache_all, "function"
    .cfi_startproc
    .align  2
invalidate_dcache_all:
    PUSH    {lr}
    MOV     r0, #1
    BL      dcache_op_all
    POP     {pc}
    .cfi_endproc

//----------------------------------------------------------------
// Return the L1 data cache line size in bytes (r0)
//----------------------------------------------------------------
    .type   dcache_line_size, "function"
    .align  2
dcache_line_size:
    MOV     r0, #0
    MCR     p15, 2, r0, c0, c0, 0   // CSSELR = L1 data
    ISB
    MRC     p15, 1, r0, c0, c0, 0   // read CCSIDR
    AND     r0, r0, #7              // line size field
    MOV     r1, #16
    MOV     r0, r1, LSL r0          // line size = 16 << field
    BX      lr

//----------------------------------------------------------------
// Flush (clean and invalidate) D-cache lines over [start, end)
// r0 = start address, r1 = end address (exclusive)
//----------------------------------------------------------------
    .global flush_dcache_range
    .type   flush_dcache_range, "function"
    .cfi_startproc
    .align  2
flush_dcache_range:
    PUSH    {r4, r5, lr}
    CMP     r1, r0
    BLS     fr_done                 // nothing to do for an empty range
    MOV     r4, r0                  // start
    MOV     r5, r1                  // end
    BL      dcache_line_size        // r0 = line size
    SUB     r2, r0, #1
    BIC     r4, r4, r2              // align start down to a line boundary
    MOV     r2, r0                  // line size stride

fr_loop:
    MCR     p15, 0, r4, c7, c14, 1  // clean and invalidate line (DCCIMVAC)
    ADD     r4, r4, r2
    CMP     r4, r5
    BLO     fr_loop
    DSB

fr_done:
    POP     {r4, r5, pc}
    .cfi_endproc

//----------------------------------------------------------------
// Invalidate D-cache lines over [start, end); partially covered
// lines are cleaned first so that dirty data is not lost
// r0 = start address, r1 = end address (exclusive)
//----------------------------------------------------------------
    .global invalidate_dcache_range
    .type   invalidate_dcache_range, "function"
    .cfi_startproc
    .align  2
invalidate_dcache_range:
    PUSH    {r4, r5, r6, lr}
    CMP     r1, r0
    BLS     ir_done                 // nothing to do for an empty range
    MOV     r4, r0                  // start
    MOV     r5, r1                  // end
    BL      dcache_line_size        // r0 = line size
    MOV     r6, r0                  // line size
    SUB     r3, r6, #1

    TST     r4, r3                  // clean + invalidate partial line at start
    BIC     r4, r4, r3
    MCRNE   p15, 0, r4, c7, c14, 1  // DCCIMVAC

    TST     r5, r3                  // clean + invalidate partial line at end
    BIC     r5, r5, r3
    MCRNE   p15, 0, r5, c7, c14, 1  // DCCIMVAC

ir_loop:
    MCR     p15, 0, r4, c7, c6, 1   // invalidate line (DCIMVAC)
    ADD     r4, r4, r6
    CMP     r4, r5
    BLO     ir_loop
    DSB

ir_done:
    POP     {r4, r5, r6, pc}
    .cfi_endproc
