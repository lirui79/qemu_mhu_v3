#include <stdint.h>
#include "system.h"
#include "gicv3_basic.h"

/**
 * IRQ handler
 */
void irq_handler(uint32_t ulInterruptID) {
    uint32_t ulCpuID = get_cpu_id();
    switch (ulInterruptID) {
        case IRQ_ID_PHY_TIMER: {
            arch_timer_isr();
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
            ts_info("CPU:%u IRQ:%u\n", ulCpuID, ulInterruptID);
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
