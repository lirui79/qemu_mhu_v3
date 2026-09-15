//----------------------------------------------------------------
// Cortex-R52 atomic operations
//
// Implemented with the LDREX/STREX exclusive-access instructions
// (LDREXD/STREXD for 64-bit operations), so each read-modify-write
// sequence is atomic against interrupts on the local core and
// against concurrent access from other cores.
//
// Usage notes:
//  - The operand must be Normal memory. For cross-core operation
//    it must also be marked Shareable in the MPU.
//  - 32-bit operations require 4-byte alignment, 64-bit operations
//    require 8-byte alignment.
//  - The arithmetic/bit read-modify-write functions return the OLD
//    value in r0 (r0:r1 for 64-bit). Compare-and-swap returns
//    1 on success, 0 on failure.
//  - These operations are relaxed with respect to memory ordering.
//    Pair them with DMB/DSB when ordering with other accesses
//    matters, e.g. around a spinlock.
//----------------------------------------------------------------

    .text

// Unified syntax: lets the file assemble in ARM or Thumb-2 mode
// (LDREX/STREX need Thumb-2 encodings, there is no Thumb-16 form)
    .syntax unified

#ifdef __THUMB__
    .thumb
#endif

//----------------------------------------------------------------
// atomic_get(p)  -> *p, plain (non-exclusive) load
//----------------------------------------------------------------
    .global atomic_get
    .type   atomic_get, "function"
    .cfi_startproc
    .align  2
atomic_get:
    LDR     r0, [r0]
    BX      lr
    .cfi_endproc

//----------------------------------------------------------------
// atomic_set(p, val)  -> *p = val, plain (non-exclusive) store
//----------------------------------------------------------------
    .global atomic_set
    .type   atomic_set, "function"
    .cfi_startproc
    .align  2
atomic_set:
    STR     r1, [r0]
    BX      lr
    .cfi_endproc

//----------------------------------------------------------------
// atomic_inc(p)  -> *p += 1, returns the old value
//----------------------------------------------------------------
    .global atomic_inc
    .type   atomic_inc, "function"
    .cfi_startproc
    .align  2
atomic_inc:
inc_loop:
    LDREX   r3, [r0]                // load current value (exclusive)
    ADD     r2, r3, #1              // new value
    STREX   r1, r2, [r0]            // try to store; r1 = status
    CMP     r1, #0
    BNE     inc_loop                // lost exclusivity, retry
    MOV     r0, r3                  // return old value
    BX      lr
    .cfi_endproc

//----------------------------------------------------------------
// atomic_dec(p)  -> *p -= 1, returns the old value
//----------------------------------------------------------------
    .global atomic_dec
    .type   atomic_dec, "function"
    .cfi_startproc
    .align  2
atomic_dec:
dec_loop:
    LDREX   r3, [r0]
    SUB     r2, r3, #1
    STREX   r1, r2, [r0]
    CMP     r1, #0
    BNE     dec_loop
    MOV     r0, r3
    BX      lr
    .cfi_endproc

//----------------------------------------------------------------
// atomic_add(p, val)  -> *p += val, returns the old value
//----------------------------------------------------------------
    .global atomic_add
    .type   atomic_add, "function"
    .cfi_startproc
    .align  2
atomic_add:
add_loop:
    LDREX   r3, [r0]                // r3 = old value
    ADD     r2, r3, r1              // r2 = new value
    STREX   r12, r2, [r0]           // r12 = status
    CMP     r12, #0
    BNE     add_loop
    MOV     r0, r3                  // return old value
    BX      lr
    .cfi_endproc

//----------------------------------------------------------------
// atomic_sub(p, val)  -> *p -= val, returns the old value
//----------------------------------------------------------------
    .global atomic_sub
    .type   atomic_sub, "function"
    .cfi_startproc
    .align  2
atomic_sub:
sub_loop:
    LDREX   r3, [r0]
    SUB     r2, r3, r1
    STREX   r12, r2, [r0]
    CMP     r12, #0
    BNE     sub_loop
    MOV     r0, r3
    BX      lr
    .cfi_endproc

//----------------------------------------------------------------
// atomic_or(p, val)  -> *p |= val, returns the old value
//----------------------------------------------------------------
    .global atomic_or
    .type   atomic_or, "function"
    .cfi_startproc
    .align  2
atomic_or:
or_loop:
    LDREX   r3, [r0]
    ORR     r2, r3, r1
    STREX   r12, r2, [r0]
    CMP     r12, #0
    BNE     or_loop
    MOV     r0, r3
    BX      lr
    .cfi_endproc

//----------------------------------------------------------------
// atomic_and(p, val)  -> *p &= val, returns the old value
//----------------------------------------------------------------
    .global atomic_and
    .type   atomic_and, "function"
    .cfi_startproc
    .align  2
atomic_and:
and_loop:
    LDREX   r3, [r0]
    AND     r2, r3, r1
    STREX   r12, r2, [r0]
    CMP     r12, #0
    BNE     and_loop
    MOV     r0, r3
    BX      lr
    .cfi_endproc

//----------------------------------------------------------------
// atomic_xor(p, val)  -> *p ^= val, returns the old value
//----------------------------------------------------------------
    .global atomic_xor
    .type   atomic_xor, "function"
    .cfi_startproc
    .align  2
atomic_xor:
xor_loop:
    LDREX   r3, [r0]
    EOR     r2, r3, r1
    STREX   r12, r2, [r0]
    CMP     r12, #0
    BNE     xor_loop
    MOV     r0, r3
    BX      lr
    .cfi_endproc

//----------------------------------------------------------------
// atomic_swap(p, val)  -> exchange, returns the old value
//----------------------------------------------------------------
    .global atomic_swap
    .type   atomic_swap, "function"
    .cfi_startproc
    .align  2
atomic_swap:
swap_loop:
    LDREX   r3, [r0]                // r3 = old value
    STREX   r12, r1, [r0]           // store val
    CMP     r12, #0
    BNE     swap_loop
    MOV     r0, r3                  // return old value
    BX      lr
    .cfi_endproc

//----------------------------------------------------------------
// atomic_cas(p, expected, desired)
// If *p == expected: *p = desired and return 1, otherwise return 0
// (the exclusive monitor is cleared on failure)
//----------------------------------------------------------------
    .global atomic_cas
    .type   atomic_cas, "function"
    .cfi_startproc
    .align  2
atomic_cas:
cas_loop:
    LDREX   r3, [r0]                // r3 = current value
    CMP     r3, r1                  // matches expected?
    BNE     cas_fail
    STREX   r12, r2, [r0]           // store desired
    CMP     r12, #0
    BNE     cas_loop                // contention, retry
    MOV     r0, #1                  // success
    BX      lr
cas_fail:
    CLREX                           // clear the local monitor
    MOV     r0, #0                  // failure
    BX      lr
    .cfi_endproc

//----------------------------------------------------------------
// atomic_tas(p)  -> test-and-set: *p = 1, returns the old value
// (spinlock acquire: loop while the old value is non-zero)
//----------------------------------------------------------------
    .global atomic_tas
    .type   atomic_tas, "function"
    .cfi_startproc
    .align  2
atomic_tas:
    MOV     r2, #1
tas_loop:
    LDREX   r3, [r0]                // r3 = old value
    STREX   r12, r2, [r0]           // try to store 1
    CMP     r12, #0
    BNE     tas_loop
    MOV     r0, r3                  // return old value
    BX      lr
    .cfi_endproc

//----------------------------------------------------------------
// atomic_set_bit(p, bit)  -> *p |= (1 << bit), returns the old value
//----------------------------------------------------------------
    .global atomic_set_bit
    .type   atomic_set_bit, "function"
    .cfi_startproc
    .align  2
atomic_set_bit:
sb_loop:
    LDREX   r3, [r0]                // r3 = old value
    MOV     r2, #1
    LSL     r12, r2, r1             // r12 = bit mask
    ORR     r12, r3, r12            // new value
    STREX   r2, r12, [r0]           // r2 = status
    CMP     r2, #0
    BNE     sb_loop
    MOV     r0, r3                  // return old value
    BX      lr
    .cfi_endproc

//----------------------------------------------------------------
// atomic_clear_bit(p, bit)  -> *p &= ~(1 << bit), returns the old value
//----------------------------------------------------------------
    .global atomic_clear_bit
    .type   atomic_clear_bit, "function"
    .cfi_startproc
    .align  2
atomic_clear_bit:
cb_loop:
    LDREX   r3, [r0]
    MOV     r2, #1
    LSL     r12, r2, r1             // r12 = bit mask
    BIC     r12, r3, r12            // new value
    STREX   r2, r12, [r0]
    CMP     r2, #0
    BNE     cb_loop
    MOV     r0, r3
    BX      lr
    .cfi_endproc

//----------------------------------------------------------------
// atomic_toggle_bit(p, bit)  -> *p ^= (1 << bit), returns the old value
//----------------------------------------------------------------
    .global atomic_toggle_bit
    .type   atomic_toggle_bit, "function"
    .cfi_startproc
    .align  2
atomic_toggle_bit:
tb_loop:
    LDREX   r3, [r0]
    MOV     r2, #1
    LSL     r12, r2, r1             // r12 = bit mask
    EOR     r12, r3, r12            // new value
    STREX   r2, r12, [r0]
    CMP     r2, #0
    BNE     tb_loop
    MOV     r0, r3
    BX      lr
    .cfi_endproc

//----------------------------------------------------------------
// 64-bit operations (LDREXD/STREXD, 8-byte alignment required)
// Arguments and results follow AAPCS:
//   r0:r1  64-bit result, r2:r3  64-bit value argument
//----------------------------------------------------------------

//----------------------------------------------------------------
// atomic64_add(p, val)  -> *p += val, returns the old value (r0:r1)
//----------------------------------------------------------------
    .global atomic64_add
    .type   atomic64_add, "function"
    .cfi_startproc
    .align  2
atomic64_add:
    PUSH    {r4-r7}
a64_add_loop:
    LDREXD  r4, r5, [r0]            // r4:r5 = old value (low:high)
    ADDS    r6, r4, r2              // r6 = new low
    ADC     r7, r5, r3              // r7 = new high
    STREXD  r12, r6, r7, [r0]       // try to store; r12 = status
    CMP     r12, #0
    BNE     a64_add_loop
    MOV     r0, r4                  // return old low
    MOV     r1, r5                  // return old high
    POP     {r4-r7}
    BX      lr
    .cfi_endproc

//----------------------------------------------------------------
// atomic64_sub(p, val)  -> *p -= val, returns the old value (r0:r1)
//----------------------------------------------------------------
    .global atomic64_sub
    .type   atomic64_sub, "function"
    .cfi_startproc
    .align  2
atomic64_sub:
    PUSH    {r4-r7}
a64_sub_loop:
    LDREXD  r4, r5, [r0]            // r4:r5 = old value (low:high)
    SUBS    r6, r4, r2              // r6 = new low
    SBC     r7, r5, r3              // r7 = new high
    STREXD  r12, r6, r7, [r0]       // try to store; r12 = status
    CMP     r12, #0
    BNE     a64_sub_loop
    MOV     r0, r4                  // return old low
    MOV     r1, r5                  // return old high
    POP     {r4-r7}
    BX      lr
    .cfi_endproc

//----------------------------------------------------------------
// atomic64_inc(p)  -> *p += 1, returns the old value (r0:r1)
//----------------------------------------------------------------
    .global atomic64_inc
    .type   atomic64_inc, "function"
    .cfi_startproc
    .align  2
atomic64_inc:
    MOV     r2, #1
    MOV     r3, #0
    B       atomic64_add            // tail call
    .cfi_endproc

//----------------------------------------------------------------
// atomic64_dec(p)  -> *p -= 1, returns the old value (r0:r1)
//----------------------------------------------------------------
    .global atomic64_dec
    .type   atomic64_dec, "function"
    .cfi_startproc
    .align  2
atomic64_dec:
    MOV     r2, #1
    MOV     r3, #0
    B       atomic64_sub            // tail call
    .cfi_endproc

//----------------------------------------------------------------
// atomic64_cas(p, expected, desired)
// If *p == expected: *p = desired and return 1, otherwise return 0
// (the exclusive monitor is cleared on failure)
//----------------------------------------------------------------
    .global atomic64_cas
    .type   atomic64_cas, "function"
    .cfi_startproc
    .align  2
atomic64_cas:
    PUSH    {r4-r7}
    MOV     r6, r3                  // desired low  (STREXD source pair
    MOV     r7, r4                  // desired high  must start at an even register)
a64_cas_loop:
    LDREXD  r4, r5, [r0]            // r4:r5 = current value (low:high)
    CMP     r4, r1                  // compare low halves
    BNE     a64_cas_fail
    CMP     r5, r2                  // compare high halves
    BNE     a64_cas_fail
    STREXD  r12, r6, r7, [r0]       // try to store desired
    CMP     r12, #0
    BNE     a64_cas_loop            // contention, retry
    MOV     r0, #1                  // success
    POP     {r4-r7}
    BX      lr
a64_cas_fail:
    CLREX                           // clear the local monitor
    MOV     r0, #0                  // failure
    POP     {r4-r7}
    BX      lr
    .cfi_endproc
