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
**                      include command r52 manager source                      **
*********************************************************************************/

#include "crc32.h"
#include "cmdr52_mgr.h"
#include "cmdr52_proc.h"
#include "cmdr52_session.h"
#include "vcx_vcmd_priv.h"
#include "vcx_cmdbuf_obj.h"




static cmdr52_mgr_t g_cmdr52_mgr;


vcmd_mgr_t*    cmdr52_get_vcmd_mgr(uint32_t mgrID) {
    if (mgrID >= VCMD_MGR_ID_MAX) {
        return NULL;
    }
    return cmdr52_mgr_get()->mtb[mgrID];
}

int32_t         cmdr52_mgr_init(cmdr52_mgr_t *mgr, uint32_t r52coreID) {
    int32_t i = 0, j = 0;
    uint32_t sessionID = 0; // session ID from 0
    core52_mgr_t *cmgr = NULL;

    mgr->ctb_size  = CORE52_MGR_MAX; // r52 core number
    for (j = 0; j < mgr->ctb_size; ++j) {
        cmgr = &mgr->ctb[j];
        cmgr->r52coreID = r52coreID + j;
        cmgr->vtb_size  = CMD_SESSION_MAX; // r52 core number
        sessionID = ((cmgr->r52coreID << 16) & 0xFFFF0000);
        spin_lock_init(&cmgr->spinlock);
        atomic_set(&cmgr->refcount, 0);
        init_waitqueue_head(&cmgr->workwaitqueue);
        for (i = 0; i < cmgr->vtb_size; ++i) {
            cmdr52_session_init(&cmgr->vtb[i], sessionID + i);
        }
    }

    for (i = 0; i < VCMD_MGR_ID_MAX; ++i) {
        mgr->mtb[i] = NULL;
    }
    /* BQueue 数据区(1024×128B data + 2×CQueue×1024×512B)由 bqueue/cqueue
     * 内部走 ddr_alloc 放共享 DDR(link.ld .ddr_buf @0x80900000,2MB),不占
     * 本地 RAM 堆,因此可恢复完整深度 1024。 */
    mgr->cmd_queue = BQueueCreate(1024, CMD_MSG_MAX_SIZE);
    return 0;
}

int32_t          cmdr52_start_mgr(void) {
    return cmdr52_thread_create(cmdr52_mgr_get());
}

int32_t          cmdr52_exit_mgr(void){
    cmdr52_mgr_t* mgr = cmdr52_mgr_get();
    cmdr52_thread_stop(mgr);
    if (mgr->cmd_queue) {
        BQueueDelete(mgr->cmd_queue);
    }

    return 0;

}
cmdr52_mgr_t *cmdr52_mgr_get(void) {
    return &g_cmdr52_mgr;
}

cmdr52_session_t*    cmdr52_mgr_get_session(uint32_t sessionID) {
    uint32_t r52ID = 0, sesID = 0, i = 0;
    core52_mgr_t *cmgr = NULL;
    cmdr52_session_t *session = NULL;
    cmdr52_mgr_t* mgr = cmdr52_mgr_get();
    r52ID  = ((sessionID & 0xFFFF0000) >> 16);
    sesID  = (sessionID & 0xFFFF);
    if (sesID >= CMD_SESSION_MAX) {
        return NULL;
    }
    for (i = 0; i < mgr->ctb_size; ++i) {
        cmgr = &mgr->ctb[i];
        if (cmgr->r52coreID == r52ID) {
            spin_lock(&cmgr->spinlock);
            session = &cmgr->vtb[sesID];
            spin_unlock(&cmgr->spinlock);
            return session;
        }
    }
    return session;
}

cmdr52_session_t*    cmdr52_mgr_get_idle_session(uint32_t r52coreID) {
    core52_mgr_t *cmgr = NULL;
    cmdr52_session_t *session = NULL;
    cmdr52_mgr_t* mgr = cmdr52_mgr_get();
    for (uint32_t i = 0; i < mgr->ctb_size; ++i) {
        cmgr = &mgr->ctb[i];
        if (cmgr->r52coreID == r52coreID) {
            spin_lock(&cmgr->spinlock);
            for (uint32_t j = 0; j < cmgr->vtb_size; ++j) {
                if (cmgr->vtb[j].status == CMD_SESSION_STATUS_IDLE) {
                    session = &cmgr->vtb[j];
                    session->status = CMD_SESSION_STATUS_USE;
                    break;
                }
            }
            spin_unlock(&cmgr->spinlock);
            return session;
        }
    }

    return NULL;
}

cmdMsg_t*         cmdr52_mgr_dequeue_cmdMsg(void) {
    return (cmdMsg_t *)BQueueDequeue(cmdr52_mgr_get()->cmd_queue);
}

cmdMsg_t*         cmdr52_mgr_acquire_cmdMsg(void) {
    return (cmdMsg_t *)BQueueAcquire(cmdr52_mgr_get()->cmd_queue);
}

int32_t           cmdr52_mgr_release_cmdMsg(cmdMsg_t* cmdMsg) {
    return   BQueueRelease(cmdr52_mgr_get()->cmd_queue, cmdMsg);
}

int32_t           cmdr52_mgr_queue_cmdMsg(cmdMsg_t* cmdMsg) {
    return   BQueueQueue(cmdr52_mgr_get()->cmd_queue, cmdMsg);
}

int32_t           cmdr52_mgr_cancel_cmdMsg(cmdMsg_t* cmdMsg) {
    return   BQueueCancel(cmdr52_mgr_get()->cmd_queue, cmdMsg);
}

static uint32_t cmd_check(cmdMsg_t *cmdMsg, cmdr52_session_t **session)
{
    uint32_t crc32 = 0, crc32Now = 0, retCode = CMD_ERR_SUCCESS;
    if (cmdMsg == NULL) {
        retCode = CMD_ERR_INVALID_POINTER;
        goto RETURN_ERROR;
    }

    if (cmdMsg->magic != CMD_MAGIC_NUMBER) {
        retCode = CMD_ERR_INVALID_MAGIC;
        goto RETURN_ERROR;
    }

    if (cmdMsg->version != CMD_VERSION) {
        retCode = CMD_ERR_INVALID_VERSION;
        goto RETURN_ERROR;
    }

    if (cmdMsg->cmdType > CMD_VCODEC_MAX) {
        retCode = CMD_ERR_INVALID_CMD_TYPE;
        goto RETURN_ERROR;
    }

    crc32 = cmdMsg->crc32;
    cmdMsg->crc32 = 0;
    crc32Now = crc32_calc((const uint8_t *)cmdMsg, cmdMsg->cmdSize);
    cmdMsg->crc32 = crc32;
    if (crc32 != crc32Now) {
        retCode = CMD_ERR_INVALID_CHECKSUM;
        goto RETURN_ERROR;
    }

    *session = cmdr52_mgr_get_session(cmdMsg->sessionID);
    if (*session == NULL) {
        retCode = CMD_ERR_INVALID_SESSIONID;
        goto RETURN_ERROR;
    }

    return CMD_ERR_SUCCESS;
RETURN_ERROR:
    {
        cmdMsg_t *cmdSMsg = cmdr52_mgr_dequeue_cmdMsg();
        cmdEvtRepCmdError_Body_t *cmdSBody = (cmdEvtRepCmdError_Body_t *)cmdSMsg->data;
        cmdr52_session_t *cmd_session = cmdr52_mgr_get_session(0x00000000);
        if (cmd_session == NULL) {
           cmd_session = cmdr52_mgr_get_session(0x00010000);
        }
        cmd_init(cmdSMsg);
        cmdSMsg->cmdType    = CMD_EVT_REPORT_CMDERROR;
        cmdSMsg->cmdSize    = CMD_MSG_MIN_SIZE + sizeof(cmdEvtRepCmdError_Body_t);
        cmdSBody->code      = retCode;
        cmdSBody->cmdType   = cmdMsg->cmdType;
        cmdSBody->seqNum    = cmdMsg->seqNum;
        cmdSBody->sessionID = cmdMsg->sessionID;
        cmdSBody->procObj   = 0x0000000000000000;//cmdMsg->procObj;
        cmdSBody->timeStamp = cmdMsg->timeStamp;
        cmdr52_session_send(cmd_session, cmdSMsg);
    }

    return retCode;
}


int32_t cmdr52_mgr_proc_cmdMsg(cmdMsg_t *cmdMsg) {
    cmdr52_session_t *session = NULL;

    if (cmd_check(cmdMsg, &session) != CMD_ERR_SUCCESS) {
        return CMD_ERR_INVALID_PARAM;
    }

    if (cmdr52_session_check(session, cmdMsg) < 0) {
        return CMD_ERR_INVALID_SEQUENCEID;
    }

    if (cmdMsg->cmdType <= CMD_SYSTEM_MAX) {
        return cmdr52_session_system(session, cmdMsg);
    }

    return cmdr52_session_vcodec(session, cmdMsg);
}
