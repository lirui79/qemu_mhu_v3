#include <stdint.h>
#include "system.h"
#include "gicv3_basic.h"
#include "FreeRTOS.h"  /* for configSUPPORT_STATIC_ALLOCATION / StaticTask_t */
#include "task.h"      /* for xTaskGetSchedulerState / taskSCHEDULER_NOT_STARTED */

/**
 * IRQ handler
 */
void irq_handler(void)
{
    uint32_t interrupt_id;

    // Read the IAR to get the INTID of the interrupt taken
    interrupt_id = readIARGrp1();

    switch (interrupt_id) {
        case IRQ_ID_PHY_TIMER: {
            arch_timer_isr();
            break;
        }
        case IRQ_ID_MHU_PBX_COMB: {
            mhu_pbx_isr();
            break;
        }
        case IRQ_ID_MHU_MBX_COMB: {
            mhu_mbx_isr();
            break;
        }
        default: {
            ts_printf("Fatal: unhandled interrupt %d\n", interrupt_id);
            while (1){};
            break;
        }
    }

    // Write EOIR to deactivate interrupt
    writeEOIGrp1(interrupt_id);
}


/*============================================================================
 * FreeRTOS Required Callbacks
 *============================================================================*/

/*
 * vApplicationIRQHandler - Called by FreeRTOS IRQ assembly wrapper.
 *
 * The port reads ICC_IAR1_EL1 and passes its value as ulICCIAR.
 * Our job: handle interrupts (timer tick), clear them, and signal
 * if a context switch is needed.
 */
void vApplicationIRQHandler(uint32_t ulICCIAR)
{
    uint32_t ulInterruptID = ulICCIAR & 0x3FF;
    /* 与 R52 侧一致:不打印 tick 中断(PPI30),避免日志噪音;
     * 只保留非 tick 中断(MHU 组合等)用于调试。 */
    if (ulInterruptID != IRQ_ID_PHY_TIMER)
        ts_printf("IRQ:%u\n", ulInterruptID);
     switch (ulInterruptID) {
        case IRQ_ID_PHY_TIMER: {
            /* 调度器启动前(boot_main 里 arch_timer_init + DAIFClr 就使能了
             * PPI30,查询/TEST 阶段 tick 中断已在跑):只重装定时器、递增
             * g_sys_ticks,不能碰 RTOS tick 结构。
             * 调度器启动后:必须调 FreeRTOS_Tick_Handler(),它完成清中断、
             * xTaskIncrementTick() 并置 ullPortYieldRequired 触发切换;
             * 否则 FreeRTOS tick 恒为 0,vTaskDelay 永不超时,
             * Task1/Task2 各打印一次后永久挂起。 */
            if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
                FreeRTOS_Tick_Handler();
            else
                arch_timer_isr();
            break;
        }
        case IRQ_ID_MHU_PBX_COMB: {
            mhu_pbx_isr();
            break;
        }
        case IRQ_ID_MHU_MBX_COMB: {
            mhu_mbx_isr();
            break;
        }
        default: {
            ts_printf("Fatal: unhandled interrupt %d\n", ulInterruptID);
            while (1){};
            break;
        }
    }

    // Write EOIR to deactivate interrupt
    writeEOIGrp1(ulInterruptID);
}


/**
 * Interrupt enable
 */
void interrupt_enable(int irq, INT_PRIORITY priority)
{
    uint32_t af = get_cpu_id();
    uint32_t rd = getRedistID(af);
    setIntPriority(irq, rd, priority);
    setIntGroup(irq, rd, GICV3_GROUP1_NON_SECURE);
    setIntRoute(irq, GICV3_ROUTE_MODE_COORDINATE, af);
    setIntType(irq, rd, GICV3_CONFIG_LEVEL);
    enableInt(irq, rd);
}

/**
 * Interrupt disable
 */
void interrupt_disable(int irq)
{
    uint32_t af = get_cpu_id();
    uint32_t rd = getRedistID(af);
    disableInt(irq, rd);
}


/*
 * vApplicationGetTimerTaskMemory - Required for static timer allocation
 */
#if (configSUPPORT_STATIC_ALLOCATION == 1)
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    configSTACK_DEPTH_TYPE *pulTimerTaskStackSize)
{
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];

    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

/*
 * vApplicationGetIdleTaskMemory - Required for static idle task allocation
 */
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   configSTACK_DEPTH_TYPE *pulIdleTaskStackSize)
{
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];

    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
#endif /* configSUPPORT_STATIC_ALLOCATION */
