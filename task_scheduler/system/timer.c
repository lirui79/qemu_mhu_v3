#include "system.h"
#include "generictimer.h"

static uint32_t g_tick_period_0 = 0;
static uint32_t g_tick_period_1 = 0;

static inline uint64_t get_sys_counter(void)
{
    uint64_t count = getCNTPCT();
    return count;
}

void arch_timer_init(uint32_t tick_ms)
{
    uint32_t freq = getCNTFRQ();
    uint32_t tick = freq / 1000 * tick_ms;

    ts_assert((tick > 0) && (tick_ms > 0));
    ts_info("Timer freq %u Hz, tick period %u ms\n", freq, tick_ms);

    if (get_cpu_id() == 0) {
        g_tick_period_0 = tick;
    } else {
        g_tick_period_1 = tick;
    }

    /* set timer value */
    setCNTP_TVAL(tick);

    /* enable interrupt */
    interrupt_enable(IRQ_ID_PHY_TIMER, INT_PRIO_NORMAL);
    enablePhyTimer();
}

void arch_timer_isr(void)
{
    if (get_cpu_id() == 0) {
        setCNTP_TVAL(g_tick_period_0);
    } else {
        setCNTP_TVAL(g_tick_period_1);
    }
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

uint64_t arch_get_time_us(void)
{
    return (get_sys_counter() / SYS_FREQ_MHZ);
}

uint64_t arch_get_time_ms(void)
{
    return (get_sys_counter() / SYS_FREQ_KHZ);
}

int time_after(uint64_t expire_ms)
{
    /* true if the current time has passed expire_ms (wrap safe) */
    if (arch_get_time_ms() > expire_ms) {
        return 1;
    }
    return 0;
}

int time_before(uint64_t expire_ms)
{
    /* true if the current time has not yet reached expire_ms (wrap safe) */
    if (arch_get_time_ms() > expire_ms) {
        return 0;
    }
    return 1;
}
