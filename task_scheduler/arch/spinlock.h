//----------------------------------------------------------------
// Cortex-R52 spinlock (implemented in arch/spinlock.S)
//
// Dual-core mutual exclusion. The lock word must be in Normal
// Shareable memory (MPU) so LDREX/STREX works across both cores.
//
// Usage:
//
//     static spinlock q_lock = SPINLOCK_INIT;
//
//     spin_lock(&q_lock);            // busy-wait until acquired
//     ... critical section ...
//     spin_unlock(&q_lock);
//
// For a critical section also entered from an ISR on the same
// core, mask IRQ while holding the lock:
//
//     flags = spin_lock_irqsave(&q_lock);
//     ... critical section ...
//     spin_unlock_irqrestore(&q_lock, flags);
//
// The lock is not recursive and must not be held across a
// context switch or long waits (another core may spin forever).
//----------------------------------------------------------------

#ifndef SPINLOCK_H_
#define SPINLOCK_H_

#include <stdint.h>

typedef struct {
    volatile uint32_t lock;    /* 0 = free, 1 = held */
} spinlock;

#define SPINLOCK_INIT { 0 }

void     spin_lock_init(spinlock *l);
void     spin_lock(spinlock *l);                    /* blocking acquire */
int      spin_trylock(spinlock *l);                 /* 1 = acquired, 0 = busy */
void     spin_unlock(spinlock *l);
uint32_t spin_lock_irqsave(spinlock *l);            /* acquire + mask IRQ, returns flags */
void     spin_unlock_irqrestore(spinlock *l, uint32_t flags);

#endif /* SPINLOCK_H_ */
