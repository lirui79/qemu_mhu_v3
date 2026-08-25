/*
 * FreeRTOSConfig.h for Cortex-R52 (task_scheduler)
 *
 * Target: ARMv8-R AArch32, GCC, FreeRTOS-Kernel ARM_CR52 port.
 * The build must be soft-float (Makefile: -mfloat-abi=soft), therefore
 * configUSE_TASK_FPU_SUPPORT is set to 1 (no FPU context by default).
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include "platform.h"

/*-----------------------------------------------------------
 * Cortex-R52 / ARM_CR52 port specific definitions.
 *-----------------------------------------------------------*/

/* The ARM_CR52 port requires a GICv2-style memory-mapped CPU interface.
 * The R52 GIC is a GICv3 (GICD at 0x2F000000, GICR at 0x2F100000) and this
 * virtual platform aliases the CPU interface registers at GICD + 0x2000,
 * matching the convention used by host_demo (A76, GICD + 0x2000). */
#define configINTERRUPT_CONTROLLER_BASE_ADDRESS         0x2F000000UL
#define configINTERRUPT_CONTROLLER_CPU_INTERFACE_OFFSET 0x2000UL

/* Number of unique interrupt priorities implemented by the GIC.  The port
 * probes GICD_IPRIORITYR0 and asserts it matches this value. */
#define configUNIQUE_INTERRUPT_PRIORITIES               (16)

/* Must be non-zero, > configUNIQUE_INTERRUPT_PRIORITIES / 2 and
 * <= configUNIQUE_INTERRUPT_PRIORITIES (checked by the port). */
#define configMAX_API_CALL_INTERRUPT_PRIORITY           10

/* The port requires this to be 1 or 2.  Soft-float build: tasks only get an
 * FPU context if they explicitly call vPortTaskUsesFPU(). */
#define configUSE_TASK_FPU_SUPPORT                      1

/*-----------------------------------------------------------
 * Constants related to the rate or tick.
 *-----------------------------------------------------------*/
#define configCPU_CLOCK_HZ                              (SYS_FREQ_HZ)
#define configTICK_RATE_HZ                              (1000)

/*-----------------------------------------------------------
 * Scheduler configuration.
 *-----------------------------------------------------------*/
#define configUSE_PREEMPTION                            1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION         1
#define configUSE_IDLE_HOOK                             0
#define configUSE_TICK_HOOK                             0
#define configUSE_16_BIT_TICKS                          0
#define configIDLE_SHOULD_YIELD                         1
#define configMAX_PRIORITIES                            (16)
#define configMINIMAL_STACK_SIZE                        (1024)      /* words */
#define configMAX_TASK_NAME_LEN                         (16)
#define configUSE_TRACE_FACILITY                        0
#define configUSE_STATS_FORMATTING_FUNCTIONS            0

/*-----------------------------------------------------------
 * Synchronisation primitives.
 *-----------------------------------------------------------*/
#define configUSE_MUTEXES                               1
#define configUSE_RECURSIVE_MUTEXES                     1
#define configUSE_COUNTING_SEMAPHORES                   1
#define configUSE_QUEUE_SETS                            1
#define configQUEUE_REGISTRY_SIZE                       0

/*-----------------------------------------------------------
 * Software timer (disabled for now, values kept valid).
 *-----------------------------------------------------------*/
#define configUSE_TIMERS                                0
#define configTIMER_TASK_PRIORITY                       (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH                        (10)
#define configTIMER_TASK_STACK_DEPTH                    (configMINIMAL_STACK_SIZE * 2)

/*-----------------------------------------------------------
 * Memory allocation.
 *-----------------------------------------------------------*/
#define configSUPPORT_STATIC_ALLOCATION                 0
#define configSUPPORT_DYNAMIC_ALLOCATION                1

/* AMP(每核独立 FreeRTOS 实例):本平台双核 Cortex-R52 共享 16MB 本地 RAM,
 * 每核各链接同一份内核镜像,但所有内核状态(调度器、任务链表、tick、临界嵌套、堆等)
 * 均按本宏的数量复制为 per-core 数组,由当前核 ID(MPIDR 低位)索引。
 * 每个核运行完全独立的调度器与任务集,互不干扰。 */
#define configACTIVE_CORE_COUNT                         (2)

/* FreeRTOS 堆放入本地 RAM(link.ld: .frtos_heap)。
 * R52 本地 RAM 已扩到 16MB,空间充足;共享 DDR(0x80000000..)MPU Region 5
 * 已配置 RW 可访问,BQueue/CQueue 大数据区由 utils/ddr_mem.c 放 DDR,
 * 因此堆只承载小对象(任务栈/TCB/信号量等)。
 * configTOTAL_HEAP_SIZE 为"每个核"的堆大小:每核 64KB,两核共 128KB,
 * cmdr52 各线程(recv/work/wait)512 words 栈 + TCB 完全够用。 */
#define configTOTAL_HEAP_SIZE                           (1024 * 1024)

/*-----------------------------------------------------------
 * Hook functions.
 *-----------------------------------------------------------*/
#define configCHECK_FOR_STACK_OVERFLOW                  0
#define configUSE_MALLOC_FAILED_HOOK                    0
#define configUSE_APPLICATION_TASK_TAG                  0

/*-----------------------------------------------------------
 * Tick interrupt.
 *
 * The R52 arch timer (CNTP, PPI 30) is used as the FreeRTOS tick.
 * arch_timer_init() arms the timer and enables the IRQ; arch_timer_isr()
 * reloads CNTP_TVAL to clear the interrupt (called by the port's weak
 * FreeRTOS_Tick_Handler via configCLEAR_TICK_INTERRUPT()).
 *-----------------------------------------------------------*/
extern void arch_timer_init(uint32_t tick_ms);
extern void arch_timer_isr(void);

#define configSETUP_TICK_INTERRUPT()    arch_timer_init(1000U / configTICK_RATE_HZ)
#define configCLEAR_TICK_INTERRUPT()    arch_timer_isr()

/*-----------------------------------------------------------
 * Assertions.
 *-----------------------------------------------------------*/
int ts_printf(const char *fmt, ...);

#define configASSERT(x)                                                        \
    if ((x) == 0)                                                              \
    {                                                                          \
        ts_printf("FreeRTOS assert failed: %s line %d\n", __FILE__, __LINE__); \
        for (;;)                                                               \
        {                                                                      \
        }                                                                      \
    }
#define configASSERT_DEFINED 1

/*-----------------------------------------------------------
 * API inclusion.
 *-----------------------------------------------------------*/
#define INCLUDE_vTaskPrioritySet            1
#define INCLUDE_uxTaskPriorityGet           1
#define INCLUDE_vTaskDelete                 1
#define INCLUDE_vTaskSuspend                1
#define INCLUDE_vTaskDelayUntil             1
#define INCLUDE_vTaskDelay                  1
#define INCLUDE_xTaskGetSchedulerState      1
#define INCLUDE_xTaskGetCurrentTaskHandle   1
#define INCLUDE_eTaskGetState               1
#define INCLUDE_xTaskGetIdleTaskHandle      0
#define INCLUDE_xTimerPendFunctionCall      0
#define INCLUDE_xSemaphoreGetMutexHolder    0

#endif /* FREERTOS_CONFIG_H */
