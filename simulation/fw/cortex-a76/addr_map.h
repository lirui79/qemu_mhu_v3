/*
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * AARCH64 Cortex-A76 address map.
 *
 * Note: GICD/GICR addresses are local to the A76 RemotePass router and
 * do NOT conflict with the R52-side GIC which lives in a separate process.
 */

#ifndef FW_A76_ADDR_MAP_H
#define FW_A76_ADDR_MAP_H

/* A76 firmware load address (in shared DDR) */
#define A76_FW_BASE         0x80100000UL

/* GIC v3 — distributor & redistributor (in A76 RemotePass local router)
 * Must match conf.lua GICD_BASE_A76 / GICR_BASE_A76 */
#define GICD_BASE           0x2F020000UL
#define GICD_SIZE           0x00010000UL
#define GICR_BASE           0x2F120000UL
#define GICR_SIZE           0x00020000UL  /* 1 CPU × 0x20000 */

/* ---- MHU-DavarAE (SoC map: inter.md §3.4 / conf.lua) ---- */
#define MHU_BLOCK_SIZE      0x00010000UL
#define MHU_SND_REG         0x2FC10000UL   /* master_if_mhu_snd_reg */
#define MHU_REC_REG         0x2FC40000UL   /* master_if_mhu_rec_reg */
#define MHU_A76_PBX         (MHU_SND_REG + MHU_BLOCK_SIZE)  /* A76 Sender  (Postbox) */
#define MHU_A76_MBX         (MHU_REC_REG + MHU_BLOCK_SIZE)  /* A76 Receiver (Mailbox) */

/* A76 SPI allocations (per inter.md:
 *   SPI[46] = MHU sender   mhus_pbx_int  (INT_ID  78, postbox combo)
 *   SPI[78] = MHU receiver mhur_mbx_int  (INT_ID 110, mailbox combo) */
#define GIC_SPI_MHU_A76_TX 46   /* sender (PBX) */
#define GIC_SPI_MHU_A76_RX 78   /* receiver (MBX) */
#define GIC_INTID_MHU_A76  (32U + GIC_SPI_MHU_A76_RX)

/* AARCH64 timer PPI */
#define ARCH_TIMER_NS_EL1_IRQ  (16U + 14)  /* PPI 30 */

#endif /* FW_A76_ADDR_MAP_H */
