#include <stdint.h>
#include "system.h"
#include "gicv3_basic.h"
#include "FreeRTOS.h"

extern void FreeRTOS_Tick_Handler(void);

/**
 * IRQ handler
 */
void vApplicationIRQHandler( uint32_t ulICCIAR)
{
    uint32_t ulInterruptID = 0, ulCpuID = get_cpu_id();
    const uint32_t ulSpuriousInterruptID = 1023; // GIC 规范中 1023 代表伪中断

    // 1. 从 IAR 寄存器中提取中断 ID (通常取低 10 位)
    ulInterruptID = ulICCIAR & 0x3FFUL;
    /* 不打印 tick 中断(PPI30,每 1ms 一次):双核 1ms tick 的 IRQ 打印
     * 会以每秒数千行的速度刷屏,且 ts_printf 逐字符发送、可被中断打断,
     * 导致日志逐字符交错、完全不可读。只保留非 tick 中断(MHU 组合等)
     * 的打印用于调试。 */
    if (ulInterruptID != IRQ_ID_PHY_TIMER)
        ts_printf("CPU:%u IRQ:%u\n", ulCpuID, ulInterruptID);
    switch (ulInterruptID) {
        case IRQ_ID_PHY_TIMER: {
            // PPI30(物理定时器)是 FreeRTOS tick 源,属于运行调度器的核。
            // 各核通过 g_scheduler_started[core] 标记其调度器是否已启动:
            //   - 已启动: 必须走 FreeRTOS_Tick_Handler()(内部递增 tick 并清除中断);
            //   - 未启动: 走 arch_timer_isr() 仅清除定时器中断源,避免
            //             xTaskIncrementTick() 在调度器未启动(指针为 NULL)时崩溃。
            if (g_scheduler_started[ulCpuID]) {
                FreeRTOS_Tick_Handler();
            } else {
                arch_timer_isr();
            }
            break;
        }
        case IRQ_ID_DMA_COMB: {
            //dma_combo_irq_handler();
            break;
        }
        case IRQ_ID_DMA_CH_0:
        case IRQ_ID_DMA_CH_1:
        case IRQ_ID_DMA_CH_2:
        case IRQ_ID_DMA_CH_3:
        case IRQ_ID_DMA_CH_4:
        case IRQ_ID_DMA_CH_5:
        case IRQ_ID_DMA_CH_6:
        case IRQ_ID_DMA_CH_7: {
            //dma_irq_handler(interrupt_id - IRQ_ID_DMA_CH_0);
            break;
        }
        case IRQ_ID_DMA_COMMON: {
            //dma_common_irq_handler();
            break;
        }
        case IRQ_ID_MHU_PBX_COMB: {
            mhu_pbx_isr();
            break;
        }
        case IRQ_ID_MHU_MBX_COMB: {
            mhu_mbx_isr();
            //ts_info("CPU:%u IRQ:%u\n", ulCpuID, ulInterruptID);
            break;
        }
        default: {
            ts_err("Fatal: unhandled interrupt %d\n", ulInterruptID);
            while (1){};
            break;
        }
    }
}

/**
 * Interrupt enable
 */
void interrupt_enable(int irq, INT_PRIORITY priority)
{
    uint32_t af = get_cpu_id();
    uint32_t rd = getRedistID(af);

    /* 防御:本平台 RD index == CPU ID(RD0<->core0, RD1<->core1)。
     * 若 VP 的 GICR_TYPER 读取异常导致 getRedistID() 失败(见 run.sh 中
     * gdb_port remote_argv 的说明),直接按 CPU ID 选取 RD,保证 PPI30
     * (Timer tick)与 MHU 组合中断仍能被使能,否则 enableInt 等操作
     * 会因 rd > gic_max_rd 静默跳过,IRQ 永远收不到。 */
    if (0xFFFFFFFF == rd) {
        rd = af;
        ts_printf("Warning: redistributor lookup fallback to cpu id %u\n", af);
    }
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

    /* 与 interrupt_enable 相同的兜底逻辑 */
    if (0xFFFFFFFF == rd) {
        rd = af;
    }
    disableInt(irq, rd);
}
