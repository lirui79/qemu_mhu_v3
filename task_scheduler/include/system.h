#ifndef __SYSTEM_H__
#define __SYSTEM_H__

#include <stdint.h>
#include "cmdef.h"
#include "platform.h"
#include "macro.h"
#include "atomic.h"
#include "spinlock.h"
#include "gicv3_basic.h"
#include "uart_pl011.h"
#include "mhu_davarae.h"

/* ------------------------------------------------------------------ */
/* Error Codes                                                        */
/* ------------------------------------------------------------------ */

#define TS_OK                   0       /* success                  */
#define TS_ERR_GENERIC          1       /* generic error            */
#define TS_ERR_INVALID_ARG      2       /* invalid argument         */
#define TS_ERR_TIMEOUT          3       /* operation timed out      */
#define TS_ERR_BUSY             4       /* resource is busy         */
#define TS_ERR_NOMEM            5       /* out of memory            */
#define TS_ERR_NOT_FOUND        6       /* resource not found       */
#define TS_ERR_NOT_INIT         7       /* not initialized          */
#define TS_ERR_NOT_SUPPORTED    8       /* not supported            */
#define TS_ERR_EXISTS           9       /* already exists           */
#define TS_ERR_EMPTY            10      /* queue/buffer is empty    */
#define TS_ERR_FULL             11      /* queue/buffer is full     */
#define TS_ERR_IO               12      /* io error                 */
#define TS_ERR_UNALIGNED        13      /* unaligned error          */
#define TS_ERR_DATA             14      /* data error               */
#define TS_ERR_SIZE             15      /* size error               */
#define TS_ERR_ADDR             16      /* address error            */
#define TS_ERR_PROCESS          17      /* process error            */
#define TS_ERR_QUEUE            18      /* queue error              */

/* ------------------------------------------------------------------ */
/* Debug Support                                                      */
/* ------------------------------------------------------------------ */

/* Define default print level */
#define TS_PRINT_ERR            0
#define TS_PRINT_WARN           1
#define TS_PRINT_INFO           2
#define TS_PRINT_DBG            3
#define TS_PRINT_DEFAULT        TS_PRINT_INFO

/**
 * @brief  Print a formatted string to the UART of the current CPU
 * @param  fmt: format string, supports %d/%i/%u/%x/%U/%X/%s/%c/%%
 * @return 0 for success
 */
int ts_printf(const char *fmt, ...);

/**
 * @brief  Assert a condition with standard assert semantics
 * @param  cond: condition that must be true
 * @note   On failure the condition and its location are printed and the
 *         CPU hangs forever (bare metal has no abort())
 */
#define ts_assert(cond)                                     \
    do {                                                    \
        if (!(cond)) {                                      \
            ts_printf("Assertion failed: %s at %s:%d\n",    \
                      #cond, __FILE__, __LINE__);           \
            while (1);                                      \
        }                                                   \
    } while (0)

/* error message */
#define ts_err(f, ...)          ts_printf(f, ##__VA_ARGS__)

/* warning message */
#if (TS_PRINT_DEFAULT > TS_PRINT_ERR)
#define ts_warn(f, ...)         ts_printf(f, ##__VA_ARGS__)
#else
#define ts_warn(f, ...)
#endif

/* information message */
#if (TS_PRINT_DEFAULT > TS_PRINT_WARN)
#define ts_info(f, ...)         ts_printf(f, ##__VA_ARGS__)
#else
#define ts_info(f, ...)
#endif

/* debug message */
#if (TS_PRINT_DEFAULT > TS_PRINT_INFO)
#define ts_dbg(f, ...)          ts_printf(f, ##__VA_ARGS__)
#else
#define ts_dbg(f, ...)
#endif

/* ------------------------------------------------------------------ */
/* System tick (every core has its own tick)                          */
/* ------------------------------------------------------------------ */

/* system tick in milliseconds */
#define SYSTEM_TICK_MS_0        1000
#define SYSTEM_TICK_MS_1        1000

/* ------------------------------------------------------------------ */
/* Interupt priority                                                  */
/* ------------------------------------------------------------------ */

/* interrupt prioprity */
typedef enum {
    INT_PRIO_LOWEST   = 0xFF,
    INT_PRIO_LOW      = 0xBF,
    INT_PRIO_NORMAL   = 0x7F,
    INT_PRIO_HIGH     = 0x3F,
    INT_PRIO_HIGHEST  = 0x00,
} INT_PRIORITY;

/* ------------------------------------------------------------------ */
/* Arch Operation                                                     */
/* ------------------------------------------------------------------ */

/**
 * @brief  Return the ID of the current CPU
 * @return CPU ID (MPIDR affinity level 0)
 */
unsigned int get_cpu_id(void);

/**
 * @brief  Initialize the system tick timer with the given period
 * @param  tick_ms: tick period in milliseconds
 * @return None
 */
void arch_timer_init(uint32_t tick_ms);

/**
 * @brief  System tick timer interrupt handler
 * @return None
 */
void arch_timer_isr(void);

/**
 * @brief  Busy-wait delay in microseconds
 * @param  count: delay time in microseconds
 * @return None
 */
void arch_delay_us(uint32_t count);

#define delay_us(us)        arch_delay_us(us)
#define delay_ms(ms)        arch_delay_us(ms * 1000)

/**
 * @brief  Return the time elapsed since startup in nanoseconds
 * @return time in nanoseconds
 */
uint64_t arch_get_time_ns(void);

/**
 * @brief  Return the time elapsed since startup in microseconds
 * @return time in microseconds
 */
uint64_t arch_get_time_us(void);

/**
 * @brief  Return the time elapsed since startup in milliseconds
 * @return time in milliseconds
 */
uint64_t arch_get_time_ms(void);

/**
 * @brief  Check whether the given deadline has already passed
 * @param  expire_ms: absolute deadline in milliseconds
 * @return non-zero if the current time is after expire_ms,
 *         zero if the deadline has not yet been reached
 */
int time_after(uint64_t expire_ms);

/**
 * @brief  Check whether the given deadline is still in the future
 * @param  expire_ms: absolute deadline in milliseconds
 * @return non-zero if the current time is before expire_ms,
 *         zero if the deadline has been reached or passed
 */
int time_before(uint64_t expire_ms);

/**
 * @brief  Disable interrupts and save the previous interrupt state
 * @return CPSR value to be passed to arch_local_irq_restore()
 */
static inline uint32_t arch_local_irq_save(void)
{
    uint32_t cpsr_val;
    __asm__ volatile(
        "mrs %0, cpsr \n"
        "cpsid if"
        : "=r" (cpsr_val)
        :
        : "memory", "cc"
    );
    return cpsr_val;
}

/**
 * @brief  Restore the interrupt state saved by arch_local_irq_save()
 * @param  cpsr_val: CPSR value returned by arch_local_irq_save()
 * @return None
 */
static inline void arch_local_irq_restore(uint32_t cpsr_val)
{
    __asm__ volatile(
        "msr cpsr_c, %0"
        :
        : "r" (cpsr_val)
        : "memory", "cc"
    );
}

/* ------------------------------------------------------------------ */
/* Cache Operation                                                    */
/* ------------------------------------------------------------------ */

/**
 * @brief  Enable the I and D caches
 * @return None
 */
void enable_caches(void);

/**
 * @brief  Flush and invalidate the whole D-cache
 * @return None
 */
void flush_dcache_all(void);

/**
 * @brief  Flush and invalidate D-cache lines of the given address range
 * @param  start: start address (inclusive)
 * @param  end: end address (exclusive)
 * @return None
 */
void flush_dcache_range(uint32_t start, uint32_t end);

/**
 * @brief  Invalidate the whole D-cache
 * @return None
 */
void invalidate_dcache_all(void);

/**
 * @brief  Invalidate D-cache lines of the given address range
 * @param  start: start address (inclusive)
 * @param  end: end address (exclusive)
 * @return None
 */
void invalidate_dcache_range(uint32_t start, uint32_t end);

/* ------------------------------------------------------------------ */
/* Interrupt Operation                                                */
/* ------------------------------------------------------------------ */

/**
 * @brief  Enable an interrupt and route it to the current CPU
 * @param  irq: interrupt ID
 * @param  priority: INT_PRIO_* interrupt priority
 * @return None
 */
void interrupt_enable(int irq, INT_PRIORITY priority);

/**
 * @brief  Disable an interrupt
 * @param  irq: interrupt ID
 * @return None
 */
void interrupt_disable(int irq);


typedef void (*irq_callback_t)(uint32_t irq, uint32_t channel);
/* mhu callback */
/* 回调类型 callback_type  0 - doorbell 1 - fast channel 2 - fifo channel */
/* 回调函数 callback */
void mhu_set_irq_callback(uint32_t callback_type, irq_callback_t callback);

#endif
