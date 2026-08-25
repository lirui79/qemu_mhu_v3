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

#include "system.h"
#include "mhu_davarae.h"
#include "FreeRTOS.h"
#include "task.h"


volatile uint32_t g_mbx_db_stat_0;      //status of doorbell channel 0
volatile uint32_t g_mbx_ff_stat_0;      //status of fifo channel 0~31
volatile uint32_t g_mbx_fc_stat_0;      //status of fast channel 0~31
volatile uint32_t g_mbx_fc_data_0[32];  //data of fast channel 0~31

volatile irq_callback_t irq_callbacks[3] = {NULL};


int mhu_init(void)
{
    uint32_t i, dbch, ffch, fch, iidr;

    dbch = (mhu_read32(MHU_MBX_BASE + MHU_MBX_DBCH_CFG0) & 0xFF) + 1; //num of doorbell channel
    ffch = (mhu_read32(MHU_MBX_BASE + MHU_MBX_FFCH_CFG0) & 0xFF) + 1; //num of fifo channel
    fch  = (mhu_read32(MHU_MBX_BASE + MHU_MBX_FCH_CFG0) & 0x3FF) + 1; //num of fast channel num
    iidr = mhu_read32(MHU_MBX_BASE + MHU_MBX_IIDR);
    ts_printf("MHU: dbch=%u ffch=%u fch=%u iidr=0x%x\n", dbch, ffch, fch, iidr);

    /* clear variables */
    g_mbx_db_stat_0 = 0;
    g_mbx_ff_stat_0 = 0;
    g_mbx_fc_stat_0 = 0;
    for (i = 0; i < 32; i++) {
        g_mbx_fc_data_0[i] = 0;
    }

    /* Enable MHU combo interrupt.
     * 注意:优先级数值必须 >= configMAX_API_CALL_INTERRUPT_PRIORITY<<portPRIORITY_SHIFT
     * (10<<4=160),否则 vPortValidateInterruptPriority() 在 ISR 内调用
     * vTaskNotifyGiveFromISR() 时 assert(port.c:518/0x206)。
     * INT_PRIO_NORMAL=0x7F(127)<160 会触发,改用 INT_PRIO_LOW=0xBF(191)。 */
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

    /* Clear stale FF interrupts before enabling, then enable.
     * 平台 MHU 模型:接收侧必须对每个 FF 通道写 FFCW_CTRL.RA_EN,
     * 数据才会经"读 MFFCW_PAY"弹出并递减 fill;否则对端 push 的
     * 数据虽入 FIFO,但本侧读 ST 的 fill 恒为 0、无法接收。 */
    for (i = 0; i < ffch; i++) {
        uint32_t stale = mhu_read32(MHU_MBX_BASE + MHU_MBX_FFCW_INT_ST(i));
        if (stale)
            mhu_write32(MHU_MBX_BASE + MHU_MBX_FFCW_INT_CLR(i), stale);
        mhu_write32(MHU_MBX_BASE + MHU_MBX_FFCW_INT_EN(i), 0xFFFFFFFF);
        //mhu_write32(MHU_MBX_BASE + MHU_MBX_FFCW_CTRL(i), MHU_FF_CTRL_RA_EN);
        stale = mhu_read32(MHU_MBX_BASE + MHU_MBX_FFCW_CTRL(i));
        mhu_write32(MHU_MBX_BASE + MHU_MBX_FFCW_CTRL(i), stale | 0xF);
    }

    return 0;
}

void mhu_set_irq_callback(uint32_t callback_type, irq_callback_t callback) {
    if (callback_type > 2) {
        ts_printf("MHU: invalid callback_type %u\n", callback_type);
        return;
    }
    irq_callbacks[callback_type] = callback;
}

/**
 * MHU postbox interrupt handler
 */
void mhu_pbx_isr(void)
{
    //ts_printf("PBX_INT\n");
}

/**
 * MHU mailbox interrupt handler
 */
void mhu_mbx_isr(void)
{
    uint32_t db_int = mhu_read32(MHU_MBX_BASE + MHU_MBX_DBCH_INT_ST(0));
    uint32_t fc_int = mhu_read32(MHU_MBX_BASE + MHU_MBX_FCH_GRP_INT_ST(0));
    uint32_t ff_int = mhu_read32(MHU_MBX_BASE + MHU_MBX_FFCH_INT_ST(0));
    uint32_t i, stat;

    if (db_int != 0) {
        for (i = 0; i < 4; i++) {
            if (db_int & (1 << i)) {
                stat = mhu_receiver_status(MHU_MBX_BASE, i);
                mhu_receiver_clear_irq(MHU_MBX_BASE, i, stat);
                if (i == 0)
                    g_mbx_db_stat_0 |= stat;
            }
        }

        if (irq_callbacks[0]) {
            irq_callbacks[0](1, db_int);
        }
    }
    if (fc_int != 0) {
        for (i = 0; i < 32; i++) {
            if (fc_int & (1 << i)) {
                g_mbx_fc_data_0[i] = mhu_read32(MHU_MBX_BASE + MHU_MBX_FCH_PAY32(i));
                /* write 0 to clear FC interrupt latch in hardware,
                 * otherwise subsequent FC notifications won't
                 * generate a new combo-interrupt edge */
                mhu_write32(MHU_MBX_BASE + MHU_MBX_FCH_PAY32(i), 0);
                /* FC0 只承载 R52 的 consume-ACK 魔数(mhu_send_data 轮询
                 * g_mbx_fc_data_0[0] 消费),不置 stat bit0:否则 ACK 到达会
                 * 令 MHU_FC_SIGNALED 假阳性(响应走 FC1 的 bit1),query 流程
                 * 提前唤醒读 ch1 得 0。 */
                if (i != 0)
                    g_mbx_fc_stat_0 |= (1u << i);
            }
        }

        if (irq_callbacks[1]) {
            irq_callbacks[1](1, fc_int);
        }
    }
    if (ff_int != 0) {
        for (i = 0; i < 4; i++) {
            if (ff_int & (1 << i)) {
                uint32_t ff_int_st = mhu_read32(MHU_MBX_BASE + MHU_MBX_FFCW_INT_ST(i));
                /* FIFO 中断是电平触发(fill>0 条件持续),仅写 INT_CLR
                 * 不能阻止中断重入——数据仍在 FIFO 里,INT_CLR 写完
                 * 立刻被硬件重新置位。必须禁用通道中断,等 recv 线程
                 * 排空 FIFO 后再重新使能。 */
                mhu_write32(MHU_MBX_BASE + MHU_MBX_FFCW_INT_EN(i), 0);
                mhu_fifo_clear_rx_irq(MHU_MBX_BASE, i, ff_int_st);
            }
        }
        g_mbx_ff_stat_0 |= ff_int;

        if (irq_callbacks[2]) {
            irq_callbacks[2](1, ff_int);
        }
    }
    /* DIAG: 打印原始门铃位与中断状态,验证 MDBCW_CLR(+0x4) 是否真正清除 */
    ts_printf("MBX_INT: db=0x%x (0x%x) dbw_st=0x%x dbw_ist=0x%x fc=0x%x ff=0x%x\n",
              db_int, g_mbx_db_stat_0,
              mhu_read32(MHU_MBX_BASE + MHU_MBX_DBCW_ST(0)),
              mhu_read32(MHU_MBX_BASE + MHU_MBX_DBCW_INT_ST(0)),
              fc_int, ff_int);
}

/**
 * Poll mailbox receive status without relying on the combo IRQ.
 *
 * 平台 MBX 组合中断投递不可靠(实测有时 ISR 不触发),若接收线程只依赖
 * irq_callback_fifo 经 ISR 唤醒,对端消息会一直锁存于硬件而无人处理。
 * 本函数显式轮询 DB/FC/FF 中断状态位,与 mhu_mbx_isr 相同地消费硬件并
 * 置软件标志、调用回调,可安全地在任务上下文周期调用,兜住无中断路径。
 * 与 ISR 的 FC0 语义保持一致:FC0 仅承载对端 consume-ACK 魔数,不置 stat bit0。
 */
void mhu_poll_rx(void)
{
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
            }
        }

        if (irq_callbacks[0]) {
            irq_callbacks[0](0, db_int);
        }
    }
    if (fc_int != 0) {
        for (i = 0; i < 32; i++) {
            if (fc_int & (1u << i)) {
                flags = arch_local_irq_save();
                v = mhu_read32(MHU_MBX_BASE + MHU_MBX_FCH_PAY32(i));
                /* write 0 to clear FC interrupt latch in hardware */
                mhu_write32(MHU_MBX_BASE + MHU_MBX_FCH_PAY32(i), 0);
                /* 防御 ISR 与 poll 双重处理同一 FC 边沿:后处理方读到
                 * PAY32==0 时不得覆盖有效元数据,也不得置位 fc_stat,否则
                 * drain 忙循环。FC0 只承载 consume-ACK,永不置 stat bit0。 */
                if ((v != 0) && (i != 0))
                    g_mbx_fc_stat_0 |= (1u << i);
                if (v != 0)
                    g_mbx_fc_data_0[i] = v;
                arch_local_irq_restore(flags);
            }
        }

        if (irq_callbacks[1]) {
            irq_callbacks[1](0, fc_int);
        }
    }
    if (ff_int != 0) {
        flags = arch_local_irq_save();
        for (i = 0; i < 4; i++) {
            if (ff_int & (1u << i)) {
                stat = mhu_read32(MHU_MBX_BASE + MHU_MBX_FFCW_INT_ST(i));
                /* 与 mhu_mbx_isr 同理:FIFO 中断是电平触发,INT_CLR 不能
                 * 阻止重入,必须禁用通道中断,recv 排空后再使能。 */
                mhu_write32(MHU_MBX_BASE + MHU_MBX_FFCW_INT_EN(i), 0);
                mhu_fifo_clear_rx_irq(MHU_MBX_BASE, i, stat);
            }
        }
        g_mbx_ff_stat_0 |= ff_int;
        arch_local_irq_restore(flags);

        if (irq_callbacks[2]) {
            irq_callbacks[2](0, ff_int);
        }
    }
}

/**
 * Send event using doorbell channel
 */
void mhu_send_event(uint32_t ch, uint32_t event)
{
    mhu_send_doorbell(MHU_PBX_BASE, ch, event);
}

/**
 * Wait for event using doorbell channel
 */
uint32_t mhu_wait_event(uint32_t ch, uint32_t event)
{
    uint32_t ev = 0;
    uint64_t flags;
    if (ch == 0) {
        while (1) {
            /*
             * 门铃事件主要来自 ISR 置位的软件状态 g_mbx_db_stat_0;此处每次
             * 醒来再合并一次硬件原始门铃位 DBCW_ST,兜住门铃中断因与 FC/FF
             * 突发竞态而丢失的情况(对端 SET 后原始位必然置位,只是中断边沿
             * 可能错过)。因 mhu_receiver_clear_irq 已写 MDBCW_CLR(+0x4)真正
             * 清除已消费的门铃位,合并读到的只可能是真实的新事件,不会产生
             * 假阳性。只等目标位(event),不能等任意非零位,否则残留的无关
             * 事件位会导致忙等死循环或 ACK 错配。 */
            flags = arch_local_irq_save();
            g_mbx_db_stat_0 |= mhu_receiver_status(MHU_MBX_BASE, ch);
            arch_local_irq_restore(flags);
            while (!(g_mbx_db_stat_0 & event)) {
                __asm__ volatile("wfi" : : : "memory");
                flags = arch_local_irq_save();
                g_mbx_db_stat_0 |= mhu_receiver_status(MHU_MBX_BASE, ch);
                arch_local_irq_restore(flags);
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
 * send data using fifo channel (SRAM bulk path)
 *
 * Writes payload into snd_data[off..], kicks via PBX FFCW,
 * then notifies the peer via fast channel with meta:
 *   fc_value = (off << 16) | dwlen   (off in bytes, dwlen in dwords)
 */
uint32_t mhu_send_data(uint32_t ch, void *data_ptr, uint32_t data_len)
{
    uint32_t *dwptr = (uint32_t*)data_ptr;
    uint32_t  dwlen = data_len / 4;
    uint32_t  i, flg, irq_st;
    if ((data_len % 4) != 0) {
        ts_printf("MHUS: invalid len %u\n", data_len);
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
 * receive data from fifo channel
 *
 * 平台 MHU 模型:KICK_DATA 会把对端 snd_data 的 payload 压入本侧
 * RX FIFO(MFFCW_PAY)并触发 FIFO(FF)中断。数据只存在于 FIFO;
 * rec_data 仅是模型内部/仿真占位,不会收到 payload。因此必须从
 * FIFO 弹出(fill 判长度 + pop32 读取),而不能读 rec_data——
 * 读 rec_data 拿到的是初始填充垃圾,导致 cmdMsg 字段错乱。
 */
uint32_t mhu_fifo_recv_data(uint32_t ch, void *buf_ptr, uint32_t buf_len)
{
    uint32_t *dwptr = (uint32_t*)buf_ptr;
    uint32_t  fill, len;
    uint32_t  i;

    /* skip if no more data in RX FIFO */
    fill = mhu_fifo_rx_fill(MHU_MBX_BASE, ch);
    if (!fill) {
        /* FIFO 空但可能被 ISR 禁用了中断(电平触发防重入),
         * 确保通道中断保持使能,否则后续数据无法触发中断。 */
        mhu_write32(MHU_MBX_BASE + MHU_MBX_FFCW_INT_EN(ch), 0xFFFFFFFF);
        return 0;
    }

    len = (fill > buf_len) ? buf_len : fill;
    if ((len % 4) != 0) {
        ts_printf("MHUR: invalid len %u\n", len);
        return 0;
    }
    ts_printf("RECV: ch=%u fill=%u len=%u buf=%u\n", ch, fill, len, buf_len);

    for (i = 0; i < (len / 4); i++) {
        dwptr[i] = mhu_fifo_pop32(MHU_MBX_BASE, ch);
    }
    /* FIFO 已排空,清除残留中断标志并重新使能通道中断。
     * ISR/poll 检测到 FIFO 数据后禁用了中断(防止电平触发重入),
     * 此处排空后重新使能,等待下一批数据到来。 */
    {
        uint32_t stale = mhu_read32(MHU_MBX_BASE + MHU_MBX_FFCW_INT_ST(ch));
        if (stale)
            mhu_fifo_clear_rx_irq(MHU_MBX_BASE, ch, stale);
        mhu_write32(MHU_MBX_BASE + MHU_MBX_FFCW_INT_EN(ch), 0xFFFFFFFF);
    }
    if (g_mbx_ff_stat_0 & (1u << ch))
        g_mbx_ff_stat_0 &= ~(1u << ch);
    return len;
}

uint32_t mhu_rx_data_fill(uint32_t ch)
{
    return mhu_fifo_rx_fill(MHU_MBX_BASE, ch);
}


/**
 * receive data from fifo channel
 */
uint32_t mhu_recv_data(uint32_t ch, void *buf_ptr, uint32_t buf_len)
{
    uint32_t *dwptr = (uint32_t*)buf_ptr;
    uint32_t  dwlen = (buf_len / 4);
    uint32_t  fill, val, len = 0, i = 0,st;
    uint32_t  flags, irq_st, stale;
    uint8_t   sot = 0, eot = 0;

    if ((buf_len % 4) != 0) {
        ts_printf("MHUS: invalid len %u\n", buf_len);
        return 0;
    }

    while (1) {
        if (len >= dwlen) {
            ts_printf("MBX: no enough buffer, len=%u\n", buf_len);
            break;
        }

        /* wait for fifo data */
        do {
            st   = mhu_read32(MHU_MBX_BASE + MHU_MBX_FFCW_ST(ch));
            fill = st & 0x7FF;
        } while (!fill);

        /* pop data and flag from fifo */
        irq_st = arch_local_irq_save();
        val   = mhu_fifo_pop32(MHU_MBX_BASE, ch);
        flags = mhu_read32(MHU_MBX_BASE + MHU_MBX_FFCW_FLG(ch));
        stale = mhu_read32(MHU_MBX_BASE + MHU_MBX_FFCW_CTRL(ch));
        arch_local_irq_restore(irq_st);
//        ts_printf("MBX: data=0x%x flg=0x%x stale=0x%x st=0x%x\n", val, flags, stale, st);
        if (flags & 0x4) {// 0
            if ((flags & 0x1) != 0)
                sot = 1;
            if ((flags & 0x2) != 0)
                eot = 1;
        }
        if (flags & (0x4 << 4)) {// 1
            if ((flags & (0x1 << 4)) != 0)
                sot = 1;
            if ((flags & (0x2 << 4)) != 0)
                eot = 1;
        }
        if (flags & (0x4 << 8)) {// 1
            if ((flags & (0x1 << 8)) != 0)
                sot = 1;
            if ((flags & (0x2 << 8)) != 0)
                eot = 1;
        }
        if (flags & (0x4 << 12)) {// 1
            if ((flags & (0x1 << 12)) != 0)
                sot = 1;
            if ((flags & (0x2 << 12)) != 0)
                eot = 1;
        }

        /* check for start of transfer boundary */
        if (!sot) {
            ts_printf("MBX: invalid SOT, drop 0x%x\n", val);
            continue;
        }

        /* save data into user buffer */
        dwptr[len++] = val;

        /* check for end of transfer boundary */
        if (eot)
            break;
    }

    irq_st = arch_local_irq_save();
    /* check if more data is pending */
    fill = mhu_fifo_rx_fill(MHU_MBX_BASE, ch);
    if (!fill) {
        stale = mhu_read32(MHU_MBX_BASE + MHU_MBX_FFCW_INT_ST(ch));
        if (stale)
            mhu_fifo_clear_rx_irq(MHU_MBX_BASE, ch, stale);
        mhu_write32(MHU_MBX_BASE + MHU_MBX_FFCW_INT_EN(ch), 0xFFFFFFFF);
        /* clear interrupt status only if fifo is empty */
        g_mbx_ff_stat_0 &= ~(1 << ch);
    }
    arch_local_irq_restore(irq_st);

    return (len * 4);
}
