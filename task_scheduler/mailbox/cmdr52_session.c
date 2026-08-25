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
**                      include command r52 session source                      **
*********************************************************************************/


#include "cmdr52_mgr.h"
#include "cmdr52_proc.h"
#include "cmdr52_session.h"
#include "vcx_vcmd_priv.h"
#include "vcx_cmdbuf_obj.h"


int32_t cmdr52_session_init(cmdr52_session_t *session, uint32_t sessionID) {
    session->sessionID = sessionID;
    session->seqRNum   = 0x00000000;
    session->seqSNum   = 0x00000000;
    session->status    = CMD_SESSION_STATUS_IDLE;
    session->procObj   = 0x00000000;
    spin_lock_init(&session->spinlock);
    return 0;
}

int32_t        cmdr52_session_check(cmdr52_session_t *session, cmdMsg_t *cmdMsg) {
    if (session->seqRNum != cmdMsg->seqNum) {
        cmdMsg_t *cmdMsg1 = cmdr52_mgr_dequeue_cmdMsg();
        cmdEvtRepCmdError_Body_t *cmdBody1 = (cmdEvtRepCmdError_Body_t *)cmdMsg1->data;
        cmd_init(cmdMsg1);
        cmdMsg1->cmdType    = CMD_EVT_REPORT_CMDERROR;
        cmdMsg1->cmdSize    = CMD_MSG_MIN_SIZE + sizeof(cmdEvtRepCmdError_Body_t);
        cmdBody1->code      = CMD_ERR_INVALID_SEQUENCEID;
        cmdBody1->cmdType   = cmdMsg->cmdType;
        cmdBody1->seqNum    = cmdMsg->seqNum;
        cmdBody1->sessionID = cmdMsg->sessionID;
        cmdBody1->procObj   = session->procObj;
        cmdBody1->timeStamp = cmdMsg->timeStamp;
        cmdr52_session_send(session, cmdMsg1);
        return CMD_ERR_INVALID_SEQUENCEID;
    }
    session->seqRNum++;
    return 0;
}

static int32_t cmd_system_open_session(cmdr52_session_t *session, cmdMsg_t *cmdMsg) {
    uint32_t r52CoreID  = ((session->sessionID & 0xFFFF0000) >> 16);
    cmdr52_session_t *cmdr52_session = cmdr52_mgr_get_idle_session(r52CoreID);
    cmdReqOpenSession_Body_t *cmdBody = (cmdReqOpenSession_Body_t *)cmdMsg->data;
    int32_t retCode = CMD_ERR_SUCCESS;
    cmdMsg_t *cmdMsg1 = cmdr52_mgr_dequeue_cmdMsg();
    cmdRspOpenSession_Body_t *cmdBody1 = (cmdRspOpenSession_Body_t *)cmdMsg1->data;
    cmd_init(cmdMsg1);
    cmdMsg1->cmdType     = CMD_RSP_OPEN_SESSION;
    cmdMsg1->sessionID   = cmdMsg->sessionID;
    cmdMsg1->cmdSize     = CMD_MSG_MIN_SIZE + sizeof(cmdRspOpenSession_Body_t);
    cmdMsg1->timeStamp   = cmdMsg->timeStamp;
    cmdMsg1->seqNum      = cmdMsg->seqNum;

    cmdBody1->ackNum     = cmdMsg->seqNum;
    cmdBody1->procObj    = cmdBody->procObj;
    cmdBody1->timeStamp  = cmdMsg->timeStamp;
    if (cmdr52_session == NULL) {
        retCode              = CMD_ERR_INVALID_SESSIONID;
        cmdBody1->sessionID  = 0xFFFFFFFF;
    } else {
        cmdr52_session->procObj = cmdBody->procObj;
        cmdr52_session->status  = CMD_SESSION_STATUS_RUN;
        if (cmdr52_session != session) {
            cmdr52_session->seqRNum = 0x00;// sequence number, from 0 to 0xFFFFFFFF
            cmdr52_session->seqSNum = 0x00;// sequence number, from 0 to 0xFFFFFFFF
            cmdr52_session->total_workload = 0x00;
        }

        retCode              = CMD_ERR_SUCCESS;
        cmdBody1->sessionID  = cmdr52_session->sessionID;
    }

    cmdBody1->code       = retCode;

    ts_printf("******************%s:%s:%d %d r52CoreID %d %x******************\n", __FILE__, __func__, __LINE__, retCode, r52CoreID, cmdr52_session->sessionID);
    return  cmdr52_session_send(session, cmdMsg1);
}

static int32_t cmd_system_close_session(cmdr52_session_t *session, cmdMsg_t *cmdMsg) {
    cmdReqCloseSession_Body_t *cmdBody = (cmdReqCloseSession_Body_t *)cmdMsg->data;
    cmdr52_session_t *cmdr52_session = cmdr52_mgr_get_session(cmdBody->sessionID);
    int32_t retCode = CMD_ERR_SUCCESS;
    cmdMsg_t *cmdMsg1 = cmdr52_mgr_dequeue_cmdMsg();
    cmdRspCloseSession_Body_t *cmdBody1 = (cmdRspCloseSession_Body_t *)cmdMsg1->data;
    cmd_init(cmdMsg1);
    cmdMsg1->cmdType     = CMD_RSP_CLOSE_SESSION;
    cmdMsg1->sessionID   = cmdMsg->sessionID;
    cmdMsg1->cmdSize     = CMD_MSG_MIN_SIZE + sizeof(cmdRspCloseSession_Body_t);
    cmdMsg1->timeStamp   = cmdMsg->timeStamp;
    cmdMsg1->seqNum      = cmdMsg->seqNum;

    cmdBody1->ackNum     = cmdMsg->seqNum;
    if (cmdr52_session == NULL) {
        retCode              = CMD_ERR_INVALID_SESSIONID;
        cmdBody1->sessionID  = cmdBody->sessionID;
    } else {
        cmdr52_session->procObj = 0x00;
        cmdr52_session->status  = CMD_SESSION_STATUS_IDLE;
        cmdr52_session->seqRNum = 0x00;// sequence number, from 0 to 0xFFFFFFFF
        cmdr52_session->seqSNum = 0x00;// sequence number, from 0 to 0xFFFFFFFF
        cmdr52_session->total_workload = 0x00;

        retCode              = CMD_ERR_SUCCESS;
        cmdBody1->sessionID  = cmdr52_session->sessionID;
    }

    cmdBody1->code       = retCode;
    cmdBody1->procObj    = cmdBody->procObj;

    ts_printf("******************%s:%s:%d %d r52CoreID %d %x******************\n", __FILE__, __func__, __LINE__, retCode, ((session->sessionID & 0xFFFF0000) >> 16), cmdr52_session->sessionID);
    return cmdr52_session_send(session, cmdMsg1);
}

int32_t        cmdr52_session_system(cmdr52_session_t *session, cmdMsg_t *cmdMsg) {
    switch (cmdMsg->cmdType) {
    case CMD_REQ_EXE_SYSCTL:
        //return cmd_system_echo(cmdMsg);
        break;
    case CMD_REQ_OPEN_SESSION:
        return cmd_system_open_session(session, cmdMsg);
        break;
    case CMD_REQ_CLOSE_SESSION:
        return cmd_system_close_session(session, cmdMsg);
        break;
    default:
        break;
    }
    return 0;
}

static int32_t          vcodec_run_cmdbuf(cmdr52_session_t *session, cmdMsg_t *cmdMsg){
    vcmd_mgr_t *vcmd_mgr = NULL;
    cmdReqRunCmdBuf_Body_t *cmdBody = (cmdReqRunCmdBuf_Body_t *)cmdMsg->data;
    int32_t retCode = CMD_ERR_SUCCESS;
    cmdMsg_t *cmdMsg1 = cmdr52_mgr_dequeue_cmdMsg();
    cmdRspRunCmdBuf_Body_t *cmdBody1 = (cmdRspRunCmdBuf_Body_t *)cmdMsg1->data;

    vcmd_mgr = cmdr52_get_vcmd_mgr(cmdBody->vcmdmgr_id);

    cmd_init(cmdMsg1);
    cmdMsg1->cmdType     = CMD_RSP_RUN_CMDBUF;
    cmdMsg1->sessionID   = cmdMsg->sessionID;
    cmdMsg1->timeStamp   = cmdMsg->timeStamp;
    cmdMsg1->seqNum      = cmdMsg->seqNum;
    cmdMsg1->cmdSize     = CMD_MSG_MIN_SIZE + sizeof(cmdRspRunCmdBuf_Body_t);

    cmdBody1->ackNum     = cmdMsg->seqNum;
    if (vcmd_mgr == NULL) {
        retCode          = CMD_ERR_INVALID_VCMDMGRID;
    } else {
        if (cmdBody->procObj != session->procObj) {
            retCode      = CMD_ERR_INVALID_PROCOBJ;
        } else {
            retCode      = vcmd_link_and_rum_cmdbuf(vcmd_mgr, session, cmdBody);
        }
    }

    cmdBody1->code       = retCode;
    cmdBody1->vcmdmgr_id = cmdBody->vcmdmgr_id;
    cmdBody1->cmdbuf_id  = cmdBody->cmdbuf_id;
    cmdBody1->core_id    = cmdBody->core_id;
    return  cmdr52_session_send(session, cmdMsg1);
}

static int32_t          vcodec_ctrl_cmdbuf(cmdr52_session_t *session, cmdMsg_t *cmdMsg){
    vcmd_mgr_t *vcmd_mgr = NULL;
    cmdReqCtlCmdBuf_Body_t *cmdBody = (cmdReqCtlCmdBuf_Body_t *)cmdMsg->data;
    int32_t retCode = CMD_ERR_SUCCESS;
    cmdMsg_t *cmdMsg1 = cmdr52_mgr_dequeue_cmdMsg();
    cmdRspCtlCmdBuf_Body_t *cmdBody1 = (cmdRspCtlCmdBuf_Body_t *)cmdMsg1->data;

    vcmd_mgr = cmdr52_get_vcmd_mgr(cmdBody->vcmdmgr_id);

    cmd_init(cmdMsg1);
    cmdMsg1->sessionID   = cmdMsg->sessionID;
    cmdMsg1->timeStamp   = cmdMsg->timeStamp;
    cmdMsg1->seqNum      = cmdMsg->seqNum;
    cmdMsg1->cmdSize     = CMD_MSG_MIN_SIZE + sizeof(cmdRspCtlCmdBuf_Body_t);

    cmdBody1->ackNum     = cmdMsg->seqNum;
    if (vcmd_mgr == NULL) {
        retCode          = CMD_ERR_INVALID_VCMDMGRID;
    } else {
        if (cmdBody->procObj != session->procObj) {
            retCode      = CMD_ERR_INVALID_PROCOBJ;
        } else {
            switch(cmdMsg->cmdType) {
                case CMD_REQ_PUSH_SLICE_REG:
                    cmdMsg1->cmdType     = CMD_RSP_PUSH_SLICE_REG;
                    retCode      = vcmd_flush_slice_regs(vcmd_mgr, cmdBody->cmdbuf_id);
                    break;
                case CMD_REQ_POLLING_CMDBUF:
                    cmdMsg1->cmdType     = CMD_RSP_POLLING_CMDBUF;
                    retCode      = vcmd_polling_cmdbuf(vcmd_mgr, cmdBody->cmdbuf_id);
                    break;
                case CMD_REQ_ABORT_CMDBUF:
                    cmdMsg1->cmdType     = CMD_RSP_ABORT_CMDBUF;
                    retCode      = vcmd_abort_cmdbuf(vcmd_mgr, cmdBody->cmdbuf_id);
                    break;
                default :
                    break;
            }
        }
    }

    cmdBody1->code       = retCode;
    return  cmdr52_session_send(session, cmdMsg1);
}

static int32_t          vcodec_drop_owner(cmdr52_session_t *session, cmdMsg_t *cmdMsg){
    vcmd_mgr_t *vcmd_mgr = NULL;
    cmdReqDropOwner_Body_t *cmdBody = (cmdReqDropOwner_Body_t *)cmdMsg->data;
    int32_t retCode = CMD_ERR_SUCCESS;
    cmdMsg_t *cmdMsg1 = cmdr52_mgr_dequeue_cmdMsg();
    cmdRspDropOwner_Body_t *cmdBody1 = (cmdRspDropOwner_Body_t *)cmdMsg1->data;

    vcmd_mgr = cmdr52_get_vcmd_mgr(cmdBody->vcmdmgr_id);

    cmd_init(cmdMsg1);
    cmdMsg1->cmdType     = CMD_RSP_DROP_OWNER;
    cmdMsg1->sessionID   = cmdMsg->sessionID;
    cmdMsg1->timeStamp   = cmdMsg->timeStamp;
    cmdMsg1->seqNum      = cmdMsg->seqNum;
    cmdMsg1->cmdSize     = CMD_MSG_MIN_SIZE + sizeof(cmdRspDropOwner_Body_t);

    cmdBody1->ackNum     = cmdMsg->seqNum;
    if (vcmd_mgr == NULL) {
        retCode          = CMD_ERR_INVALID_VCMDMGRID;
    } else {
        if (cmdBody->procObj != session->procObj) {
            retCode      = CMD_ERR_INVALID_PROCOBJ;
        } else {
            retCode      = vcmd_drop_owner(vcmd_mgr, session, cmdBody->ownerID, cmdBody1);
        }
    }

    cmdBody1->code       = retCode;
    return  cmdr52_session_send(session, cmdMsg1);
}

int32_t        cmdr52_session_vcodec(cmdr52_session_t *session, cmdMsg_t *cmdMsg) {
    switch (cmdMsg->cmdType) {
    case CMD_REQ_RUN_CMDBUF:
        return vcodec_run_cmdbuf(session, cmdMsg);
        break;
    case CMD_REQ_PUSH_SLICE_REG:
    case CMD_REQ_POLLING_CMDBUF:
    case CMD_REQ_ABORT_CMDBUF:
        return vcodec_ctrl_cmdbuf(session, cmdMsg);
        break;
    case CMD_REQ_DROP_OWNER:
        return vcodec_drop_owner(session, cmdMsg);
        break;
    default:
        break;
    }
    return 0;
}

int32_t        cmdr52_session_send(cmdr52_session_t *session, cmdMsg_t *cmdMsg) {
    cmdMsg->sessionID    = session->sessionID;
    cmdMsg->seqNum       = session->seqSNum++;
    cmdMsg->timeStamp    = 0x00000000;
    return cmdr52_send(cmdMsg);
}



