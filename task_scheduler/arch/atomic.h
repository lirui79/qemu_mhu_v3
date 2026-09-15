//----------------------------------------------------------------
// Cortex-R52 atomic operations (implemented in arch/atomic.S)
//
// All read-modify-write operations use LDREX/STREX (LDREXD/STREXD
// for the 64-bit ones) and are atomic against interrupts on the
// local core and against concurrent access from other cores.
//
// Requirements:
//  - The operand must be Normal memory; for cross-core operation
//    it must be marked Shareable in the MPU.
//  - 32-bit operations need 4-byte alignment, 64-bit operations
//    need 8-byte alignment.
//
// Return values:
//  - The arithmetic/bit functions return the OLD value.
//  - atomic_cas / atomic64_cas return 1 on success, 0 on failure.
//
// Memory ordering:
//  - These operations are relaxed; pair them with DMB/DSB when the
//    result must be ordered with other accesses, e.g. a spinlock:
//
//      while (atomic_tas(&lock) != 0) ;        // acquire
//      __asm volatile("dmb ish" ::: "memory");
//      ... critical section ...
//      __asm volatile("dmb ish" ::: "memory"); // release
//      atomic_set(&lock, 0);
//----------------------------------------------------------------

#ifndef ATOMIC_H_
#define ATOMIC_H_

#include <stdint.h>

uint32_t atomic_get(volatile uint32_t *p);          // plain load
void     atomic_set(volatile uint32_t *p, uint32_t val); // plain store

uint32_t atomic_inc(volatile uint32_t *p);          // *p += 1, old
uint32_t atomic_dec(volatile uint32_t *p);          // *p -= 1, old
uint32_t atomic_add(volatile uint32_t *p, uint32_t val); // *p += val, old
uint32_t atomic_sub(volatile uint32_t *p, uint32_t val); // *p -= val, old
uint32_t atomic_or(volatile uint32_t *p, uint32_t val);  // *p |= val, old
uint32_t atomic_and(volatile uint32_t *p, uint32_t val); // *p &= val, old
uint32_t atomic_xor(volatile uint32_t *p, uint32_t val); // *p ^= val, old

uint32_t atomic_swap(volatile uint32_t *p, uint32_t val); // exchange, old
int      atomic_cas(volatile uint32_t *p, uint32_t expected, uint32_t desired);
uint32_t atomic_tas(volatile uint32_t *p);          // *p = 1, old (spinlock acquire)

uint32_t atomic_set_bit(volatile uint32_t *p, uint32_t bit);    // *p |= 1<<bit, old
uint32_t atomic_clear_bit(volatile uint32_t *p, uint32_t bit);  // *p &= ~(1<<bit), old
uint32_t atomic_toggle_bit(volatile uint32_t *p, uint32_t bit); // *p ^= 1<<bit, old

uint64_t atomic64_add(volatile uint64_t *p, uint64_t val); // *p += val, old
uint64_t atomic64_sub(volatile uint64_t *p, uint64_t val); // *p -= val, old
uint64_t atomic64_inc(volatile uint64_t *p);          // *p += 1, old
uint64_t atomic64_dec(volatile uint64_t *p);          // *p -= 1, old
int      atomic64_cas(volatile uint64_t *p, uint64_t expected, uint64_t desired);

#endif /* ATOMIC_H_ */
