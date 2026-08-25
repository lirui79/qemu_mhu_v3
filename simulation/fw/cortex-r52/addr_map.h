/*
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ADDR_MAP_H
#define ADDR_MAP_H

/* Must match conf.lua and addr-map.xlsx */
#define RAM_BASE           0x00000000UL
#define RAM_SIZE           0x01000000UL
#define DDR_BASE           0x80000000UL
#define DDR_SIZE           0x20000000UL
#define GICD_BASE          0x2F000000UL
#define GICD_SIZE          0x00010000UL
#define GICR_BASE          0x2F100000UL
#define GICR_SIZE          0x00040000UL   /* 2 CPUs × 0x20000 */

/* master_if_* @ 0x2FC6_0000 */
#define UART_BASE          0x2FC60000UL   /* master_if_uart_0 */
#define UART_SIZE          0x00001000UL
#define UART1_BASE         0x2FC61000UL   /* master_if_uart_1 */
#define UART1_SIZE         0x00001000UL
#define TS_GEN_BASE        0x2FC62000UL   /* master_if_timestamp_gen (apb_ctrl) */
#define TS_GEN_SIZE        0x00001000UL
#define TS_GEN_REG_BASE    0x2FC63000UL   /* master_if_timestamp_gen_reg (apb_cnt) */
#define TS_GEN_REG_SIZE    0x00001000UL
#define DMA_BASE           0x2FC64000UL   /* master_if_dma / dw_axi_dmac */
#define DMA_SIZE           0x00004000UL

/* ---- MHU-DavarAE (SoC map: inter.md §3.4 / conf.lua) ---- */
/* snd_reg / rec_reg are 128 KiB; each holds two 64 KiB PBX/MBX frames. */
#define MHU_SND_DATA       0x2FC00000UL   /* master_if_mhu_snd_data, 64 KiB */
#define MHU_SND_REG        0x2FC10000UL   /* master_if_mhu_snd_reg,  128 KiB */
#define MHU_REC_DATA       0x2FC30000UL   /* master_if_mhu_rec_data, 64 KiB */
#define MHU_REC_REG        0x2FC40000UL   /* master_if_mhu_rec_reg,  128 KiB */
#define MHU_BLOCK_SIZE     0x00010000UL
#define MHU_R52_PBX        (MHU_SND_REG)                      /* R52 Sender  (Postbox) */
#define MHU_A76_PBX        (MHU_SND_REG + MHU_BLOCK_SIZE)     /* A76 Sender  (Postbox) */
#define MHU_R52_MBX        (MHU_REC_REG)                      /* R52 Receiver (Mailbox) */
#define MHU_A76_MBX        (MHU_REC_REG + MHU_BLOCK_SIZE)     /* A76 Receiver (Mailbox) */

#define IRQ_TEST_BASE      0xC0001000UL   /* VP-only IRQ injector */
#define IRQ_TEST_SIZE      0x00001000UL

/* SPI assignments per inter.md (model supports 8 ch, SoC table lists 16):
 *   SPI[0]   = DMA  dma_intr        (INT_ID 32,  combined)
 *   SPI[1..8]= DMA  dma_intr_ch[0..7](INT_ID 33-40, 8 ch model supports)
 *   SPI[9..16]= reserved (ch8-15, model does not support)
 *   SPI[17]  = DMA  dma_intr_cmnreq (INT_ID 49,  common registers)
 *   SPI[18]  = UART uart0_intr      (INT_ID 50)
 *   SPI[19]  = IRQ test (VP-only)   (INT_ID 51)
 *   SPI[46]  = MHU  mhus_pbx_int    (INT_ID 78,  sender/postbox combo)
 *   SPI[78]  = MHU  mhur_mbx_int    (INT_ID 110, receiver/mailbox combo)
 */
#define GIC_SPI_DMA_COMB   0
#define GIC_SPI_DMA_CH_BASE  1     /* SPI[1] is dma_intr_ch[0] */
#define GIC_SPI_DMA_CMN    17
#define GIC_SPI_UART       18
#define GIC_SPI_TEST       19
#define GIC_SPI_MHU_R52_TX 46   /* sender (PBX) mhus_pbx_int */
#define GIC_SPI_MHU_R52_RX 78   /* receiver (MBX) mhur_mbx_int */

#define GIC_INTID_DMA_CH(n)  (32U + GIC_SPI_DMA_CH_BASE + (n))
#define GIC_INTID_DMA_COMB  (32U + GIC_SPI_DMA_COMB)
#define GIC_INTID_DMA_CMN   (32U + GIC_SPI_DMA_CMN)
#define GIC_INTID_UART      (32U + GIC_SPI_UART)
#define GIC_INTID_TEST      (32U + GIC_SPI_TEST)
#define GIC_INTID_MHU_R52   (32U + GIC_SPI_MHU_R52_RX)

#define GIC_SPI_ARCH_TIMER 14   /* PPI, not SPI */
#define GIC_INTID_ARCH_TIMER (16U + GIC_SPI_ARCH_TIMER)

/* Timer PPI mappings */
#define ARCH_TIMER_NS_EL1_IRQ  (16U + 14)   /* PPI 30 */

#define IRQ_TEST_CLEAR     (IRQ_TEST_BASE + 0x0UL)
#define IRQ_TEST_START     (IRQ_TEST_BASE + 0x4UL)

/* A76 firmware load address (in shared DDR) */
#define A76_FW_BASE        (DDR_BASE + 0x00100000UL)

#endif /* ADDR_MAP_H */
