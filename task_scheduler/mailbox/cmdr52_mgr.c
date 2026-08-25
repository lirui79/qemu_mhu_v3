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
        cmdMsg_t *cmdMsg1 = cmdr52_mgr_dequeue_cmdMsg();
        cmdEvtRepCmdError_Body_t *cmdBody1 = (cmdEvtRepCmdError_Body_t *)cmdMsg1->data;
        cmdr52_session_t *session1 = cmdr52_mgr_get_session(0x00000000);
        if (session1 == NULL) {
           session1 = cmdr52_mgr_get_session(0x00010000);
        }
        cmd_init(cmdMsg1);
        cmdMsg1->cmdType    = CMD_EVT_REPORT_CMDERROR;
        cmdMsg1->cmdSize    = CMD_MSG_MIN_SIZE + sizeof(cmdEvtRepCmdError_Body_t);
        cmdBody1->code      = retCode;
        cmdBody1->cmdType   = cmdMsg->cmdType;
        cmdBody1->seqNum    = cmdMsg->seqNum;
        cmdBody1->sessionID = cmdMsg->sessionID;
        cmdBody1->procObj   = 0x0000000000000000;//cmdMsg->procObj;
        cmdBody1->timeStamp = cmdMsg->timeStamp;
        cmdr52_session_send(session1, cmdMsg1);
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

int32_t           vcmd_wait_cmdbuf(vcmd_mgr_t *vcmd_mgr) {
    cmdMsg_t *cmdMsg = NULL;
	struct cmdbuf_obj *obj = NULL;
    cmdr52_session_t *session = NULL;
    cmdEvtRepCmdBufReady_Body_t *cmdBody = NULL;
    int32_t retCode = CMD_ERR_SUCCESS;
    uint16_t cmdbuf_id = ANY_CMDBUF_ID;
    if (vcmd_wait_cmdbuf_ready(vcmd_mgr, cmdbuf_id, &cmdbuf_id) < 0) {
        retCode = CMD_ERR_INVALID_PARAM;
        return retCode;
    };
    ts_printf("%s:%s:%d cmdbuf_id:%d\n", __FILE__, __func__, __LINE__, cmdbuf_id);
    obj = &vcmd_mgr->objs[cmdbuf_id];
    session = obj->session;
    cmdMsg = cmdr52_mgr_dequeue_cmdMsg();
    cmd_init(cmdMsg);
    cmdBody = (cmdEvtRepCmdBufReady_Body_t *)cmdMsg->data;
    cmdMsg->cmdType     = CMD_EVT_REPORT_CMDBUF_READY;
    cmdMsg->sessionID   = session->sessionID;
    cmdMsg->timeStamp   = 0;
    cmdMsg->cmdSize     = CMD_MSG_MIN_SIZE + sizeof(cmdEvtRepCmdBufReady_Body_t);
    cmdBody->cmdbuf_id  = cmdbuf_id;
    cmdBody->status     = 0;// 0 - success, > 0 - fail
    cmdBody->vcmdmgr_id = vcmd_mgr->vcmd_mgr_id;
    cmdBody->procObj    = session->procObj;// process object id
    cmdr52_session_send(session, cmdMsg);
    vcmd_release_cmdbuf(vcmd_mgr, cmdbuf_id);
    //cmdr52_mgr_release_cmdMsg(cmdMsg);
    return 0;
}

#if 0
int32_t vcmd_link_and_rum_cmdbuf(vcmd_mgr_t *vcmd_mgr, cmdr52_session_t *session, cmdReqRunCmdBuf_Body_t *cmd_body){
	struct cmdbuf_obj *obj;
	uint16_t cmdbuf_id = cmd_body->cmdbuf_id;
	uint16_t batchcount = ((cmd_body->interrupt_ctrl >> 32) & 0xff);

	if (cmdbuf_id >= SLOT_NUM_CMDBUF) {		//should not happen
		ts_printf("%s: ERROR cmdbuf_id %d!!\n", __func__, cmdbuf_id);
		return -1;
	}

	obj = &vcmd_mgr->objs[cmdbuf_id];
	obj->owner          = cmd_body->ownerID;
	obj->session        = session;
	obj->cmdbuf_size    = cmd_body->cmdbuf_size;
	obj->interrupt_ctrl = cmd_body->interrupt_ctrl;
	obj->module_type    = cmd_body->module_type;
	obj->core_mask      = cmd_body->core_mask;
	obj->core_id        = 0;
	cmd_body->core_id = obj->core_id;
//    ts_printf("Assign cmdbuf[%d] to core[%d]\n", cmdbuf_id, cmd_body->core_id);
    vcmd_add_done_job(vcmd_mgr, obj);
	return 0;
}


int32_t vcmd_drop_owner(vcmd_mgr_t *vcmd_mgr, cmdr52_session_t *session, uint64_t ownerID, cmdRspDropOwner_Body_t *cmd_body){
    struct cmdbuf_obj *obj = 0;
    uint16_t cmdbuf_id = 0, handled = 0;
	long dropped_cmdbuf_num = 0;
    for (cmdbuf_id = 0; cmdbuf_id < SLOT_NUM_CMDBUF; cmdbuf_id++) {
        obj = &vcmd_mgr->objs[cmdbuf_id];
        if ((ownerID != 0x00) &&(obj->owner == ownerID) && (obj->session == session)) {
            obj->owner = 0;
            obj->session = NULL;
            ts_printf("Drop cmdbuf[%d] from core[%d]\n", cmdbuf_id, obj->core_id);
            dropped_cmdbuf_num++;
            handled++;
        }
    }

    if ((ownerID != 0x00) && (handled > 0)) {
        wake_up_interruptible(&vcmd_mgr->job_waitq);
    }
	cmd_body->cmdbuf_num = dropped_cmdbuf_num;
    return 0;
}
#endif



