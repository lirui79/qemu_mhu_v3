#include <stdio.h>
#include <stdint.h>
#include "host.h"
#include "system.h"
#include "vcodec.h"
#include "cmdr52_proc.h"

static volatile int g_core0_started = 0;
static volatile int g_core1_started = 0;

/* per-core FreeRTOS scheduler running flag (declared extern in system.h) */
volatile uint8_t g_scheduler_started[2] = {0, 0};

/**
 * initterupts initialization
 */
static void interrupt_init(void)
{
    uint32_t af = get_cpu_id();
    uint32_t rd;

    if (af == 0) {
        // Set location of GIC
        setGICAddr((void*)GICD_BASE, (void*)GICR_BASE);

        // Enable GIC
        enableGIC();
    }

    // Get the ID of the Redistributor connected to this PE
    rd = getRedistID(af);
    ts_assert(0xFFFFFFFF != rd);

    // Mark this core as being active
    wakeUpRedist(rd);

    // Configure the CPU interface
    // This assumes that the SRE bits are already set
    setPriorityMask(0xFF);
    enableGroup0Ints();
    enableGroup1Ints();
}

/**
 * peripheral initialization
 */
static void peripheral_init(void)
{
    /* uart initialization */
    uart_config_t uart_cfg = {8, 1, 0, 115200};
    uart_configure(UART0_BASE, &uart_cfg);
    uart_configure(UART1_BASE, &uart_cfg);

    /* mhu initialization */
    mhu_init();
}

/**
 * core 0 entry
 */
int main(void)
{
    /* system initialization */
      interrupt_init();
      peripheral_init();

      vcodecr52_init();

      cmdr52_set_callback();
      ts_info("R52 core 0 startup\n");

      /* enable caches */
      enable_caches();

      /* enable arch timer */
      arch_timer_init(SYSTEM_TICK_MS_0);

      /* enable IRQ and FIQ in SVC mode */
      __asm volatile ("CPSIE if");

      g_core0_started = 1;
      while (!g_core1_started) {
      }

      /* run self test */
      //self_test();

      /* signal startup event to host CPU */
      mhu_send_event(0, TS_EVENT_STARTUP);

      /* wait until host CPU has initialized its side of the MHU */
      ts_info("Wait host startup doorbell\n");
      mhu_wait_event(0, HOST_EVENT_STARTUP);
      ts_info("Host startup doorbell received\n");

      /* start command processor */
      cmdr52_proc_loop();

      /* never exist */
      while (1) {
          __asm volatile ("WFI");
      }
      return 0;
}

/**
 * core 1 entry
 *
 * AMP:每个核各自调用 vTaskStartScheduler(),启动自己独立的调度器实例。
 */
int main_core1(void)
{
    /* wait until core 0 is started */
    while (!g_core0_started) {
    }
    ts_info("R52 core 1 startup\n");

    /* system initialization */
    interrupt_init();

    /* enable the caches */
    enable_caches();

    /* enable arch timer */
    arch_timer_init(SYSTEM_TICK_MS_1);

    /* enable IRQ and FIQ in SVC mode */
    __asm volatile ("CPSIE if");

    g_core1_started = 1;

    /* start task scheduler */


    /* never exist */
    while (1) {
        __asm volatile ("WFI");
    }
    return 0;
}
