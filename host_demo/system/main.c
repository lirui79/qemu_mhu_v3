#include <stdio.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

#include "system.h"
#include "gicv3_basic.h"
#include "ts_protocol.h"
#include "vcodec.h"
#include "vcodec_test.h"

#include "cmda78_proc.h"

/**
 * initterupts initialization
 */
static void interrupt_init(void)
{
    uint32_t af = get_cpu_id();
    uint32_t rd;

    // Set location of GIC
    setGICAddr((void*)GICD_BASE, (void*)GICR_BASE);

    // Enable GIC
    enableGIC();

    // Get the ID of the Redistributor connected to this PE
    rd = getRedistID(af);
    if (0xFFFFFFFF == rd)
    {
        ts_printf("Invalid redistributor\n");
        return;
    }

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
    uart_config uart_cfg = {8, 1, 0, 115200};
    uart_configure(UART0_BASE, &uart_cfg);

    /* mhu initialization */
    mhu_init();
}

/* get current exception level */
static inline int get_current_el(void)
{
    uint64_t current_el;
    __asm__ volatile("MRS %0, CurrentEL" : "=r" (current_el));
    return (int)(current_el >> 2) & 0x3;
}

/* query TS information */
static int query_ts_info(void)
{
    ts_cmd_query query;
    ts_data_hw_info hw_info;
    ts_data_sw_info sw_info;
    uint32_t len;

    query.header     = TS_HDR_MAKE(TS_CMD_CODE_QUERY, 0, TS_PACKET_LEN(ts_cmd_query));
    query.type_param = (uint32_t)TS_QUERY_TYPE_HW;  /* type:8, param:24=0 */

    /* clear FC status before sending query to avoid stale event */
    {
        unsigned long flags = arch_local_irq_save();
        g_mbx_fc_stat_0 = 0;
        arch_local_irq_restore(flags);
    }

    ts_printf("SEND QUERY HW type=%u\n", (uint8_t)(query.type_param & 0xFF));
    len = mhu_send_data(0, &query, sizeof(ts_cmd_query));
    if (len != sizeof(ts_cmd_query)) {
        ts_printf("failed to send query cmd, ret %u\n", len);
        return -1;
    }
    ts_printf("WAIT FC for HW info...\n");
    /* Re-check before WFI to avoid losing interrupt that arrived
     * between clearing g_mbx_fc_stat_0 and entering WFI */
    while (!MHU_FC_SIGNALED) {
        __asm__ volatile("wfi" : : : "memory");
    }
    ts_printf("GOT FC HW, reading...\n");
    len = mhu_recv_data(1, &hw_info, sizeof(hw_info));
    if (len != sizeof(hw_info)) {
        ts_printf("failed to receive hw info, ret %u\n", len);
        return -1;
    }

    query.header     = TS_HDR_MAKE(TS_CMD_CODE_QUERY, 0, TS_PACKET_LEN(ts_cmd_query));
    query.type_param = (uint32_t)TS_QUERY_TYPE_SW;  /* type:8, param:24=0 */

    /* clear FC status before sending query to avoid stale event */
    {
        unsigned long flags = arch_local_irq_save();
        g_mbx_fc_stat_0 = 0;
        arch_local_irq_restore(flags);
    }

    len = mhu_send_data(0, &query, sizeof(ts_cmd_query));
    if (len != sizeof(ts_cmd_query)) {
        ts_printf("failed to send query cmd, ret %u\n", len);
        return -1;
    }
    while (!MHU_FC_SIGNALED) {
        __asm__ volatile("wfi" : : : "memory");
    }
    len = mhu_recv_data(1, &sw_info, sizeof(sw_info));
    if (len != sizeof(sw_info)) {
        ts_printf("failed to receive hw info, ret %u\n", len);
        return -1;
    }

    ts_printf("<TS HW INFO>\n");
    ts_printf("  vendor id      : 0x%x\n",     (uint16_t)(hw_info.vendor_device & 0xFFFF));
    ts_printf("  device id      : 0x%x\n",     (uint16_t)(hw_info.vendor_device >> 16));
    ts_printf("  sm count       : %u\n",       (uint8_t)(hw_info.sm_config & 0xFF));
    ts_printf("  core per sm    : %u\n",       (uint8_t)((hw_info.sm_config >> 8) & 0xFF));
    ts_printf("  warp size      : %u\n",       (uint8_t)((hw_info.sm_config >> 16) & 0xFF));
    ts_printf("  warp per core  : %u\n",       (uint8_t)((hw_info.sm_config >> 24) & 0xFF));
    ts_printf("  smem size      : %u bytes\n", hw_info.smem_size);
    ts_printf("  l2 cache size  : %u bytes\n", hw_info.l2_cache_size);
    ts_printf("  l1 dcache size : %u bytes\n", hw_info.l1_dcache_size);
    ts_printf("  l1 icache size : %u bytes\n", hw_info.l1_icache_size);

    ts_printf("<TS SW INFO>\n");
    ts_printf("  fw version     : 0x%x\n", sw_info.ts_fw_version);

    return 0;
}

/*
 * Task 1 - Periodic status output
 */
static void vTask1(void *pvParameters)
{
    uint32_t ulCounter = 0;

    ts_printf("[TASK1] Started\r\n");
    for (;;) {
        ts_printf("[Task1] Tick: %u, Counter: %u\r\n",
              (uint32_t)xTaskGetTickCount(), ulCounter++);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/*
 * Task 2 - Heartbeat with short period
 */
static void vTask2(void *pvParameters)
{
    uint32_t ulBeat = 0;

    ts_printf("[TASK2] Started\r\n");
    for (;;) {
        if (ulBeat % 100 == 0) {
           ts_printf("[Task2] Heartbeat #%u\r\n", ulBeat++);
        }

        mhu_poll_rx();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static  void irq_callback_fifo(uint32_t irq, uint32_t channel) {
    ts_printf("[IRQ] FIFO %u\r\n", channel);
    uint32_t r52CoreID = 0;
    if (irq == 0) {
        for (r52CoreID = 0; r52CoreID < 2; r52CoreID++) {
            if (channel & (1ul << (2 * r52CoreID + 1))) {
               cmda78_thread_wakeup(r52CoreID);
            }
        }
    } else {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        for (r52CoreID = 0; r52CoreID < 2; r52CoreID++) {
            if (channel & (1ul << (2 * r52CoreID + 1))) {
                cmda78_thread_wakeup_from_isr(r52CoreID, &xHigherPriorityTaskWoken);
            }
        }
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}


/**
 * core 0 entry
 */
void boot_main(void)
{
        /* demo 任务仅做 ts_printf + vTaskDelay,调用链浅,1KB 栈足够。
     * 每核 heap 仅 12KB(configTOTAL_HEAP_SIZE):command 4KB + idle 4KB
     * + 两个 demo 各 1KB + 4×TCB ≈ 10.8KB。此前用 2048 words(8KB)
     * 直接耗尽 heap 导致 xTaskCreate(Task1) 返回失败。 */
    #define BLINK_STACK_SIZE  256  /* 1KB words */

    TaskHandle_t xTask1Handle = NULL;
    TaskHandle_t xTask2Handle = NULL;
    BaseType_t xResult;

    /* system initialization */
    interrupt_init();
    peripheral_init();

    ts_printf("A76 startup (CPU%u EL%d)\n", get_cpu_id(), get_current_el());

    /* enable arch timer */
    arch_timer_init(SYSTEM_TICK_MS);

    /* unmask interrupts at PSTATE */
    __asm__ volatile("msr DAIFClr, #0xF" : : : "memory");

    /* signal TS that host is ready */
    mhu_send_event(0, MHU_DB0_EVENT_HOST_STARTUP);
    ts_printf("Host startup signal sent\n");

    /* wait for TS to start */
    mhu_wait_event(0, MHU_DB0_EVENT_TS_STARTUP);
    ts_printf("TS started\n");

    /* query ts information */
//    query_ts_info();

    /* run tests */
//    run_test();

    /* Create demo tasks */
/*
    xResult = xTaskCreate(
        vTask1,
        "Task1",
        BLINK_STACK_SIZE,
        NULL,
        2,
        &xTask1Handle
    );
    if (xResult != pdPASS) {
        ts_printf("[ERROR] Failed to create Task1!\r\n");
        for (;;) { __asm__ volatile("wfi"); }
    }
*/
    xResult = xTaskCreate(
        vTask2,
        "Task2",
        BLINK_STACK_SIZE,
        NULL,
        3,
        &xTask2Handle
    );
    if (xResult != pdPASS) {
        ts_printf("[ERROR] Failed to create Task2!\r\n");
        for (;;) { __asm__ volatile("wfi"); }
    }
//*
    mhu_set_irq_callback(2, irq_callback_fifo);

    vcodeca78_init();

    vcodec_test_encode();
    vcodec_test_encode();
//    vcodec_test_encode();

    vcodec_test_decode();
    vcodec_test_decode();//*/

    /* Start the scheduler - never returns */
    vTaskStartScheduler();

    /* Should never reach here */
    ts_printf("[ERROR] Scheduler returned!\r\n");
    for (;;) {
        __asm__ volatile("wfi");
    }
}

int main(void) {
    boot_main();
    return 0;
}

void *memset(void *s, int c, size_t n)
{
    unsigned char *p   = (unsigned char *)s;
    unsigned char  val = (unsigned char)c;
    unsigned long *p_long;
    unsigned long  long_val = 0;

    if (s == NULL || n == 0) {
        return s;
    }

    while (n > 0 && ((uintptr_t)p % sizeof(unsigned long) != 0)) {
        *p++ = val;
        n--;
    }
    for (size_t i = 0; i < sizeof(unsigned long); i++) {
        long_val |= ((unsigned long)val << (i * 8));
    }
    p_long = (unsigned long *)p;
    while (n >= sizeof(unsigned long)) {
        *p_long++ = long_val;
        n -= sizeof(unsigned long);
    }
    p = (unsigned char *)p_long;
    while (n-- > 0) {
        *p++ = val;
    }

    return s;
}

void *memcpy(void *dest, const void *src, size_t n)
{
    unsigned char       *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    unsigned long       *d_long;
    const unsigned long *s_long;

    if (dest == NULL || src == NULL || n == 0) {
        return dest;
    }

    while (n > 0 && ((uintptr_t)d % sizeof(unsigned long) != 0)) {
        *d++ = *s++;
        n--;
    }
    d_long = (unsigned long *)d;
    s_long = (const unsigned long *)s;
    while (n >= sizeof(unsigned long)) {
        *d_long++ = *s_long++;
        n -= sizeof(unsigned long);
    }
    d = (unsigned char *)d_long;
    s = (const unsigned char *)s_long;
    while (n-- > 0) {
        *d++ = *s++;
    }

    return dest;
}



/*
 * vApplicationStackOverflowHook
 */
#if (configCHECK_FOR_STACK_OVERFLOW > 0)
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    ts_printf("\r\n[PANIC] Stack overflow in task: %s\r\n", pcTaskName);
    taskDISABLE_INTERRUPTS();
    for (;;) {
        __asm__ volatile("wfi");
    }
}
#endif
