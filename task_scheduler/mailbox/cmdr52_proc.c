/********************************************************************************* 
**       This software is confidential and proprietary and may be used          **
**        only as expressly authorized by a licensing agreement from            **
**                                                                              **
**                            omnidimension                                     **
**                                                                              **
**                   (C) COPYRIGHT 2026 OMNIDIMENSION                           **
**                            ALL RIGHTS RESERVED                               **
**                                                                              **
**                 The entire notice above must be reproduced                   **
**                  on all copies and should not be removed.                    **
**                                                                              **
**********************************************************************************
**                        *.c cmdr52 proc source code                           **
*********************************************************************************/


#include "crc32.h"
#include "system.h" /* g_mbx_fc_stat_0 / mhu_poll_rx / mhu_send_fast_event / MHU_FC0_ACK_VALUE */
#include "cmdr52_mgr.h"
#include "cmdr52_proc.h"
#include "vcx_vcmd_priv.h"
#include "vcx_cmdbuf_obj.h"


static void doorbell_irq_callback_t(uint32_t irq, uint32_t channel) {
    cmdr52_mgr_t *mgr = (cmdr52_mgr_t*) cmdr52_mgr_get();
    cmdMsg_t *cmdMsg = NULL;

    if (irq == 0) {

    } else {

    }
    ts_printf("doorbell_irq_callback_t\n");
}

static void fastchan_irq_callback_t(uint32_t irq, uint32_t channel) {
    cmdr52_mgr_t *mgr = (cmdr52_mgr_t*) cmdr52_mgr_get();
    cmdMsg_t *cmdMsg = NULL;
    if (irq == 0) {

    } else {

    }
    ts_printf("fastchan_irq_callback_t\n");
}

/* MHU FIFO 事件去重位图:bit n 表示 channel n 已有一条 CMD_EVT_INTIRQ_MHU
 * 事件排在 cmd_queue 里、尚未被取出处理。mhu_mbx_isr(irq=1)与
 * cmdr52_proc_loop 里的轮询(irq=0)会对同一批 FIFO 数据各注入一次通知,
 * 靠该位图把重复通知合并成一条事件。 */
static volatile uint32_t g_mhu_evt_pending;

/* 事件已被取出处理:放行该 channel,允许后续新到的数据再次注入事件 */
static void fifochan_evt_consumed(uint32_t channel) {
    uint32_t flags;
    if (channel >= 32U) {
        return;
    }
    flags = arch_local_irq_save();
    g_mhu_evt_pending &= ~(1U << channel);
    arch_local_irq_restore(flags);
}

static void fifochan_irq_callback_t(uint32_t irq, uint32_t channel) {
    cmdr52_mgr_t *mgr = (cmdr52_mgr_t*) cmdr52_mgr_get();
    cmdEvtIntIrqMhu_Body_t *cmdBody = NULL;
    cmdMsg_t *cmdMsg = NULL;
    uint32_t flags;

    /* 同一 channel 已有未消费的事件就不再重复注入:ISR 与轮询会对同一批
     * FIFO 数据各发一次通知,重复的事件会让 recv_cmdMsg 在 FIFO 已空时
     * 空等 MHU_WAIT_TIMEOUT(500ms)并报 "failed to receive"。 */
    flags = arch_local_irq_save();
    if ((channel < 32U) && (g_mhu_evt_pending & (1U << channel))) {
        arch_local_irq_restore(flags);
        return;
    }
    g_mhu_evt_pending |= (1U << channel);
    arch_local_irq_restore(flags);

    if (irq == 0) {
        cmdMsg = BQueueDequeue(mgr->cmd_queue);
    } else {
        cmdMsg = BQueueDequeueFromISR(mgr->cmd_queue);
    }
    if (cmdMsg == NULL) {
        /* free 池耗尽:回滚占位,避免该 channel 的事件永久丢失 */
        ts_printf("fifochan: no free cmdMsg ch=%u\n", channel);
        fifochan_evt_consumed(channel);
        return;
    }
    cmdBody = (cmdEvtIntIrqMhu_Body_t *)cmdMsg->data;
    cmd_init(cmdMsg);
    cmdMsg->cmdType = CMD_EVT_INTIRQ_MHU;
    cmdMsg->cmdSize = CMD_MSG_MIN_SIZE + sizeof(cmdEvtIntIrqMhu_Body_t);// command size  (include cmd header and body)
    cmdMsg->sessionID = 0xFFFFFFFF;// session id
    cmdMsg->timeStamp = 0xFFFFFFFF;// time stamp, default current time in ms
    cmdMsg->seqNum    = 0x00;
    cmdBody->inttype  = 0x02;
    cmdBody->channel  = channel;
//    ts_printf("QUEUE:ptr=%08x magic=%x ver=%d type=%x size=%u sid=%x seq=%x crc=%x\n", \
        (uint32_t)(uintptr_t)cmdMsg, cmdMsg->magic, cmdMsg->version, cmdMsg->cmdType, \
        cmdMsg->cmdSize, cmdMsg->sessionID, cmdMsg->seqNum, cmdMsg->crc32);
    if (irq == 0) {
        BQueueQueue(mgr->cmd_queue, cmdMsg);    
    } else {
        BQueueQueueFromISR(mgr->cmd_queue, cmdMsg);
    }
    ts_printf("fifochan_irq_callback_t %u %u\n", irq, channel);
}

void        cmdr52_set_callback(void) {
    mhu_set_irq_callback(0, doorbell_irq_callback_t);
    mhu_set_irq_callback(1, fastchan_irq_callback_t);
    mhu_set_irq_callback(2, fifochan_irq_callback_t);
}

static int32_t  cmdr52_mgr_recv_cmdMsg(cmdMsg_t *cmdIMsg) {
    cmdr52_mgr_t *mgr = (cmdr52_mgr_t*) cmdr52_mgr_get();
    cmdEvtIntIrqMhu_Body_t *cmdBody = (cmdEvtIntIrqMhu_Body_t *) cmdIMsg->data;
    uint32_t channel = cmdBody->channel;
    cmdMsg_t *cmdMsg = NULL;
    int32_t   rvsz = 0;
    /* 事件已取出,先放行该 channel,使接收期间新到的数据能再次注入事件 */
    fifochan_evt_consumed(channel);
    cmdMsg = cmdr52_mgr_dequeue_cmdMsg();
    if (cmdMsg == NULL) {
        ts_printf("failed to alloc recv buf, channel %u\n", channel);
        return -1;
    }
    // mailbox_recv(cmdMsg);
//    ts_printf("%s:%s:%d\n", __FILE__, __func__, __LINE__);
    rvsz = mhu_recv_data(channel, cmdMsg, CMD_MSG_MAX_SIZE);
    if (rvsz <= 0) {
        ts_printf("failed %u to receive, ret %d\n", channel, rvsz);
        cmdr52_mgr_cancel_cmdMsg(cmdMsg);
        return -1;
    }
    cmdr52_mgr_queue_cmdMsg(cmdMsg);
    return 0;
}

static int32_t cmdr52_mgr_vpu_cmdMsg(cmdMsg_t *cmdIMsg) {
    cmdr52_mgr_t *mgr = (cmdr52_mgr_t*) cmdr52_mgr_get();
    cmdEvtRepCmdBufReady_Body_t *cmdBody = NULL;
    cmdEvtIntIrqVpu_Body_t *cmdIBody = NULL;
    cmdr52_session_t *session = NULL;
    struct cmdbuf_obj *obj = NULL;
    vcmd_mgr_t *vcmd_mgr = NULL;
    cmdMsg_t *cmdMsg = NULL;
    cmdIBody = (cmdEvtIntIrqVpu_Body_t *) cmdIMsg->data;
    vcmd_mgr = cmdr52_get_vcmd_mgr(cmdIBody->vcmdmgr_id);
    obj = &vcmd_mgr->objs[cmdIBody->cmdbuf_id];
    session = obj->session;
    cmdMsg = cmdr52_mgr_dequeue_cmdMsg();
    cmd_init(cmdMsg);
    cmdBody = (cmdEvtRepCmdBufReady_Body_t *)cmdMsg->data;
    cmdMsg->cmdType     = CMD_EVT_REPORT_CMDBUF_READY;
    cmdMsg->sessionID   = session->sessionID;
    cmdMsg->timeStamp   = 0;
    cmdMsg->cmdSize     = CMD_MSG_MIN_SIZE + sizeof(cmdEvtRepCmdBufReady_Body_t);
    cmdBody->cmdbuf_id  = cmdIBody->cmdbuf_id;
    cmdBody->status     = 0;// 0 - success, > 0 - fail
    cmdBody->vcmdmgr_id = vcmd_mgr->vcmd_mgr_id;
    cmdBody->procObj    = session->procObj;// process object id
    cmdr52_session_send(session, cmdMsg);
    vcmd_release_cmdbuf(vcmd_mgr, cmdIBody->cmdbuf_id);
    cmdr52_mgr_release_cmdMsg(cmdMsg);
    return 0;
}

static int32_t cmdr52_mgr_proc_internal_cmdMsg(cmdMsg_t *cmdMsg) {
    int32_t retCode = CMD_ERR_SUCCESS;
    switch (cmdMsg->cmdType) {
    case CMD_EVT_INTIRQ_MHU:
        retCode = cmdr52_mgr_recv_cmdMsg(cmdMsg);
        /* 事件消息本身用完即回收:否则每来一次 FIFO 事件就泄漏一个缓冲区,
         * 最终耗尽 1024 个 free 池,回调/recv 会拿到 NULL */
        cmdr52_mgr_release_cmdMsg(cmdMsg);
        break;
    case CMD_EVT_INTIRQ_TIMER:
        cmdr52_mgr_release_cmdMsg(cmdMsg);
        break;
    case CMD_EVT_INTIRQ_VCODEC:
        retCode = cmdr52_mgr_vpu_cmdMsg(cmdMsg);
        cmdr52_mgr_release_cmdMsg(cmdMsg);
        break;
    default:
        cmdr52_mgr_release_cmdMsg(cmdMsg);
        break;
    }
    return retCode;
}

void        cmdr52_proc_loop(void) {
    cmdr52_mgr_t *mgr = (cmdr52_mgr_t*) cmdr52_mgr_get();
    uint64_t  timeout = arch_get_time_ms() + 200;
    cmdMsg_t *cmdMsg = NULL;
    int32_t   code = 0, channel = 0;
    while(1) {
        cmdMsg = cmdr52_mgr_acquire_cmdMsg();
        if (cmdMsg == NULL) {//
            if (time_before(timeout)) {
                /*
                 * 空闲等待:无可处理消息且轮询窗口未到。关中断复查一次就绪
                 * 队列,确认为空才 WFI(避免 ISR 刚投递的事件被拖到下一个
                 * tick);任何中断(定时器 tick / MHU)都会唤醒 WFI,不会丢
                 * 事件。注意 SYSTEM_TICK_MS_0=1000,若 MHU 中断真的丢失,
                 * 兜底轮询最迟约 1s 后才跑一次。
                 */
                uint32_t flags = arch_local_irq_save();
                if (BQueueSize(mgr->cmd_queue) == 0U) {
                    __asm volatile ("wfi");
                }
                arch_local_irq_restore(flags);
                continue;
            }

            timeout = arch_get_time_ms() + 200;// no cmd 200ms scan
            for (channel = 0; channel < 3; channel += 2) {
                if (mhu_fifo_rx_fill(MHU_MBX_BASE, channel) < CMD_MSG_MIN_SIZE) {
                    continue;
                }
                fifochan_irq_callback_t(0, channel);
            }

            continue;
        }

        ts_printf("QUEUE:ptr=%08x magic=%x ver=%d type=%x size=%u sid=%x seq=%x crc=%x\n", \
            (uint32_t)(uintptr_t)cmdMsg, cmdMsg->magic, cmdMsg->version, cmdMsg->cmdType, \
            cmdMsg->cmdSize, cmdMsg->sessionID, cmdMsg->seqNum, cmdMsg->crc32);
        if ((cmdMsg->cmdType >= CMD_INTIRQ_MIN) &&
            (cmdMsg->cmdType <= CMD_INTIRQ_MAX)) {
            code = cmdr52_mgr_proc_internal_cmdMsg(cmdMsg);
        } else {
            code = cmdr52_mgr_proc_cmdMsg(cmdMsg);
        }

        if (code != CMD_ERR_SUCCESS) {
            ts_printf("cmdr52_mgr_proc_cmdMsg:%d\n", code);
        }
        //ts_printf("%s:%s:%d %d\n", __FILE__, __func__, __LINE__, code);
        //cmdr52_mgr_release_cmdMsg(cmdMsg);
    }
}