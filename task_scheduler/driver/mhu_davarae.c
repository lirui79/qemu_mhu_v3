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
#include <stddef.h>
#include "system.h"
#include "mhu_davarae.h"

volatile uint32_t g_mbx_db_stat_0;      //status of doorbell channel 0
volatile uint32_t g_mbx_ff_stat_0;      //status of fifo channel 0~31
volatile uint32_t g_mbx_fc_stat_0;      //status of fast channel 0~31
volatile uint32_t g_mbx_fc_data_0[32];  //data of fast channel 0~31

volatile irq_callback_t irq_callbacks[3] = {NULL};
/* polling timeout in milliseconds */
#define MHU_WAIT_TIMEOUT    (500)

/* MFFCW_CTRL register bits */
#define MFFCW_CTRL_FF           (1U << 31U)
#define MFFCW_CTRL_MSBF         (1U << 1U)
#define MFFCW_CTRL_MBX_COMB_EN  (1U << 0U)

/* MFFCW_FLG32 field masks */
#define MFFCW_FLG_VFLG(m)       (1U << (2U + 4U*(m)))
#define MFFCW_FLG_FLG_SHIFT(m)  (0U + 4U*(m))
#define MFFCW_FLG_FLG_MASK      0x3U

/* FLG field encoding */
#define MHU_FLG_PAYLOAD     0x00U
#define MHU_FLG_SOT         0x01U
#define MHU_FLG_EOT         0x02U
#define MHU_FLG_SOT_EOT     0x03U


int mhu_init(void) {
    uint32_t i, dbch, ffch, fch, iidr;

    dbch = (mhu_read32(MHU_MBX_BASE + MHU_MBX_DBCH_CFG0) & 0xFF) + 1; //num of doorbell channel
    ffch = (mhu_read32(MHU_MBX_BASE + MHU_MBX_FFCH_CFG0) & 0xFF) + 1; //num of fifo channel
    fch  = (mhu_read32(MHU_MBX_BASE + MHU_MBX_FCH_CFG0) & 0x3FF) + 1; //num of fast channel num
    iidr = mhu_read32(MHU_MBX_BASE + MHU_MBX_IIDR);
    ts_info("MHU: dbch=%u ffch=%u fch=%u iidr=0x%x\n", dbch, ffch, fch, iidr);

    /* clear variables */
    g_mbx_db_stat_0 = 0;
    g_mbx_ff_stat_0 = 0;
    g_mbx_fc_stat_0 = 0;
    for (i = 0; i < 32; i++) {
        g_mbx_fc_data_0[i] = 0;
    }

    /* Enable MHU combo interrupt.
     * 优先级数值必须 >= configMAX_API_CALL_INTERRUPT_PRIORITY<<portPRIORITY_SHIFT
     * (10<<4=160),否则 ISR 内调用 FreeRTOS API 时 vPortValidateInterruptPriority()
     * 会 assert。INT_PRIO_NORMAL=0x7F(127)<160,改用 INT_PRIO_LOW=0xBF(191)。 */
    //interrupt_enable(IRQ_ID_MHU_PBX_COMB, INT_PRIO_LOW);
    interrupt_enable(IRQ_ID_MHU_MBX_COMB, INT_PRIO_LOW);

    /* Enable MBX doorbell channel interrupt */
    mhu_write32(MHU_MBX_BASE + MHU_MBX_DBCH_CTRL,   0x4);      //INT_EN
    mhu_write32(MHU_MBX_BASE + MHU_MBX_DBG_INT_EN,  0x1);      //DB group 0

    /* Clear stale FC interrupts, then enable */
    for (i = 0; i < fch && i < 32; i++) {
        mhu_write32(MHU_MBX_BASE + MHU_MBX_FCH_PAY32(i), 0);
    }
    mhu_write32(MHU_MBX_BASE + MHU_MBX_FCH_CTRL,   0x4);      //INT_EN
    mhu_write32(MHU_MBX_BASE + MHU_MBX_FCG_INT_EN, 0x1);

    /* Clear stale FF interrupts before enabling, then enable */
    for (i = 0; i < ffch; i++) {
        uint32_t stale = mhu_read32(MHU_MBX_BASE + MHU_MBX_FFCW_INT_ST(i));
        if (stale)
            mhu_write32(MHU_MBX_BASE + MHU_MBX_FFCW_INT_CLR(i), stale);
        mhu_write32(MHU_MBX_BASE + MHU_MBX_FFCW_INT_EN(i), 0xFFFFFFFF);
        stale = mhu_read32(MHU_MBX_BASE + MHU_MBX_FFCW_CTRL(i));
        mhu_write32(MHU_MBX_BASE + MHU_MBX_FFCW_CTRL(i), stale | 0xF);
    }

    return TS_OK;
}

void mhu_set_irq_callback(uint32_t callback_type, irq_callback_t callback) {
    if (callback_type > 2) {
        ts_warn("warn:MHU invalid callback_type %u\n", callback_type);
        return;
    }
    irq_callbacks[callback_type] = callback;
}

/**
 * MHU postbox interrupt handler
 */
void mhu_pbx_isr(void) {
    ts_dbg("PBX_INT\n");
}

/**
 * Poll-and-merge MBX hardware status into the software flags.
 *
 * 与 mhu_wait_event 对 doorbell 的做法一致:当 R52 的 GIC 未投递 MBX 组合
 * 中断时(实际观察到 R52 收不到任何中断),ISR 不会运行,g_mbx_*_stat_0 永远
 * 不会被置位,command_processor 将卡死在 WFI。此函数在命令循环里被周期调用,
 * 直接把硬件锁存状态(DB/FC/FF)并入软件标志,兜住这条没有中断的路径。
 *
 * 注意:FC 状态寄存器 FCH_GRP_INT_ST 读回后需把对应 FCH_PAY32 写 0 才能清除
 * 锁存边沿(与 mhu_mbx_isr 一致),否则后续 FC 不会再被识别。
 */
void mhu_poll_rx(void) {
    uint32_t flags = arch_local_irq_save();
    uint32_t db_int = mhu_read32(MHU_MBX_BASE + MHU_MBX_DBCH_INT_ST(0));
    uint32_t fc_int = mhu_read32(MHU_MBX_BASE + MHU_MBX_FCH_GRP_INT_ST(0));
    uint32_t ff_int = mhu_read32(MHU_MBX_BASE + MHU_MBX_FFCH_INT_ST(0));
    uint32_t i, stat, v;
    arch_local_irq_restore(flags);
    if (db_int != 0) {
        for (i = 0; i < 4; i++) {
            if (db_int & (1u << i)) {
                flags = arch_local_irq_save();
                stat = mhu_receiver_status(MHU_MBX_BASE, i);
                mhu_receiver_clear_irq(MHU_MBX_BASE, i, stat);
                if (i == 0)
                    g_mbx_db_stat_0 |= stat;
                arch_local_irq_restore(flags);
                if (irq_callbacks[0]) {
                    irq_callbacks[0](0, i);
                }
            }
        }
    }

    if (fc_int != 0) {
        for (i = 0; i < 32; i++) {
            if (fc_int & (1u << i)) {
                flags = arch_local_irq_save();
                v = mhu_read32(MHU_MBX_BASE + MHU_MBX_FCH_PAY32(i));
                /* write 0 to clear FC interrupt latch in hardware */
                mhu_write32(MHU_MBX_BASE + MHU_MBX_FCH_PAY32(i), 0);
                /*
                 * 防御 ISR 与 poll 双重处理同一个 FC 边沿:先处理方已把
                 * PAY32 读到有效元数据并写 0 清锁存;后处理方仍会看到
                 * FCH_GRP_INT_ST 残留位,但此时 PAY32 已为 0。若无条件
                 * 写入,会把有效元数据覆盖为 0(实测 SW query 的
                 * fc=0x2 被覆盖成 0x0 → drain dwlen=0 → 忙循环,A76
                 * 等不到响应死锁)。仅当读回值非 0 时(真正的通知)
                 * 才更新数据并置位统计位;残留边沿(PAY32==0)只清
                 * 锁存、不置位,避免无限 drain 忙循环。
                 */
                if (v != 0) {
                    g_mbx_fc_data_0[i] = v;
                    g_mbx_fc_stat_0 |= (1u << i);
                }
                arch_local_irq_restore(flags);
                if (irq_callbacks[1]) {
                    irq_callbacks[1](0, i);
                }
            }
        }
    }

    if (ff_int != 0) {
        for (i = 0; i < 4; i++) {
            if (ff_int & (1u << i)) {
                flags = arch_local_irq_save();
                stat = mhu_read32(MHU_MBX_BASE + MHU_MBX_FFCW_INT_ST(i));
                /* FIFO 中断是电平触发(fill>0 条件持续),仅写 INT_CLR
                 * 不能阻止中断重入——数据仍在 FIFO 里,INT_CLR 写完
                 * 立刻被硬件重新置位。必须禁用通道中断,等 recv 线程
                 * 排空 FIFO 后再重新使能。 */
                mhu_write32(MHU_MBX_BASE + MHU_MBX_FFCW_INT_EN(i), 0);
                mhu_fifo_clear_rx_irq(MHU_MBX_BASE, i, stat);
                arch_local_irq_restore(flags);
                if (irq_callbacks[2]) {
                    irq_callbacks[2](0, i);
                }
            }
        }
        g_mbx_ff_stat_0 |= ff_int;
    }
//    ts_dbg("MBX_INT: db=0x%x (0x%x) fc=0x%x ff=0x%x\n", db_int, g_mbx_db_stat_0, fc_int, ff_int);
}

/**
 * MHU mailbox interrupt handler
 */
void mhu_mbx_isr(void) {
    uint32_t db_int = mhu_read32(MHU_MBX_BASE + MHU_MBX_DBCH_INT_ST(0));
    uint32_t fc_int = mhu_read32(MHU_MBX_BASE + MHU_MBX_FCH_GRP_INT_ST(0));
    uint32_t ff_int = mhu_read32(MHU_MBX_BASE + MHU_MBX_FFCH_INT_ST(0));
    uint32_t i, stat;

    if (db_int != 0) {
        for (i = 0; i < 4; i++) {
            if (db_int & (1 << i)) {
                stat = mhu_receiver_status(MHU_MBX_BASE, i);
                mhu_receiver_clear_irq(MHU_MBX_BASE, i, stat);
                if (i == 0) {
                    g_mbx_db_stat_0 |= stat;
                }

                if (irq_callbacks[0]) {
                    irq_callbacks[0](1, i);
                }
            }
        }
    }

    if (fc_int != 0) {
        for (i = 0; i < 32; i++) {
            if (fc_int & (1 << i)) {
                uint32_t v = mhu_read32(MHU_MBX_BASE + MHU_MBX_FCH_PAY32(i));
                /* write 0 to clear FC interrupt latch in hardware,
                 * otherwise subsequent FC notifications won't
                 * generate a new combo-interrupt edge */
                mhu_write32(MHU_MBX_BASE + MHU_MBX_FCH_PAY32(i), 0);
                /* 与 mhu_poll_rx 相同的防御:同一 FC 边沿若被 poll/ISR
                 * 双重处理,后处理方读到 PAY32==0,不得覆盖有效元数据,
                 * 也不得置位 fc_stat(否则 drain 忙循环)。 */
                if (v != 0) {
                    g_mbx_fc_data_0[i] = v;
                    g_mbx_fc_stat_0 |= (1u << i);
                }

                if (irq_callbacks[1]) {
                    irq_callbacks[1](1, i);
                }
            }
        }
    }

    if (ff_int != 0) {
        for (i = 0; i < 4; i++) {
            if (ff_int & (1 << i)) {
                stat = mhu_read32(MHU_MBX_BASE + MHU_MBX_FFCW_INT_ST(i));
                /* 与 mhu_poll_rx 同理:FIFO 中断是电平触发,INT_CLR 不能
                 * 阻止重入,必须禁用通道中断,recv 排空后再使能。 */
                mhu_write32(MHU_MBX_BASE + MHU_MBX_FFCW_INT_EN(i), 0);
                mhu_fifo_clear_rx_irq(MHU_MBX_BASE, i, stat);
                if (irq_callbacks[2]) {
                    irq_callbacks[2](1, i);
                }
            }
        }
        g_mbx_ff_stat_0 |= ff_int;
    }
//    ts_printf("MBX_INT: db=0x%x (0x%x) fc=0x%x ff=0x%x\n", db_int, g_mbx_db_stat_0, fc_int, ff_int);
}

/**
 * Send event using doorbell channel
 */
void mhu_send_event(uint32_t ch, uint32_t event) {
    mhu_send_doorbell(MHU_PBX_BASE, ch, event);
}

/**
 * Wait for event using doorbell channel
 */
uint32_t mhu_wait_event(uint32_t ch, uint32_t event) {
    uint32_t ev = 0;
    uint32_t flags;
    uint64_t timeout = arch_get_time_ms() + MHU_WAIT_TIMEOUT;
    if (ch == 0) {
        while (1) {
            /*
             * 门铃事件主要来自 ISR / mhu_poll_rx 置位的软件状态
             * g_mbx_db_stat_0;此处每次醒来再合并一次硬件原始门铃位
             * DBCW_ST,兜住门铃中断因与 FC/FF 突发竞态而丢失的情况。
             * 因 mhu_receiver_clear_irq 已写 MDBCW_CLR(+0x4)真正清除已
             * 消费的门铃位,合并读到的只可能是真实的新事件,不会产生
             * 假阳性。只等目标位(event),不能等任意非零位,否则残留的
             * 无关事件位会导致忙等死循环或 ACK 错配。 */
            flags = arch_local_irq_save();
            g_mbx_db_stat_0 |= mhu_receiver_status(MHU_MBX_BASE, ch);
            arch_local_irq_restore(flags);
            while (!(g_mbx_db_stat_0 & event)) {
                __asm volatile ("WFI");
                flags = arch_local_irq_save();
                g_mbx_db_stat_0 |= mhu_receiver_status(MHU_MBX_BASE, ch);
                arch_local_irq_restore(flags);
                if (time_after(timeout))
                    return 0;
            }
            flags = arch_local_irq_save();
            ev = g_mbx_db_stat_0 & event;
            if (ev) {
                mhu_receiver_clear_irq(MHU_MBX_BASE, ch, ev);
                g_mbx_db_stat_0 &= ~ev;
            }
            arch_local_irq_restore(flags);
            if (ev)
                break;
        }
    }
    return ev;
}

/**
 * Clear pending event bits on receiver doorbell channel.
 */
void mhu_clear_event(uint32_t ch, uint32_t event) {
    uint32_t stat;
    uint32_t ev;
    uint32_t flags;
    if (ch == 0) {
        flags = arch_local_irq_save();
        g_mbx_db_stat_0 &= ~event;
        arch_local_irq_restore(flags);
    }

    stat = mhu_receiver_status(MHU_MBX_BASE, ch);
    ev = (stat | g_mbx_db_stat_0) & event;
    if (!ev) {
        return;
    }

    flags = arch_local_irq_save();
    if (stat & ev)
        mhu_receiver_clear_irq(MHU_MBX_BASE, ch, stat & ev);
    g_mbx_db_stat_0 &= ~ev;
    arch_local_irq_restore(flags);
}

/**
 * send data using fifo channel
 */
uint32_t mhu_send_data(uint32_t ch, void *data_ptr, uint32_t data_len) {
    uint32_t *dwptr = (uint32_t*)data_ptr;
    uint32_t  dwlen = data_len / 4;
    uint32_t  i, flg, free, irq_st;
    uint64_t  timeout = arch_get_time_ms() + MHU_WAIT_TIMEOUT;

    ts_assert((data_len % 4) == 0);

    /* wait until fifo has enough space */
    do {
        free = mhu_read32(MHU_PBX_BASE + MHU_PBX_FFCW_PAY(ch)) & 0x7FF;
    } while ((free < data_len) && time_before(timeout));
    if (free < data_len) {
        ts_warn("mhu: fifo%u timeout on full (%u < %u)\n", ch, free, data_len);
        return 0;
    }

    irq_st = arch_local_irq_save();
    /* 平台 MHU 模型:数据逐 word 经 PBX FIFO(push32)送达对端 FIFO,
     * 每 word 需写 FFCW_FLG 标记 SOT(首)/EOT+ACK(末)。不能用
     * KICK_DATA+snd_data(那是被禁用的 #else 路径,数据不会进入 FIFO)。 */
    for (i = 0; i < dwlen; i++) {
        flg = 0;
        if (i == 0)
            flg |= 0x02;    /* SOT */
        if (i == (dwlen - 1))
            flg |= 0x04;    /* EOT+ACK */
        mhu_write32(MHU_PBX_BASE + MHU_PBX_FFCW_FLG(ch), flg);
        mhu_fifo_push32(MHU_PBX_BASE, ch, dwptr[i]);
    }
    arch_local_irq_restore(irq_st);
    return data_len;
}

/**
 * receive ONE complete fifo packet(SOT ~ EOT) from fifo channel
 * @param ch fifo channel id
 * @param buf_ptr user receive buffer
 * @param buf_len buffer size(bytes, 4‑bytes aligned)
 * @return >0: valid packet byte length; 0: no packet / timeout / error
 *
 * Reference: ARM‑AES‑0072 MHU‑DavarAE spec
 * Hardware rule: POP PAY first, then read MFFCW_FLG32.
 * FHB can cache up to 4 history entries (FLG0‑FLG3).
 * MSBF bit in MFFCW_CTRL decides which FLG entry corresponds to latest pop word.
 */
uint32_t mhu_recv_data(uint32_t ch, void *buf_ptr, uint32_t buf_len) {
    uint32_t *dwptr = (uint32_t*)buf_ptr;
    uint32_t  dwlen = (buf_len / 4U);
    uint32_t  fill, val, len = 0U;
    uint32_t  flg_val, flg32_reg, ctrl_reg, vflg_valid;
    uint8_t   sot = 0U, eot = 0U;
    uint64_t  timeout = arch_get_time_ms() + MHU_WAIT_TIMEOUT;

    sot = 0U;
    eot = 0U;
    len = 0U;

    /* wait for at least one word in fifo */
    do {
        fill = mhu_fifo_rx_fill(MHU_MBX_BASE, ch);
        if(time_after(timeout)) {
            if (fill >= CMD_MSG_MIN_SIZE) {
                break;
            }

            //ts_printf("mhu: fifo%u timeout on empty\n", ch);
            return 0U;
        }
    } while (fill < CMD_MSG_MIN_SIZE);

    while (1) {
        if (len >= dwlen) {
            ts_printf("MBX: recv buffer overflow ch=%u buf_len=%u\n", ch, buf_len);
            return 0U;
        }

        /* wait for at least one word in fifo */
        do {
            fill = mhu_fifo_rx_fill(MHU_MBX_BASE, ch);
            if(time_after(timeout)) {
                if (fill > 0) {
                    break;
                }

                //ts_printf("mhu: fifo%u timeout on empty\n", ch);
                return 0U;
            }
        } while (!fill);

        flg_val = arch_local_irq_save();
        /* 【硬件强制顺序】1.pop PAY */
        val   = mhu_fifo_pop32(MHU_MBX_BASE, ch);
        /* 【硬件强制顺序】2.immediately read FLG32 */
        flg32_reg = mhu_read32(MHU_MBX_BASE + MHU_MBX_FFCW_FLG(ch));
        /* read MSBF runtime to decide which FHB entry is latest popped word */
        ctrl_reg  = mhu_read32(MHU_MBX_BASE + MHU_MBX_FFCW_CTRL(ch));
        arch_local_irq_restore(flg_val);

         /* MSBF=0 → latest pop word is FLG0; MSBF=1 → latest pop word is FLG3 */
        if ((sot == 0) || (eot == 0)) {
            vflg_valid = ((flg32_reg & MFFCW_FLG_VFLG(0)) != 0U);
            if (vflg_valid) {
                flg_val    = (flg32_reg >> MFFCW_FLG_FLG_SHIFT(0)) & MFFCW_FLG_FLG_MASK;
                if (sot == 0) {
                    sot = ((flg_val == MHU_FLG_SOT) || (flg_val == MHU_FLG_SOT_EOT)) ? 1U : 0U;
                }
                if (eot == 0) {
                    eot = ((flg_val == MHU_FLG_EOT) || (flg_val == MHU_FLG_SOT_EOT)) ? 1U : 0U;
                }
            }
        }

        if ((sot == 0) || (eot == 0)) {
            vflg_valid = ((flg32_reg & MFFCW_FLG_VFLG(3)) != 0U);
            if (vflg_valid) {
                flg_val    = (flg32_reg >> MFFCW_FLG_FLG_SHIFT(3)) & MFFCW_FLG_FLG_MASK;
                if (sot == 0) {
                    sot = ((flg_val == MHU_FLG_SOT) || (flg_val == MHU_FLG_SOT_EOT)) ? 1U : 0U;
                }
                if (eot == 0) {
                    eot = ((flg_val == MHU_FLG_EOT) || (flg_val == MHU_FLG_SOT_EOT)) ? 1U : 0U;
                }
            }
        }

        /* drop garbage data before SOT arrives */
        if(!sot && len == 0U) {
            ts_printf("MBX: drop pre-SOT garbage word 0x%08x ch=%u\n", val, ch);
            continue;
        }

        dwptr[len++] = val;

        if(eot) {
            /* complete one full packet */
            break;
        }
    }

    flg_val = arch_local_irq_save();
    /* 收完一包后必须无条件清中断状态并使能本通道中断。
     * IRQ handler 进入时会关闭该通道 INT_EN(电平触发, 防中断风暴), 而接收线程
     * mhu_v3_wait_event_interruptible 没有超时, 只能靠中断唤醒。若此处因为 FIFO
     * 里残留了下一个包的前半部分(0 < fill < CMD_MSG_MIN_SIZE)而不使能中断, 对端
     * 把剩余 word push 进来时就不会再产生中断, 接收线程将永久睡眠, 该包(应答)
     * 永远不会被取出 -> 上层表现为请求 "timeout!"。
     * 残留数据 >= CMD_MSG_MIN_SIZE 时, 等待条件本身就会立即返回并收取, 因此
     * 无条件使能不会重复处理已经收到的数据。 */
    val = mhu_read32(MHU_MBX_BASE + MHU_MBX_FFCW_INT_ST(ch));
    if (val)
        mhu_fifo_clear_rx_irq(MHU_MBX_BASE, ch, val);
    mhu_write32(MHU_MBX_BASE + MHU_MBX_FFCW_INT_EN(ch), 0xFFFFFFFFU);
    g_mbx_ff_stat_0 &= ~(1U << ch);
    arch_local_irq_restore(flg_val);
    return (len * 4U);
}

/**
 * Check if fifo data is ready
 */
int mhu_is_data_ready(uint32_t ch) {
    if (g_mbx_ff_stat_0 & (1 << ch))
        return 1;
    return 0;
}

/**
 * Send event using fast channel
 */
void mhu_send_fast_event(uint32_t ch, uint32_t value) {
    mhu_send_fast(MHU_PBX_BASE, ch, value);
}

/**
 * Return events triggerred by fast channel
 */
uint32_t mhu_take_fast_events(void) {
    uint32_t flags;
    uint32_t events;

    flags = arch_local_irq_save();
    events = g_mbx_fc_stat_0;
    g_mbx_fc_stat_0 = 0;
    arch_local_irq_restore(flags);
    return events;
}

/**
 * Get value of fast channel
 */
uint32_t mhu_get_fast_event_value(uint32_t ch) {
    if (ch < 32) {
        return g_mbx_fc_data_0[ch];
    }
    return 0;
}
