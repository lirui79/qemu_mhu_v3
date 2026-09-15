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
**                      include command a78 session source                      **
*********************************************************************************/

#include "crc32.h"
#include "cmdnode.h"
#include "cmda78_mgr.h"
#include "cmda78_proc.h"
#include "cmda78_session.h"
#include "vcx_vcmd_priv.h"
#include "vcx_vcmd.h"


int32_t cmda78_session_init(cmda78_session_t *session, struct proc_obj *proc, uint32_t sessionID) {
    session->proc      = proc;
    if (proc != NULL) {
        proc->session      = session;
    }
    session->sessionID = sessionID;
    session->seqRNum   = 0x00000000;
    session->seqSNum   = 0x00000000;
    session->status    = CMD_SESSION_STATUS_IDLE;
    session->cmdroot   = RB_ROOT;
    spin_lock_init(&session->spinlock);
    return 0;
}

int32_t        cmda78_session_check(cmda78_session_t *session, cmdMsg_t *cmdMsg) {
    
    if (session->seqRNum != cmdMsg->seqNum) {
        return CMD_ERR_INVALID_SEQUENCEID;
    }
    session->seqRNum++;
    return 0;
}

static cmdnode_t *cmdsession_search_cmdnode(cmda78_session_t *session, uint32_t ackNum) {
    cmdnode_t *cnode = NULL;
    spin_lock(&session->spinlock);
    cnode = cmdnode_search(&session->cmdroot, ackNum);
    spin_unlock(&session->spinlock);
    if (cnode != NULL) {
        kref_get(&cnode->refcount);
    }
    return cnode;
}

static int32_t cmdsession_wake_up_all(cmdnode_t *cnode, cmdMsg_t *cmdMsg) {
    int32_t  retCode = CMD_ERR_SUCCESS;
    cnode->cmdMsg = cmdMsg;
    retCode = cnode->code;
    wake_up_interruptible_all(&cnode->wait);
    cmdnode_free(cnode);
    return retCode;
}


static int32_t cmd_system_open_session(cmda78_session_t *session, cmdMsg_t *cmdMsg) {
    cmdRspOpenSession_Body_t *cmdBody = (cmdRspOpenSession_Body_t *)cmdMsg->data;
    cmda78_session_t *cmd_session = NULL;
    struct proc_obj *proc = NULL;
    cmdnode_t *cnode = NULL;

    cmd_session = cmda78_get_session(cmdBody->sessionID);
    cnode = cmdsession_search_cmdnode(session, cmdBody->ackNum);
    if (cnode == NULL) {
        cmda78_release_cmdMsg(cmdMsg);
        return CMD_ERR_INVALID_ACKNUM;
    }

    cnode->code = cmdBody->code;
    if (cnode->procObj == cmdBody->procObj) {
        proc = cnode->proc;
        proc->session = cmd_session;

        if (cmd_session != NULL) {
            spin_lock(&cmd_session->spinlock);
            cmd_session->proc = proc;
            cmd_session->status  = CMD_SESSION_STATUS_RUN;
            if (cmd_session != session) {
                cmd_session->seqRNum = 0x00;// sequence number, from 0 to 0xFFFFFFFF
                ts_printf("++++++++++++%s:%s:%d %d ptr=%08x %x  proc=%08x++++++++++++\n", __FILE__, __func__, __LINE__, cnode->code, (uint32_t)(uintptr_t)session, cmdBody->sessionID, (uint32_t)(uintptr_t)cmd_session);
                cmd_session->seqSNum = 0x00;// sequence number, from 0 to 0xFFFFFFFF
            }
            spin_unlock(&cmd_session->spinlock);
        } else {
            cnode->code = CMD_ERR_INVALID_SESSIONID;
        }
    } else {
        cnode->code = CMD_ERR_INVALID_PROCOBJ;
    }


    ts_printf("++++++++++++%s:%s:%d %d %x++++++++++++\n", __FILE__, __func__, __LINE__, cnode->code, cmdBody->sessionID);
    return cmdsession_wake_up_all(cnode, cmdMsg);
}

static int32_t cmd_system_close_session(cmda78_session_t *session, cmdMsg_t *cmdMsg) {
    cmdRspCloseSession_Body_t *cmdBody = (cmdRspCloseSession_Body_t *)cmdMsg->data;
    cmda78_session_t *cmd_session = NULL;
    struct proc_obj *proc = NULL;
    cmdnode_t *cnode = NULL;

    cmd_session = cmda78_get_session(cmdBody->sessionID);
    cnode = cmdsession_search_cmdnode(session, cmdBody->ackNum);
    if (cnode == NULL) {
        cmda78_release_cmdMsg(cmdMsg);
        return CMD_ERR_INVALID_ACKNUM;
    }

    cnode->code = cmdBody->code;
    if (cnode->procObj == cmdBody->procObj) {
        proc = cnode->proc;
        proc->session = NULL;

        if (cmd_session != NULL) {
            spin_lock(&cmd_session->spinlock);
            cmd_session->status  = CMD_SESSION_STATUS_IDLE;
            cmd_session->seqRNum = 0x00;// sequence number, from 0 to 0xFFFFFFFF
            cmd_session->seqSNum = 0x00;// sequence number, from 0 to 0xFFFFFFFF
            cmd_session->proc = NULL;
            spin_unlock(&cmd_session->spinlock);
        } else {
            cnode->code = CMD_ERR_INVALID_SESSIONID;
        }
    } else {
        cnode->code = CMD_ERR_INVALID_PROCOBJ;
    }

    ts_printf("++++++++++++%s:%s:%d %d %x++++++++++++\n", __FILE__, __func__, __LINE__, cnode->code, cmdBody->sessionID);
    return cmdsession_wake_up_all(cnode, cmdMsg);
}

static int32_t cmd_system_report(cmda78_session_t *session, cmdMsg_t *cmdMsg) {
    cmdEvtRepCmdError_Body_t *cmdBody = (cmdEvtRepCmdError_Body_t *)cmdMsg->data;
    //ts_printf("QUEUE:ptr=%08x magic=%x ver=%d type=%x size=%u sid=%x seq=%x crc=%x\n", \
            (uint32_t)(uintptr_t)cmdMsg, cmdMsg->magic, cmdMsg->version, cmdMsg->cmdType, \
            cmdMsg->cmdSize, cmdMsg->sessionID, cmdMsg->seqNum, cmdMsg->crc32);

    //ts_printf("code:%x cmdType:%x seqNum:%x sessionID:%x procObj:%llx timeStamp:%llx\n", \
            cmdBody->code, cmdBody->cmdType, cmdBody->seqNum, cmdBody->sessionID, \
            (unsigned long long)cmdBody->procObj, (unsigned long long)cmdBody->timeStamp);
    cmda78_release_cmdMsg(cmdMsg);
    return 0;
}

int32_t        cmda78_session_system(cmda78_session_t *session, cmdMsg_t *cmdMsg) {
    switch (cmdMsg->cmdType) {
    case CMD_RSP_EXE_SYSCTL:  //CMD_REQ_EXE_SYSCTL:
        //return cmd_system_echo(cmdMsg);
        break;
    case CMD_RSP_OPEN_SESSION:  //CMD_REQ_OPEN_SESSION:
        return cmd_system_open_session(session, cmdMsg);
        break;
    case CMD_RSP_CLOSE_SESSION:  //CMD_REQ_CLOSE_SESSION:
        return cmd_system_close_session(session, cmdMsg);
        break;
    case CMD_EVT_REPORT_CMDERROR: //
        return cmd_system_report(session, cmdMsg);
         break;
    default:
        break;
    }
    cmda78_release_cmdMsg(cmdMsg);
    return 0;
}

static int32_t          vcodec_run_cmdbuf(cmda78_session_t *session, cmdMsg_t *cmdMsg) {
    cmdnode_t *cnode = NULL;
    cmdRspRunCmdBuf_Body_t *cmdBody = (cmdRspRunCmdBuf_Body_t *)cmdMsg->data;

    cnode = cmdsession_search_cmdnode(session, cmdBody->ackNum);
    if (cnode == NULL) {
        cmda78_release_cmdMsg(cmdMsg);
        return CMD_ERR_INVALID_ACKNUM;
    }

    cnode->code = cmdBody->code;
    if (cnode->sessionID != cmdMsg->sessionID) {
        cnode->code = CMD_ERR_INVALID_SESSIONID;
    }

    return cmdsession_wake_up_all(cnode, cmdMsg);
}

static int32_t          vcodec_ctrl_cmdbuf(cmda78_session_t *session, cmdMsg_t *cmdMsg) {
    cmdnode_t *cnode = NULL;
    cmdRspCtlCmdBuf_Body_t *cmdBody = (cmdRspCtlCmdBuf_Body_t *)cmdMsg->data;

    cnode = cmdsession_search_cmdnode(session, cmdBody->ackNum);
    if (cnode == NULL) {
        cmda78_release_cmdMsg(cmdMsg);
        return CMD_ERR_INVALID_ACKNUM;
    }

    cnode->code = cmdBody->code;
    if (cnode->sessionID != cmdMsg->sessionID) {
        cnode->code = CMD_ERR_INVALID_SESSIONID;
    }

    return cmdsession_wake_up_all(cnode, cmdMsg);
}

static int32_t          vcodec_drop_owner(cmda78_session_t *session, cmdMsg_t *cmdMsg) {
    cmdnode_t *cnode = NULL;
    cmdRspDropOwner_Body_t *cmdBody = (cmdRspDropOwner_Body_t *)cmdMsg->data;

    cnode = cmdsession_search_cmdnode(session, cmdBody->ackNum);
    if (cnode == NULL) {
        cmda78_release_cmdMsg(cmdMsg);
        return CMD_ERR_INVALID_ACKNUM;
    }

    cnode->code = cmdBody->code;
    if (cnode->sessionID != cmdMsg->sessionID) {
        cnode->code = CMD_ERR_INVALID_SESSIONID;
    }

    return cmdsession_wake_up_all(cnode, cmdMsg);
}

static int32_t          vcodec_report_cmdbuf_ready(cmda78_session_t *session, cmdMsg_t *cmdMsg) {
    cmdEvtRepCmdBufReady_Body_t *cmdBody = (cmdEvtRepCmdBufReady_Body_t *)cmdMsg->data;
    vcmd_mgr_t* vcmd_mgr = NULL;
    struct cmdbuf_obj *obj = NULL;
    int32_t  retCode = CMD_ERR_SUCCESS;

    if (cmdBody->status != 0x00) {
        cmda78_release_cmdMsg(cmdMsg);
        return CMD_ERR_INVALID_PARAM;
    }

    if ((cmdBody->cmdbuf_id == ANY_CMDBUF_ID) || (cmdBody->cmdbuf_id >= SLOT_NUM_CMDBUF)) {
        cmda78_release_cmdMsg(cmdMsg);
        return CMD_ERR_INVALID_CMDBUFID;
    }


    vcmd_mgr = cmda78_get_vcmd_mgr(cmdBody->vcmdmgr_id);
    obj = &vcmd_mgr->objs[cmdBody->cmdbuf_id];
    if (obj->po != session->proc) {
        cmda78_release_cmdMsg(cmdMsg);
        return CMD_ERR_INVALID_PROCOBJ;
    }

    if (cmdBody->vcmdmgr_id >= VCMD_MGR_ID_MAX) {
        cmda78_release_cmdMsg(cmdMsg);
        return CMD_ERR_INVALID_VCMDMGRID;
    }

    obj->cmdbuf_run_done = 1;

    switch(cmdBody->vcmdmgr_id) {
        case VCMD_MGR_ID_ENC:
            vce_proc_add_done_job(vcmd_mgr, obj);
            break;
        case VCMD_MGR_ID_DEC:
            vcd_proc_add_done_job(vcmd_mgr, obj);
            break;
        default:
            retCode = CMD_ERR_INVALID_VCMDMGRID;
            break;
    }
    cmda78_release_cmdMsg(cmdMsg);
    return retCode;
}

int32_t        cmda78_session_vcodec(cmda78_session_t *session, cmdMsg_t *cmdMsg) {
    switch (cmdMsg->cmdType) {
    case CMD_RSP_RUN_CMDBUF:  ///CMD_REQ_RUN_CMDBUF:
        return vcodec_run_cmdbuf(session, cmdMsg);
        break;
    case CMD_RSP_PUSH_SLICE_REG:  ///CMD_REQ_PUSH_SLICE_REGION:
    case CMD_RSP_POLLING_CMDBUF:  ///CMD_REQ_POLLING_CMDBUF:
    case CMD_RSP_ABORT_CMDBUF:    ///CMD_REQ_ABORT_CMDBUF
        return vcodec_ctrl_cmdbuf(session, cmdMsg);
        break;
    case CMD_RSP_DROP_OWNER:  ///CMD_REQ_DROP_OWNER:
        return vcodec_drop_owner(session, cmdMsg);
        break;
    case CMD_EVT_REPORT_CMDBUF_READY:  ///CMD_EVT_REPORT_CMDBUF_READY:
        return vcodec_report_cmdbuf_ready(session, cmdMsg);
        break;
    default:
        break;
    }

    cmda78_release_cmdMsg(cmdMsg);
    return 0;
}

int32_t        cmda78_session_send(cmda78_session_t *session, cmdMsg_t *cmdMsg) {
    uint32_t ch = 2 * ((session->sessionID & 0xFFFF0000) >> 16), snsz = 0;// 0 r52   0- channel a78 -> r52   1- channel r52 -> a78 ; 1 r52   2- channel a78 -> r52   3- channel r52 -> a78 
    cmdMsg->sessionID    = session->sessionID;
    cmdMsg->seqNum       = session->seqSNum++;
    cmdMsg->timeStamp    = 0x00000000;
    cmdMsg->crc32 = crc32_calc((const uint8_t *)cmdMsg, cmdMsg->cmdSize);
// mailbox_send(cmdMsg);
    snsz = mhu_send_data(ch, (void *)cmdMsg, cmdMsg->cmdSize);
    if (snsz != cmdMsg->cmdSize) {
        ts_printf("Failed to send cmd, ret %u\n", snsz);
        return -1;
    }
    return 0;
}
