/*
 * FreeRTOSConfig.h - FreeRTOS v11.3.0 Configuration
 * Platform: Cortex-A76 (AArch64), QEMU virt, GICv3, Generic Timer Tick
 */

#ifndef _FREERTOS_CONFIG_H_
#define _FREERTOS_CONFIG_H_

/*============================================================================
 * Scheduler Configuration
 *============================================================================*/
#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configUSE_TICKLESS_IDLE                 0
#define configCPU_CLOCK_HZ                      62500000UL
#define configTICK_RATE_HZ                      1000
#define configMAX_PRIORITIES                    16
#define configMINIMAL_STACK_SIZE                1024     /* words = 2KB */
#define configMAX_TASK_NAME_LEN                 16
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_TASK_NOTIFICATIONS            1
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configQUEUE_REGISTRY_SIZE               8
#define configUSE_QUEUE_SETS                    1
#define configUSE_TIME_SLICING                  1
#define configUSE_NEWLIB_REENTRANT              0
#define configENABLE_BACKWARD_COMPATIBILITY     0
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 5
#define configSTACK_DEPTH_TYPE                  uint16_t

/*============================================================================
 * Memory Allocation
 *============================================================================*/
#define configSUPPORT_STATIC_ALLOCATION         1
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configTOTAL_HEAP_SIZE                   (1024 * 1024)
#define configAPPLICATION_ALLOCATED_HEAP        0

/*============================================================================
 * Hook Functions
 *============================================================================*/
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configCHECK_FOR_STACK_OVERFLOW          1
#define configUSE_MALLOC_FAILED_HOOK            0
#define configUSE_DAEMON_TASK_STARTUP_HOOK      0

/*============================================================================
 * Runtime Statistics
 *============================================================================*/
#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_TRACE_FACILITY                0
#define configUSE_STATS_FORMATTING_FUNCTIONS    0

/*============================================================================
 * Co-routines (deprecated)
 *============================================================================*/
#define configUSE_CO_ROUTINES                   0
#define configMAX_CO_ROUTINE_PRIORITIES         2

/*============================================================================
 * Timer / Software Timer
 *============================================================================*/
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            512

/*============================================================================
 * ARM CA72 64-bit Port Specific
 *============================================================================*/
#define configKERNEL_INTERRUPT_PRIORITY         255
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    128
#define configUNIQUE_INTERRUPT_PRIORITIES               (16)

/*============================================================================
 * Interrupt Nesting - ARM_CA72_64_Bit port uses these
 *============================================================================*/
#define configMAX_API_CALL_INTERRUPT_PRIORITY   10

/* 1. GIC 分发器基地址 (GIC Distributor Base Address) */
/* 常见值：0xF9000000, 0x2C000000 等，请查阅你的芯片手册 */
#define configINTERRUPT_CONTROLLER_BASE_ADDRESS         0x2F020000UL


/* 2. GIC CPU 接口偏移量 (CPU Interface Offset from Base) */
/* GICv2 通常为 0x2000 或 0x10000，取决于内存映射布局 */
#define configINTERRUPT_CONTROLLER_CPU_INTERFACE_OFFSET 0x2000UL

/*============================================================================
 * Tick Timer Configuration (ARM Generic Timer)
 * These macros are required by the ARM_AARCH64 port.
 *
 * vPortSetupTimerInterrupt() is implemented in generic_timer.c.
 * generic_timer_clear_irq() clears the timer IRQ condition.
 *============================================================================*/
extern void vPortSetupTimerInterrupt(void);
extern void generic_timer_clear_irq(void);

#define configSETUP_TICK_INTERRUPT()    vPortSetupTimerInterrupt()
#define configCLEAR_TICK_INTERRUPT()    generic_timer_clear_irq()

/*------------------------------------------------------------------------------
 * FreeRTOS ARM_CA72_64_Bit port expects ulPortYieldRequired to be declared
 * in the application. This variable is set by vApplicationIRQHandler when
 * a tick requires a context switch.
 *----------------------------------------------------------------------------*/

/*============================================================================
 * ASSERT
 *============================================================================*/
extern void uart_puts(const char *s);
extern void uart_put_hex32(uint32_t val);
#define configASSERT(x)                                                      \
    if ((x) == 0) {                                                          \
        uart_puts("\r\n[ASSERT] " __FILE__ ":");                             \
        uart_put_hex32(__LINE__);                                            \
        uart_puts("\r\n");                                                   \
        taskDISABLE_INTERRUPTS();                                            \
        for (;;);                                                            \
    }

/*============================================================================
 * INCLUDE Demo / Test Functions
 *============================================================================*/
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTaskGetIdleTaskHandle          1
#define INCLUDE_eTaskGetState                   1
#define INCLUDE_xEventGroupSetBitFromISR        1
#define INCLUDE_xTimerPendFunctionCall          1
#define INCLUDE_xTaskAbortDelay                 1
#define INCLUDE_xTaskGetHandle                  1
#define INCLUDE_xTaskResumeFromISR              1

/*============================================================================
 * Assert output via UART
 *============================================================================*/
extern void uart_puts(const char *s);
extern void vTaskStartScheduler(void);

#endif /* _FREERTOS_CONFIG_H_ */
