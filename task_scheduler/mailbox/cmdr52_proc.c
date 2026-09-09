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

static void fifochan_irq_callback_t(uint32_t irq, uint32_t channel) {
    cmdr52_mgr_t *mgr = (cmdr52_mgr_t*) cmdr52_mgr_get();
    cmdEvtIntIrqMhu_Body_t *cmdBody = NULL;
    cmdMsg_t *cmdMsg = NULL;

    if (irq == 0) {
        cmdMsg = BQueueDequeue(mgr->cmd_queue);
    } else {
        cmdMsg = BQueueDequeueFromISR(mgr->cmd_queue);
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
    cmdMsg = cmdr52_mgr_dequeue_cmdMsg();
    // mailbox_recv(cmdMsg);
    ts_printf("%s:%s:%d\n", __FILE__, __func__, __LINE__);
    rvsz = mhu_recv_data(channel, cmdMsg, CMD_MSG_MAX_SIZE);
    if (rvsz <= 0) {
        ts_printf("failed to receive, ret %u\n", rvsz);
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
    return 0;
}

static int32_t cmdr52_mgr_proc_internal_cmdMsg(cmdMsg_t *cmdMsg) {
    switch (cmdMsg->cmdType) {
    case CMD_EVT_INTIRQ_MHU:
        return cmdr52_mgr_recv_cmdMsg(cmdMsg);
        break;
    case CMD_EVT_INTIRQ_TIMER:
        break;
    case CMD_EVT_INTIRQ_VCODEC:
        return cmdr52_mgr_vpu_cmdMsg(cmdMsg);
        break;
    default:
        break;
    }
    return 0;
}

void        cmdr52_proc_loop(void) {
    cmdr52_mgr_t *mgr = (cmdr52_mgr_t*) cmdr52_mgr_get();
    uint64_t  timeout = arch_get_time_ms() + 200;
    cmdMsg_t *cmdMsg = NULL;
    int32_t   code = 0, channel = 0;
    while(1) {
        cmdMsg = cmdr52_mgr_acquire_cmdMsg();
        if (cmdMsg == NULL) {//
            for (channel = 0; channel < 3; channel += 2) {
                if (mhu_fifo_rx_fill(MHU_MBX_BASE, channel) < CMD_MSG_MIN_SIZE) {
                    continue;
                }
                fifochan_irq_callback_t(0, channel);
            }

            if (time_before(timeout)) {
                continue;
            }
            timeout = arch_get_time_ms() + 200;
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
        cmdr52_mgr_release_cmdMsg(cmdMsg);
    }
}