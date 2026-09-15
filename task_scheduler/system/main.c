#include <stdio.h>
#include <stdint.h>
#include "host.h"
#include "system.h"
#include "gicv3_basic.h"
#include "mhu_davarae.h"
#include "FreeRTOS.h"
#include "task.h"
#include "vcodec.h"
#include "cmdr52_proc.h"

static volatile int g_core0_started = 0;
static volatile int g_core1_started = 0;

/* per-core FreeRTOS scheduler running flag (declared extern in system.h) */
volatile uint8_t g_scheduler_started[2] = {0, 0};

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
        ts_printf("[Task2] Heartbeat #%u\r\n", ulBeat++);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

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

    vcodecr52_init();
    cmdr52_set_callback();
    ts_printf("R52 core 0 startup\n");

    /* enable caches */
    enable_caches();

    /* NOTE: core0 现在运行 FreeRTOS 调度器。vTaskStartScheduler() 会通过
     * configSETUP_TICK_INTERRUPT() (= arch_timer_init(1)) 配置物理定时器 tick,
     * core0 收到 PPI30(IRQ:30) 后由 vApplicationIRQHandler 路由到
     * FreeRTOS_Tick_Handler()->xTaskIncrementTick(),调度器已启动,安全。 */

    /* enable IRQ and FIQ in SVC mode */
          /* enable arch timer */
    arch_timer_init(SYSTEM_TICK_MS_0);
    
    __asm volatile ("CPSIE if");

    g_core0_started = 1;
    while (!g_core1_started) {
    }

    /* signal startup event to host CPU */
    mhu_send_event(0, TS_EVENT_STARTUP);

    /* wait until host CPU has initialized its side of the MHU */
    ts_printf("Wait host startup doorbell\n");
    mhu_wait_event(0, HOST_EVENT_STARTUP);
    ts_printf("Host startup doorbell received\n");


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
    }*/


//*
    g_scheduler_started[0] = 1;

    /* Start the scheduler - never returns */
    vTaskStartScheduler();

    /* Should never reach here */
    ts_printf("[ERROR] Scheduler returned!\r\n");
    for (;;) {
        __asm__ volatile("wfi");
    }
    return 0;
}

/**
 * core1 的演示任务:证明 core1 拥有完全独立的 FreeRTOS 实例。
 * 该任务在 core1 自己的调度器上运行(vTaskDelay 挂起/唤醒均由
 * core1 独立的延时链表与 tick 驱动),与 core0 的 command_task
 * 互不干扰。每 500ms 打印一次。
 */
static void core1_demo_task(void *arg)
{
    (void)arg;
    for (;;) {
        ts_printf("[CORE1] demo task alive, tick=%lu\n",
                  (unsigned long)xTaskGetTickCount());
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/**
 * core 1 entry
 *
 * AMP:每个核各自调用 vTaskStartScheduler(),启动自己独立的调度器实例。
 */
int main_core1(void)
{
    /* demo 任务仅做 ts_printf + vTaskDelay,调用链浅,1KB 栈足够。
     * 每核 heap 仅 12KB(configTOTAL_HEAP_SIZE):command 4KB + idle 4KB
     * + 两个 demo 各 1KB + 4×TCB ≈ 10.8KB。此前用 2048 words(8KB)
     * 直接耗尽 heap 导致 xTaskCreate(Task1) 返回失败。 */
    #define BLINK_STACK_SIZE  256  /* 1KB words */

    TaskHandle_t xTask1Handle = NULL;
    TaskHandle_t xTask2Handle = NULL;
    BaseType_t xResult;

    /* wait until core 0 is started */
    while (!g_core0_started) {
    }

    /* AMP:core1 必须也初始化本核的 GIC CPU interface(ICC_PMR/
     * ICC_IGRPEN0/1_EL1 是 per-core 状态)。缺 enableGroup1Ints() 时,
     * Group1 的 PPI30(tick)虽在 RD 上使能,但不会被 CPU interface 投递,
     * core1 收不到任何中断(此前 demo task 打印一次后永远睡死)。
     * GICD 全局配置由 core0 完成,重复调用幂等无害。 */
    interrupt_init();

    ts_printf("R52 core 1 startup\n");

    /* enable the caches */
    enable_caches();

    /* enable arch timer */
    arch_timer_init(SYSTEM_TICK_MS_1);

    /* enable IRQ and FIQ in SVC mode */
    __asm volatile ("CPSIE if");

    g_core1_started = 1;

    /* core1 创建自己的任务(栈来自 core1 独立的堆),然后启动 core1
     * 自己的调度器。PPI30(tick)按核分发,core1 的 tick 驱动 core1
     * 的调度器。 */
    if (pdPASS != xTaskCreate(core1_demo_task, "c1demo",
                              configMINIMAL_STACK_SIZE / 2, NULL,
                              tskIDLE_PRIORITY + 1, NULL))
    {
        ts_printf("[ERROR] Failed to create core1 demo task\r\n");
        for (;;) {
            __asm__ volatile("wfi");
        }
    }

    /* Create demo tasks */

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


    /* 标记本核调度器已启动,使 IRQ 处理把 PPI30 路由到
     * FreeRTOS_Tick_Handler()(xTaskIncrementTick)。 */
    g_scheduler_started[1] = 1;

    vTaskStartScheduler();

    /* Should never reach here */
    ts_printf("[ERROR] Scheduler returned!\r\n");
    for (;;) {
        __asm__ volatile("wfi");
    }
    return 0;
}
