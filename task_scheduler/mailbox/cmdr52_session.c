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

#include "crc32.h"
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
        cmdMsg_t *cmdSMsg = cmdr52_mgr_dequeue_cmdMsg();
        cmdEvtRepCmdError_Body_t *cmdSBody = (cmdEvtRepCmdError_Body_t *)cmdSMsg->data;
        cmd_init(cmdSMsg);
        cmdSMsg->cmdType    = CMD_EVT_REPORT_CMDERROR;
        cmdSMsg->cmdSize    = CMD_MSG_MIN_SIZE + sizeof(cmdEvtRepCmdError_Body_t);
        cmdSBody->code      = CMD_ERR_INVALID_SEQUENCEID;
        cmdSBody->cmdType   = cmdMsg->cmdType;
        cmdSBody->seqNum    = cmdMsg->seqNum;
        cmdSBody->sessionID = cmdMsg->sessionID;
        cmdSBody->procObj   = session->procObj;
        cmdSBody->timeStamp = cmdMsg->timeStamp;
        cmdr52_session_send(session, cmdSMsg);
        cmdr52_mgr_release_cmdMsg(cmdSMsg);
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
    cmdMsg_t *cmdSMsg = cmdr52_mgr_dequeue_cmdMsg();
    cmdRspOpenSession_Body_t *cmdSBody = (cmdRspOpenSession_Body_t *)cmdSMsg->data;
    cmd_init(cmdSMsg);
    cmdSMsg->cmdType     = CMD_RSP_OPEN_SESSION;
    cmdSMsg->sessionID   = cmdMsg->sessionID;
    cmdSMsg->cmdSize     = CMD_MSG_MIN_SIZE + sizeof(cmdRspOpenSession_Body_t);
    cmdSMsg->timeStamp   = cmdMsg->timeStamp;
    cmdSMsg->seqNum      = cmdMsg->seqNum;

    cmdSBody->ackNum     = cmdMsg->seqNum;
    cmdSBody->procObj    = cmdBody->procObj;
    cmdSBody->timeStamp  = cmdMsg->timeStamp;
    if (cmdr52_session == NULL) {
        retCode              = CMD_ERR_INVALID_SESSIONID;
        cmdSBody->sessionID  = 0xFFFFFFFF;
    } else {
        cmdr52_session->procObj = cmdBody->procObj;
        cmdr52_session->status  = CMD_SESSION_STATUS_RUN;
        if (cmdr52_session != session) {
            cmdr52_session->seqRNum = 0x00;// sequence number, from 0 to 0xFFFFFFFF
            cmdr52_session->seqSNum = 0x00;// sequence number, from 0 to 0xFFFFFFFF
            cmdr52_session->total_workload = 0x00;
        }

        retCode              = CMD_ERR_SUCCESS;
        cmdSBody->sessionID  = cmdr52_session->sessionID;
    }

    cmdSBody->code       = retCode;

    ts_printf("******************%s:%s:%d %d r52CoreID %d %x******************\n", __FILE__, __func__, __LINE__, retCode, r52CoreID, cmdr52_session->sessionID);
    retCode = cmdr52_session_send(session, cmdSMsg);
    cmdr52_mgr_release_cmdMsg(cmdSMsg);
    return retCode;
}

static int32_t cmd_system_close_session(cmdr52_session_t *session, cmdMsg_t *cmdMsg) {
    cmdReqCloseSession_Body_t *cmdBody = (cmdReqCloseSession_Body_t *)cmdMsg->data;
    cmdr52_session_t *cmdr52_session = cmdr52_mgr_get_session(cmdBody->sessionID);
    int32_t retCode = CMD_ERR_SUCCESS;
    cmdMsg_t *cmdSMsg = cmdr52_mgr_dequeue_cmdMsg();
    cmdRspCloseSession_Body_t *cmdSBody = (cmdRspCloseSession_Body_t *)cmdSMsg->data;
    cmd_init(cmdSMsg);
    cmdSMsg->cmdType     = CMD_RSP_CLOSE_SESSION;
    cmdSMsg->sessionID   = cmdMsg->sessionID;
    cmdSMsg->cmdSize     = CMD_MSG_MIN_SIZE + sizeof(cmdRspCloseSession_Body_t);
    cmdSMsg->timeStamp   = cmdMsg->timeStamp;
    cmdSMsg->seqNum      = cmdMsg->seqNum;

    cmdSBody->ackNum     = cmdMsg->seqNum;
    if (cmdr52_session == NULL) {
        retCode              = CMD_ERR_INVALID_SESSIONID;
        cmdSBody->sessionID  = cmdBody->sessionID;
    } else {
        cmdr52_session->procObj = 0x00;
        cmdr52_session->status  = CMD_SESSION_STATUS_IDLE;
        cmdr52_session->seqRNum = 0x00;// sequence number, from 0 to 0xFFFFFFFF
        cmdr52_session->seqSNum = 0x00;// sequence number, from 0 to 0xFFFFFFFF
        cmdr52_session->total_workload = 0x00;

        retCode              = CMD_ERR_SUCCESS;
        cmdSBody->sessionID  = cmdr52_session->sessionID;
    }

    cmdSBody->code       = retCode;
    cmdSBody->procObj    = cmdBody->procObj;

    ts_printf("******************%s:%s:%d %d r52CoreID %d %x******************\n", __FILE__, __func__, __LINE__, retCode, ((session->sessionID & 0xFFFF0000) >> 16), cmdr52_session->sessionID);
    retCode = cmdr52_session_send(session, cmdSMsg);
    cmdr52_mgr_release_cmdMsg(cmdSMsg);
    return retCode;
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
    cmdMsg_t *cmdSMsg = cmdr52_mgr_dequeue_cmdMsg();
    cmdRspRunCmdBuf_Body_t *cmdSBody = (cmdRspRunCmdBuf_Body_t *)cmdSMsg->data;

    vcmd_mgr = cmdr52_get_vcmd_mgr(cmdBody->vcmdmgr_id);

    cmd_init(cmdSMsg);
    cmdSMsg->cmdType     = CMD_RSP_RUN_CMDBUF;
    cmdSMsg->sessionID   = cmdMsg->sessionID;
    cmdSMsg->timeStamp   = cmdMsg->timeStamp;
    cmdSMsg->seqNum      = cmdMsg->seqNum;
    cmdSMsg->cmdSize     = CMD_MSG_MIN_SIZE + sizeof(cmdRspRunCmdBuf_Body_t);

    cmdSBody->ackNum     = cmdMsg->seqNum;
    if (vcmd_mgr == NULL) {
        retCode          = CMD_ERR_INVALID_VCMDMGRID;
    } else {
        if (cmdBody->procObj != session->procObj) {
            retCode      = CMD_ERR_INVALID_PROCOBJ;
        } else {
            retCode      = vcmd_link_and_rum_cmdbuf(vcmd_mgr, session, cmdBody);
        }
    }

    cmdSBody->code       = retCode;
    cmdSBody->vcmdmgr_id = cmdBody->vcmdmgr_id;
    cmdSBody->cmdbuf_id  = cmdBody->cmdbuf_id;
    cmdSBody->core_id    = cmdBody->core_id;
    retCode = cmdr52_session_send(session, cmdSMsg);
    cmdr52_mgr_release_cmdMsg(cmdSMsg);
    return retCode;
}

static int32_t          vcodec_ctrl_cmdbuf(cmdr52_session_t *session, cmdMsg_t *cmdMsg){
    vcmd_mgr_t *vcmd_mgr = NULL;
    cmdReqCtlCmdBuf_Body_t *cmdBody = (cmdReqCtlCmdBuf_Body_t *)cmdMsg->data;
    int32_t retCode = CMD_ERR_SUCCESS;
    cmdMsg_t *cmdSMsg = cmdr52_mgr_dequeue_cmdMsg();
    cmdRspCtlCmdBuf_Body_t *cmdSBody = (cmdRspCtlCmdBuf_Body_t *)cmdSMsg->data;

    vcmd_mgr = cmdr52_get_vcmd_mgr(cmdBody->vcmdmgr_id);

    cmd_init(cmdSMsg);
    cmdSMsg->sessionID   = cmdMsg->sessionID;
    cmdSMsg->timeStamp   = cmdMsg->timeStamp;
    cmdSMsg->seqNum      = cmdMsg->seqNum;
    cmdSMsg->cmdSize     = CMD_MSG_MIN_SIZE + sizeof(cmdRspCtlCmdBuf_Body_t);

    cmdSBody->ackNum     = cmdMsg->seqNum;
    if (vcmd_mgr == NULL) {
        retCode          = CMD_ERR_INVALID_VCMDMGRID;
    } else {
        if (cmdBody->procObj != session->procObj) {
            retCode      = CMD_ERR_INVALID_PROCOBJ;
        } else {
            switch(cmdMsg->cmdType) {
                case CMD_REQ_PUSH_SLICE_REG:
                    cmdSMsg->cmdType     = CMD_RSP_PUSH_SLICE_REG;
                    retCode      = vcmd_flush_slice_regs(vcmd_mgr, cmdBody->cmdbuf_id);
                    break;
                case CMD_REQ_POLLING_CMDBUF:
                    cmdSMsg->cmdType     = CMD_RSP_POLLING_CMDBUF;
                    retCode      = vcmd_polling_cmdbuf(vcmd_mgr, cmdBody->cmdbuf_id);
                    break;
                case CMD_REQ_ABORT_CMDBUF:
                    cmdSMsg->cmdType     = CMD_RSP_ABORT_CMDBUF;
                    retCode      = vcmd_abort_cmdbuf(vcmd_mgr, cmdBody->cmdbuf_id);
                    break;
                default :
                    break;
            }
        }
    }

    cmdSBody->code       = retCode;
    retCode = cmdr52_session_send(session, cmdSMsg);
    cmdr52_mgr_release_cmdMsg(cmdSMsg);
    return retCode;
}

static int32_t          vcodec_drop_owner(cmdr52_session_t *session, cmdMsg_t *cmdMsg){
    vcmd_mgr_t *vcmd_mgr = NULL;
    cmdReqDropOwner_Body_t *cmdBody = (cmdReqDropOwner_Body_t *)cmdMsg->data;
    int32_t retCode = CMD_ERR_SUCCESS;
    cmdMsg_t *cmdSMsg = cmdr52_mgr_dequeue_cmdMsg();
    cmdRspDropOwner_Body_t *cmdSBody = (cmdRspDropOwner_Body_t *)cmdSMsg->data;

    vcmd_mgr = cmdr52_get_vcmd_mgr(cmdBody->vcmdmgr_id);

    cmd_init(cmdSMsg);
    cmdSMsg->cmdType     = CMD_RSP_DROP_OWNER;
    cmdSMsg->sessionID   = cmdMsg->sessionID;
    cmdSMsg->timeStamp   = cmdMsg->timeStamp;
    cmdSMsg->seqNum      = cmdMsg->seqNum;
    cmdSMsg->cmdSize     = CMD_MSG_MIN_SIZE + sizeof(cmdRspDropOwner_Body_t);

    cmdSBody->ackNum     = cmdMsg->seqNum;
    if (vcmd_mgr == NULL) {
        retCode          = CMD_ERR_INVALID_VCMDMGRID;
    } else {
        if (cmdBody->procObj != session->procObj) {
            retCode      = CMD_ERR_INVALID_PROCOBJ;
        } else {
            retCode      = vcmd_drop_owner(vcmd_mgr, session, cmdBody->ownerID, cmdSBody);
        }
    }

    cmdSBody->code       = retCode;
    retCode = cmdr52_session_send(session, cmdSMsg);
    cmdr52_mgr_release_cmdMsg(cmdSMsg);
    return retCode;
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
    uint32_t ch = 2 * ((session->sessionID & 0xFFFF0000) >> 16) + 1, snsz = 0;// 0 r52   0- channel a78 -> r52   1- channel r52 -> a78 ; 1 r52   2- channel a78 -> r52   3- channel r52 -> a78 
    cmdMsg->sessionID    = session->sessionID;
    cmdMsg->timeStamp    = 0x00000000;
    spin_lock(&session->spinlock);
    cmdMsg->seqNum       = session->seqSNum++;
    cmdMsg->crc32        = crc32_calc((const uint8_t *)cmdMsg, cmdMsg->cmdSize);
    snsz                 = mhu_send_data(ch, (void*)cmdMsg, cmdMsg->cmdSize);
    spin_unlock(&session->spinlock);
    if (snsz != cmdMsg->cmdSize) {
        ts_printf("Failed to send create process cmd, ret %u\n", snsz);
        return -1;
    }
    return 0;
}
