//----------------------------------------------------------------
// Cortex-R52 spinlock - dual-core mutual exclusion
//
// The lock word is 0 (free) / 1 (held). Acquisition uses
// LDREX/STREX so taking the lock is atomic across cores; release
// is a plain store. Memory ordering is enforced with DMB ISH
// (inner shareable): the critical section is ordered after the
// acquire and before the release.
//
// Requirements:
//  - The lock word must be in Normal Shareable memory (MPU) so
//    the exclusive monitor works across both cores.
//  - The lock is not recursive: the core holding it must not try
//    to take it again (deadlock).
//
// spin_lock / spin_trylock / spin_unlock
// spin_lock_irqsave / spin_unlock_irqrestore
//     variants that also mask/restore IRQ (I bit) on the local
//     core, for locks shared between main context and ISRs.
//----------------------------------------------------------------

    .text

// Unified syntax: lets the file assemble in ARM or Thumb-2 mode
    .syntax unified

#ifdef __THUMB__
    .thumb
#endif

//----------------------------------------------------------------
// spin_lock_init(l)  -> l->lock = 0
//----------------------------------------------------------------
    .global spin_lock_init
    .type   spin_lock_init, "function"
    .cfi_startproc
    .align  2
spin_lock_init:
    MOV     r1, #0
    STR     r1, [r0]
    BX      lr
    .cfi_endproc

//----------------------------------------------------------------
// spin_lock(l)  -> busy-wait until the lock is acquired
//----------------------------------------------------------------
    .global spin_lock
    .type   spin_lock, "function"
    .cfi_startproc
    .align  2
spin_lock:
    MOV     r1, #1
spl_retry:
    LDREX   r2, [r0]                // try to take the lock
    CMP     r2, #0
    BNE     spl_wait                // held by the other core
    STREX   r2, r1, [r0]            // r2 = status
    CMP     r2, #0
    BNE     spl_retry               // lost exclusivity, retry
    DMB     ish                     // acquire barrier: critical section
    BX      lr                      // ordered after taking the lock
spl_wait:
    LDR     r2, [r0]                // poll until free (WFE/SEV would be
    CMP     r2, #0                  // a power optimization, polling is
    BNE     spl_wait                // always correct)
    B       spl_retry
    .cfi_endproc

//----------------------------------------------------------------
// spin_trylock(l)  -> 1 if acquired, 0 if busy (non-blocking)
//----------------------------------------------------------------
    .global spin_trylock
    .type   spin_trylock, "function"
    .cfi_startproc
    .align  2
spin_trylock:
    MOV     r1, #1
spt_retry:
    LDREX   r2, [r0]
    CMP     r2, #0
    BNE     spt_fail                // already held
    STREX   r2, r1, [r0]
    CMP     r2, #0
    BNE     spt_retry               // exclusivity lost, one more attempt
    DMB     ish                     // acquire barrier
    MOV     r0, #1                  // acquired
    BX      lr
spt_fail:
    CLREX                           // clear the local monitor
    MOV     r0, #0                  // busy
    BX      lr
    .cfi_endproc

//----------------------------------------------------------------
// spin_unlock(l)  -> release the lock
//----------------------------------------------------------------
    .global spin_unlock
    .type   spin_unlock, "function"
    .cfi_startproc
    .align  2
spin_unlock:
    DMB     ish                     // release barrier: critical section
    MOV     r1, #0                  // ordered before releasing the lock
    STR     r1, [r0]
    BX      lr
    .cfi_endproc

//----------------------------------------------------------------
// spin_lock_irqsave(l)  -> acquire and mask IRQ (I bit) on this
// core; returns the old CPSR as flags
//----------------------------------------------------------------
    .global spin_lock_irqsave
    .type   spin_lock_irqsave, "function"
    .cfi_startproc
    .align  2
spin_lock_irqsave:
    MRS     r1, cpsr                // r1 = old CPSR (returned as flags)
    CPSID   i                       // mask IRQ on this core
    MOV     r2, #1
spi_retry:
    LDREX   r3, [r0]
    CMP     r3, #0
    BNE     spi_wait
    STREX   r3, r2, [r0]
    CMP     r3, #0
    BNE     spi_retry
    DMB     ish                     // acquire barrier
    MOV     r0, r1                  // return old CPSR
    BX      lr
spi_wait:
    LDR     r3, [r0]
    CMP     r3, #0
    BNE     spi_wait
    B       spi_retry
    .cfi_endproc

//----------------------------------------------------------------
// spin_unlock_irqrestore(l, flags)  -> release and restore the IRQ
// state saved by spin_lock_irqsave
//----------------------------------------------------------------
    .global spin_unlock_irqrestore
    .type   spin_unlock_irqrestore, "function"
    .cfi_startproc
    .align  2
spin_unlock_irqrestore:
    DMB     ish                     // release barrier
    MOV     r2, #0
    STR     r2, [r0]
    MSR     cpsr_c, r1              // restore IRQ state
    BX      lr
    .cfi_endproc
