/*
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * MHU-DavarAE firmware driver — common to all cores.
 *
 * Register layout matches mhu_davarae SystemC model.
 * RAS / TrustZone bits omitted.
 *
 * Address map (per side register frame):
 *   +0x1000  doorbell windows (PDBCW/MDBCW)
 *   +0x2000  FIFO windows (PFFCW/MFFCW)
 *   +0x3000  fast channel PAY32
 *
 * SoC data windows (separate from reg frames):
 *   MHU_SND_DATA / MHU_REC_DATA — 64 KiB SRAM; use with KICK_DATA.
 */

#ifndef FW_COMMON_MHU_DAVARAE_H
#define FW_COMMON_MHU_DAVARAE_H

#include <stdint.h>

/* ===================== PBX (Sender / Postbox) offsets ===================== */
#define MHU_PBX_DBCH_CFG0           0x20u
#define MHU_PBX_FFCH_CFG0           0x30u
#define MHU_PBX_FCH_CFG0            0x40u
#define MHU_PBX_IIDR                0xFC8u

#define MHU_PBX_DBCW_BASE(n)        (0x1000u + 32u * (n))
#define MHU_PBX_DBCW_ST(n)          (MHU_PBX_DBCW_BASE(n) + 0x0000u)
#define MHU_PBX_DBCW_SET(n)         (MHU_PBX_DBCW_BASE(n) + 0x000Cu)
#define MHU_PBX_DBCW_INT_ST(n)      (MHU_PBX_DBCW_BASE(n) + 0x0010u)
#define MHU_PBX_DBCW_INT_CLR(n)     (MHU_PBX_DBCW_BASE(n) + 0x0014u)
#define MHU_PBX_DBCW_INT_EN(n)      (MHU_PBX_DBCW_BASE(n) + 0x0018u)
#define MHU_PBX_DBCW_CTRL(n)        (MHU_PBX_DBCW_BASE(n) + 0x001Cu)

/* FIFO channel window n: PFFCW @ 0x2000 + 64*n */
#define MHU_PBX_FFCW_BASE(n)        (0x2000u + 64u * (n))
#define MHU_PBX_FFCW_PAY(n)         (MHU_PBX_FFCW_BASE(n) + 0x00u)
#define MHU_PBX_FFCW_FLG(n)         (MHU_PBX_FFCW_BASE(n) + 0x08u)
#define MHU_PBX_FFCW_INT_ST(n)      (MHU_PBX_FFCW_BASE(n) + 0x10u)
#define MHU_PBX_FFCW_INT_CLR(n)     (MHU_PBX_FFCW_BASE(n) + 0x14u)
#define MHU_PBX_FFCW_INT_EN(n)      (MHU_PBX_FFCW_BASE(n) + 0x18u)
#define MHU_PBX_FFCW_CTRL(n)        (MHU_PBX_FFCW_BASE(n) + 0x20u)
#define MHU_PBX_FFCW_ST(n)          (MHU_PBX_FFCW_BASE(n) + 0x24u)
#define MHU_PBX_FFCW_TIDE(n)        (MHU_PBX_FFCW_BASE(n) + 0x2Cu)
#define MHU_PBX_FFCW_DATA_OFF(n)    (MHU_PBX_FFCW_BASE(n) + 0x30u)
#define MHU_PBX_FFCW_DATA_LEN(n)    (MHU_PBX_FFCW_BASE(n) + 0x34u)

#define MHU_PBX_FCH_PAY32(n)        (0x3000u + 4u * (n))
#define MHU_PBX_FCTRL               0xF000u

/* ===================== MBX (Receiver / Mailbox) offsets ===================== */
#define MHU_MBX_DBCH_CFG0           0x20u
#define MHU_MBX_FFCH_CFG0           0x30u
#define MHU_MBX_FCH_CFG0            0x40u
#define MHU_MBX_DBCH_CTRL           0x130u
#define MHU_MBX_DBG_INT_EN          0x134u
#define MHU_MBX_FCH_CTRL            0x140u
#define MHU_MBX_FCG_INT_EN          0x144u
#define MHU_MBX_DBCH_INT_ST(n)      (0x400u + 4u * (n))
#define MHU_MBX_FFCH_INT_ST(n)      (0x410u + 4u * (n))
#define MHU_MBX_FCG_INT_ST          0x470u
#define MHU_MBX_FCH_GRP_INT_ST(n)   (0x480u + 4u * (n))
#define MHU_MBX_IIDR                0xFC8

#define MHU_MBX_DBCW_BASE(n)        (0x1000u + 32u * (n))
#define MHU_MBX_DBCW_ST(n)          (MHU_MBX_DBCW_BASE(n) + 0x0000u)
#define MHU_MBX_DBCW_CLR(n)         (MHU_MBX_DBCW_BASE(n) + 0x0008u)
#define MHU_MBX_DBCW_SET(n)         (MHU_MBX_DBCW_BASE(n) + 0x000Cu)
#define MHU_MBX_DBCW_INT_ST(n)      (MHU_MBX_DBCW_BASE(n) + 0x0010u)
#define MHU_MBX_DBCW_INT_CLR(n)     (MHU_MBX_DBCW_BASE(n) + 0x0014u)
#define MHU_MBX_DBCW_INT_EN(n)      (MHU_MBX_DBCW_BASE(n) + 0x0018u)
#define MHU_MBX_DBCW_CTRL(n)        (MHU_MBX_DBCW_BASE(n) + 0x001Cu)

#define MHU_MBX_FFCW_BASE(n)        (0x2000u + 64u * (n))
#define MHU_MBX_FFCW_PAY(n)         (MHU_MBX_FFCW_BASE(n) + 0x00u)
#define MHU_MBX_FFCW_FLG(n)         (MHU_MBX_FFCW_BASE(n) + 0x08u)
#define MHU_MBX_FFCW_INT_ST(n)      (MHU_MBX_FFCW_BASE(n) + 0x10u)
#define MHU_MBX_FFCW_INT_CLR(n)     (MHU_MBX_FFCW_BASE(n) + 0x14u)
#define MHU_MBX_FFCW_INT_EN(n)      (MHU_MBX_FFCW_BASE(n) + 0x18u)
#define MHU_MBX_FFCW_CTRL(n)        (MHU_MBX_FFCW_BASE(n) + 0x20u)
#define MHU_MBX_FFCW_ST(n)          (MHU_MBX_FFCW_BASE(n) + 0x24u)
#define MHU_MBX_FFCW_FIFO_POP(n)    (MHU_MBX_FFCW_BASE(n) + 0x28u)

#define MHU_MBX_FCH_PAY32(n)        (0x3000u + 4u * (n))
#define MHU_MBX_FCTRL               0xF000u

/* FIFO CTRL / INT bits (match model) */
#define MHU_FF_CTRL_RA_EN           (1u << 1)
#define MHU_FF_CTRL_FLUSH           (1u << 2)
#define MHU_FF_CTRL_KICK_DATA       (1u << 8)
#define MHU_FF_INT_ACK              (1u << 0)

/* FIFO offset in shared memory */
#define MHU_FF_OFFSET(ch)           ((ch) * 1024u)

/* ===================== Accessors ===================== */

static inline void mhu_write32(uintptr_t addr, uint32_t value)
{
    *(volatile uint32_t*)addr = value;
}

static inline uint32_t mhu_read32(uintptr_t addr)
{
    return *(volatile uint32_t*)addr;
}

/* ---- Sender (PBX) helpers ---- */

static inline void mhu_send_doorbell(uintptr_t pbx_base, unsigned ch, uint32_t bits)
{
    mhu_write32(pbx_base + MHU_PBX_DBCW_SET(ch), bits);
}

static inline uint32_t mhu_sender_status(uintptr_t pbx_base, unsigned ch)
{
    return mhu_read32(pbx_base + MHU_PBX_DBCW_ST(ch));
}

/* ---- Receiver (MBX) helpers ---- */

static inline uint32_t mhu_receiver_status(uintptr_t mbx_base, unsigned ch)
{
    return mhu_read32(mbx_base + MHU_MBX_DBCW_ST(ch));
}

static inline uint32_t mhu_receiver_int_st(uintptr_t mbx_base, unsigned ch)
{
    return mhu_read32(mbx_base + MHU_MBX_DBCW_INT_ST(ch));
}

static inline void mhu_receiver_clear_irq(uintptr_t mbx_base, unsigned ch, uint32_t bits)
{
    /* 真实 DavarAE 规范:MDBCW_CLR 位于 +0x4(写1清除门铃位),而 +0x0 的
     * MDBCW_ST 是只读状态寄存器,+0x14 的 INT_CLR 只清中断状态、不清门铃
     * 位。若门铃位残留,wait_event 合并硬件状态时会反复读到已消费的事件
     * (假阳性),ACK 流控失效 → 对端连发多包覆盖 fast channel 单值通知
     * (实测 CREATE_PROCESS 被 CREATE_QUEUE 覆盖);且残留位令后续 SET 无
     * 0→1 边沿,门铃中断丢失(A76 等 ACK 卡死)。因此必须写 MDBCW_CLR(+0x4)
     * 清除门铃位,再清中断状态。 */
    mhu_write32(mbx_base + MHU_MBX_DBCW_ST(ch), bits);    /* W1C,兼容部分实现 */
    mhu_write32(mbx_base + MHU_MBX_DBCW_CLR(ch), bits);   /* MDBCW_CLR 写1清除 */
    mhu_write32(mbx_base + MHU_MBX_DBCW_INT_CLR(ch), bits);
}

static inline void mhu_receiver_enable_irq(uintptr_t mbx_base, unsigned ch, uint32_t bits)
{
    mhu_write32(mbx_base + MHU_MBX_DBCW_INT_EN(ch), bits);
}

/* ---- Fast channel helpers ---- */

static inline void mhu_send_fast(uintptr_t pbx_base, unsigned fch, uint32_t value)
{
    mhu_write32(pbx_base + MHU_PBX_FCH_PAY32(fch), value);
}

static inline uint32_t mhu_receive_fast(uintptr_t mbx_base, unsigned fch)
{
    return mhu_read32(mbx_base + MHU_MBX_FCH_PAY32(fch));
}

static inline void mhu_receiver_clear_fast_irq(uintptr_t mbx_base)
{
    mhu_write32(mbx_base + MHU_MBX_DBCW_INT_CLR(0), (1u << 31));
}

/* ---- FIFO helpers (PAY streaming + SoC KICK_DATA bulk) ---- */

static inline void mhu_fifo_enable_rx_irq(uintptr_t mbx_base, unsigned ch, uint32_t bits)
{
    mhu_write32(mbx_base + MHU_MBX_FFCW_INT_EN(ch), bits);
    mhu_write32(mbx_base + MHU_MBX_FFCW_CTRL(ch), MHU_FF_CTRL_RA_EN);
}

static inline void mhu_fifo_push32(uintptr_t pbx_base, unsigned ch, uint32_t value)
{
    mhu_write32(pbx_base + MHU_PBX_FFCW_PAY(ch), value);
}

static inline uint32_t mhu_fifo_pop32(uintptr_t mbx_base, unsigned ch)
{
    return mhu_read32(mbx_base + MHU_MBX_FFCW_PAY(ch));
}

static inline uint32_t mhu_fifo_rx_fill(uintptr_t mbx_base, unsigned ch)
{
    return (mhu_read32(mbx_base + MHU_MBX_FFCW_ST(ch)) & 0x7FF);
}

static inline void mhu_fifo_clear_rx_irq(uintptr_t mbx_base, unsigned ch, uint32_t bits)
{
    mhu_write32(mbx_base + MHU_MBX_FFCW_INT_CLR(ch), bits);
}

/* Bulk: write payload into snd_data[off..], then kick via PBX FFCW */
static inline void mhu_fifo_kick_data(uintptr_t pbx_base, unsigned ch,
                                      uint32_t data_off, uint32_t data_len)
{
    mhu_write32(pbx_base + MHU_PBX_FFCW_DATA_OFF(ch), data_off);
    mhu_write32(pbx_base + MHU_PBX_FFCW_DATA_LEN(ch), data_len);
    mhu_write32(pbx_base + MHU_PBX_FFCW_CTRL(ch), MHU_FF_CTRL_KICK_DATA);
}

#endif
