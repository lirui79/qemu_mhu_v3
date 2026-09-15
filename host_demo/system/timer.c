#include "system.h"

static   uint32_t  g_tick_period = 0;
volatile uint64_t  g_sys_ticks   = 0;

static inline uint64_t get_timer_freq(void)
{
    uint64_t freq;
    asm volatile("mrs %0, cntfrq_el0" : "=r" (freq));
    return freq;
}

static inline void set_timer_value(uint32_t delay_ticks)
{
    asm volatile("msr cntp_tval_el0, %0" : : "r" (delay_ticks));
}

static inline void enable_physical_timer(void)
{
    uint32_t ctl = 0;
    asm volatile("mrs %0, cntp_ctl_el0" : "=r" (ctl));
    ctl |= (1 << 0);   // ENABLE
    ctl &= ~(1 << 1);  // IMASK = 0
    asm volatile("msr cntp_ctl_el0, %0" : : "r" (ctl));
}

static inline uint64_t get_sys_counter(void)
{
    uint64_t count;
    asm volatile ("mrs %0, cntpct_el0" : "=r" (count));
    return count;
}

void arch_timer_init(uint32_t tick_ms)
{
    uint64_t freq = get_timer_freq();
    g_tick_period = freq / 1000 * tick_ms;
    if (!g_tick_period)
        g_tick_period = 100000;

    ts_printf("Timer freq %llu Hz, tick period %u ms\n", freq, tick_ms);

    /* set timer value */
    set_timer_value(g_tick_period);

    /* enable interrupt */
    interrupt_enable(IRQ_ID_PHY_TIMER, INT_PRIO_NORMAL);
    enable_physical_timer();
}

void arch_timer_isr(void)
{
    g_sys_ticks++;
    set_timer_value(g_tick_period);
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

/* FreeRTOS tick hooks required by the ARM_AARCH64 port.
 * See FreeRTOSConfig.h: configSETUP_TICK_INTERRUPT / configCLEAR_TICK_INTERRUPT. */
void vPortSetupTimerInterrupt(void)
{
    if (!g_tick_period)
        g_tick_period = get_timer_freq() / 1000; /* default 1ms */

    set_timer_value(g_tick_period);
    enable_physical_timer();
}

void generic_timer_clear_irq(void)
{
    /* reprogram the timer to clear the interrupt condition */
    set_timer_value(g_tick_period);
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

