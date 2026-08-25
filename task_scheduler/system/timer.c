#include "system.h"
#include "generictimer.h"

static   uint32_t  g_tick_period = 0;
volatile uint64_t  g_sys_ticks   = 0;

static inline uint64_t get_sys_counter(void)
{
    uint64_t count = getCNTPCT();
    return count;
}

void arch_timer_init(uint32_t tick_ms)
{
    uint32_t freq = getCNTFRQ();
    g_tick_period = freq / 1000 * tick_ms;
    if (!g_tick_period)
        g_tick_period = 100000;

    ts_printf("Timer freq %u Hz, tick period %u ms\n", freq, tick_ms);

    /* set timer value */
    setCNTP_TVAL(g_tick_period);

    /* enable interrupt */
    interrupt_enable(IRQ_ID_PHY_TIMER, INT_PRIO_NORMAL);
    enablePhyTimer();
}

void arch_timer_isr(void)
{
    g_sys_ticks++;
    setCNTP_TVAL(g_tick_period);
}

void arch_delay_us(uint32_t count)
{
    volatile uint64_t dlycnt = (uint64_t)count * SYS_FREQ_MHZ;
    volatile uint64_t curcnt = get_sys_counter();
    volatile uint64_t expcnt = curcnt + dlycnt;
    if (expcnt < curcnt)
    {
        /* wrap on counter overflow */
        while (curcnt > expcnt)
        {
            curcnt = get_sys_counter();
        }
    }
    while (curcnt < expcnt)
    {
        curcnt = get_sys_counter();
    }
}

uint64_t arch_get_time_ns(void)
{
    return (get_sys_counter() * 1000 / SYS_FREQ_MHZ);
}
