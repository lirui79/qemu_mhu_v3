#ifndef __MACRO_H__
#define __MACRO_H__

#include <stdint.h>

#define REG32(addr)                 (*(volatile uint32_t *)(uintptr_t)(addr))
#define REG16(addr)                 (*(volatile uint16_t *)(uintptr_t)(addr))
#define REG8(addr)                  (*(volatile uint8_t *)(uintptr_t)(addr))
#define REG64(addr)                 (*(volatile uint64_t *)(uintptr_t)(addr))

#define WRITE32(addr, val)          (REG32(addr) = (uint32_t)(val))
#define WRITE16(addr, val)          (REG16(addr) = (uint16_t)(val))
#define WRITE8(addr, val)           (REG8(addr) = (uint8_t)(val))
#define WRITE64(addr, val)          (REG64(addr) = (uint64_t)(val))

#define READ32(addr)                (REG32(addr))
#define READ16(addr)                (REG16(addr))
#define READ8(addr)                 (REG8(addr))
#define READ64(addr)                (REG64(addr))

#define WRITE32_SYNC(addr, val) do { \
    REG32(addr) = (uint32_t)(val);   \
    __asm volatile("dsb sy" ::: "memory"); \
} while(0)

#define READ32_SYNC(addr) ({ \
    uint32_t __val;          \
    __asm volatile("dsb sy" ::: "memory"); \
    __val = REG32(addr);     \
    __asm volatile("isb" ::: "memory"); \
    __val;                   \
})

#define SET_BIT32(addr, mask)       (REG32(addr) |= (uint32_t)(mask))
#define CLEAR_BIT32(addr, mask)     (REG32(addr) &= ~(uint32_t)(mask))
#define TOGGLE_BIT32(addr, mask)    (REG32(addr) ^= (uint32_t)(mask))
#define READ_BITS32(addr, mask)     (REG32(addr) & (uint32_t)(mask))

#endif
