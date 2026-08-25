#ifndef __SYSTEM_H__
#define __SYSTEM_H__

#include <stdio.h>
#include <stdint.h>
#include "platform.h"
#include "macro.h"

/* system tick in milliseconds: 与 configTICK_RATE_HZ(1000) 一致,即 1ms */
#define SYSTEM_TICK_MS      1 //ms

/* system ticks from startup */
extern volatile uint64_t g_sys_ticks;

/* uart configuration */
typedef struct {
    uint8_t     data_bits;
    uint8_t     stop_bits;
    uint8_t     parity;
    uint32_t    baudrate;
} uart_config;

/* interrupt prioprity */
typedef enum {
    INT_PRIO_LOWEST   = 0xFF,
    INT_PRIO_LOW      = 0xBF,
    INT_PRIO_NORMAL   = 0x7F,
    INT_PRIO_HIGH     = 0x3F,
    INT_PRIO_HIGHEST  = 0x00,
} INT_PRIORITY;

/* event definition (mhu mbx doorbell channel 0) */
#define MHU_DB0_EVENT_TS_STARTUP        (1 << 0)
#define MHU_DB0_EVENT_KERNEL_COMPLETE   (1U << 31)
#define MHU_DB0_EVENT_CMD_ACK           (1U << 30)

/* consume-ACK 通过 fast channel 0 承载:门铃中断/门铃位清除在本模型不可靠
 * (残留 bit30 令 wait_event 假阳性连发覆盖 FC 单值通知),而 FC 通道双向
 * 均可靠。命令 FC 值 = (off<<16)|dwlen < 0x10000,此魔数不会与命令冲突。 */
#define MHU_FC0_ACK_VALUE               (0x41434B00u)

/* event definition (mhu pbx doorbell channel 0) */
#define MHU_DB0_EVENT_HOST_STARTUP      (1 << 0)

/* all possible events */
#define MHU_DB_EVENT_ALL                0xFFFFFFFF

/* arch operation */
uint32_t get_cpu_id(void);
void arch_timer_init(uint32_t tick_ms);
void arch_timer_isr(void);
void arch_delay_us(uint32_t count);
uint64_t arch_get_time_ns(void);

static inline uint64_t arch_local_irq_save(void)
{
    uint64_t flags;
    __asm__ volatile(
        "mrs %0, daif \n"
        "msr daifset, #3"
        : "=r" (flags)
        :
        : "memory", "cc"
    );
    return flags;
}

static inline void arch_local_irq_restore(uint64_t flags)
{
    __asm__ volatile(
        "msr daif, %0"
        :
        : "r" (flags)
        : "memory", "cc"
    );
}

/* interrupt operation */
void interrupt_enable(int irq, INT_PRIORITY priority);
void interrupt_disable(int irq);

/* uart operation */
int uart_configure(uint32_t uart_base, uart_config* config);
void uart_putchar(uint32_t uart_base, char c);
void uart_write(uint32_t uart_base, const char* data);
int uart_getchar(uint32_t uart_base, char* c);

/* mhu operation */
int mhu_init(void);
void mhu_pbx_isr(void);
void mhu_mbx_isr(void);
void mhu_poll_rx(void);
void mhu_send_event(uint32_t ch, uint32_t event);
uint32_t mhu_wait_event(uint32_t ch, uint32_t event);
uint32_t mhu_send_data(uint32_t ch, void *data_ptr, uint32_t data_len);
uint32_t mhu_recv_data(uint32_t ch, void *buf_ptr, uint32_t buf_len);

uint32_t mhu_rx_data_fill(uint32_t ch);


typedef void (*irq_callback_t)(uint32_t irq, uint32_t channel);
/* mhu callback */
/* 回调类型 callback_type  0 - doorbell 1 - fast channel 2 - fifo channel */
/* 回调函数 callback */
void mhu_set_irq_callback(uint32_t callback_type, irq_callback_t callback);

extern volatile uint32_t g_mbx_db_stat_0;
extern volatile uint32_t g_mbx_ff_stat_0;
extern volatile uint32_t g_mbx_fc_stat_0;
extern volatile uint32_t g_mbx_fc_data_0[32];
#define MHU_DB_SIGNALED     (g_mbx_db_stat_0 != 0)
#define MHU_FF_SIGNALED     (g_mbx_ff_stat_0 != 0)
#define MHU_FC_SIGNALED     (g_mbx_fc_stat_0 != 0)


/* debug print */
int ts_printf(const char *fmt, ...);

/* misc functions */
#define delay_us(us)        arch_delay_us(us)
#define delay_ms(ms)        arch_delay_us(ms * 1000)

/* tests */
int run_test(void);

#endif
