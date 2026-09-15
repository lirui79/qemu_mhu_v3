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
**                        include command a78 source                            **
*********************************************************************************/

#include "cmda78_mgr.h"
#include "cmda78_proc.h"
#include "cmda78_session.h"
#include "vcx_vcmd_priv.h"



static cmda78_mgr_t g_cmda78_mgr;

static int               _cmda78_init_mgr(cmda78_mgr_t *mgr) {
    int32_t i = 0, j = 0;
    uint32_t  coremask[CMD_R52_MGR_MAX] = {R52_CORE_MASK_VENC, R52_CORE_MASK_VDEC};
    cmd_r52mgr_t *rmgr = NULL;
    mgr->rtb_size  = CMD_R52_MGR_MAX; // r52 core number
    for (i = 0; i < CMD_R52_MGR_MAX; ++i) {
        rmgr = &mgr->rtb[i];
        atomic_set(&rmgr->refcount, 0);
        init_waitqueue_head(&rmgr->workwaitqueue);
        rmgr->coremask  = coremask[i];
        rmgr->workload  = 0;
        rmgr->status    = CMD_R52MGR_STATUS_INIT;
        rmgr->r52coreid = i;
        rmgr->vtb_size  = CMDA78_SESSION_MAX; // session number per r52 core
        rmgr->usedsize  = 0; // current used session number
        spin_lock_init(&rmgr->spinlock);
        clist_init(&rmgr->cmd_queue);
    // 创建并启动内核线程，将 dev 作为参数传入
        for (j = 0 ; j < CMDA78_SESSION_MAX; ++j) {
            uint32_t sessionID = ((rmgr->r52coreid << 16) & 0xFFFF0000) | j;
            cmda78_session_init(&rmgr->vtb[j], NULL, sessionID);
        }
    }

    for (i = 0; i < VCMD_MGR_ID_MAX; ++i) {
        mgr->mtb[i] = NULL;
    }

    mgr->cmd_size = 1024;
    mgr->cmd_data = (uint8_t*)vmalloc(mgr->cmd_size * (32 + CMD_MSG_MAX_SIZE));
    clist_init(&mgr->cmd_free);
    spin_lock_init(&mgr->spinlock);
    for (i = 0; i < mgr->cmd_size; ++i) {
        cnode_t *node = (cnode_t *)(mgr->cmd_data + i * (32 + CMD_MSG_MAX_SIZE));
        cnode_init(node);
        clist_push_back(&mgr->cmd_free, node);
    }

    return  0;
}

int32_t               cmda78_init_mgr(void) {
    return _cmda78_init_mgr(cmda78_get_mgr());
}

int32_t               cmda78_start_mgr(void) {
    return cmda78_thread_create(cmda78_get_mgr());
}

int32_t               cmda78_exit_mgr(void) {
    cmda78_mgr_t* mgr = cmda78_get_mgr();
    cmda78_thread_stop(mgr);
    if (mgr->cmd_data) {
        vfree(mgr->cmd_data);
        mgr->cmd_data = NULL;
    }

    return 0;

}

cmda78_mgr_t*       cmda78_get_mgr(void) {
    return &g_cmda78_mgr;
}

int32_t           cmda78_set_vcmd_mgr(uint32_t mgrID, vcmd_mgr_t* vcmdMgr) {
    cmda78_mgr_t *mgr = NULL;
    if (mgrID >= VCMD_MGR_ID_MAX) {
        return -1;
    }
    mgr = cmda78_get_mgr();
    mgr->mtb[mgrID] = vcmdMgr;
    return 0;
}

vcmd_mgr_t*    cmda78_get_vcmd_mgr(uint32_t mgrID) {
    if (mgrID >= VCMD_MGR_ID_MAX) {
        return NULL;
    }
    return cmda78_get_mgr()->mtb[mgrID];
}

cmda78_session_t*    cmda78_get_session(uint32_t sessionID) {
    uint32_t r52ID = 0, sesID = 0;
    cmd_r52mgr_t *rmgr = NULL;
    cmda78_session_t *session = NULL;
    r52ID  = ((sessionID & 0xFFFF0000) >> 16);
    sesID  = (sessionID & 0xFFFF);
    if ((r52ID >= CMD_R52_MGR_MAX) || (sesID >= CMDA78_SESSION_MAX)) {
        return NULL;
    }
    rmgr = &cmda78_get_mgr()->rtb[r52ID];
    spin_lock(&rmgr->spinlock);
    session = &rmgr->vtb[sesID];
    spin_unlock(&rmgr->spinlock);
    return session;
}


cmda78_session_t*    cmda78_get_minused_r52_session0(void) {
    int32_t i = 0;
    uint32_t minused = 0;
    cmda78_session_t *session = NULL;
    cmda78_mgr_t* mgr = cmda78_get_mgr();
    cmd_r52mgr_t *rmgr = &mgr->rtb[0];

    spin_lock(&rmgr->spinlock);
    minused = rmgr->usedsize;
    session = &rmgr->vtb[0];
    spin_unlock(&rmgr->spinlock);
    for (i = 1; i < CMD_R52_MGR_MAX; ++i) {
        rmgr = &mgr->rtb[i];
        spin_lock(&rmgr->spinlock);
        if (rmgr->usedsize < minused) {
            minused = rmgr->usedsize;
            session = &rmgr->vtb[0];
        }
        spin_unlock(&rmgr->spinlock);
    }
    return session;
}

cmda78_session_t*    cmda78_get_coremask_session0(uint32_t coremask) {
    int32_t i = 0;
    cmda78_session_t *session = NULL;
    cmda78_mgr_t* mgr = cmda78_get_mgr();
    cmd_r52mgr_t *rmgr = NULL;

    for (i = 0; i < CMD_R52_MGR_MAX; ++i) {
        rmgr = &mgr->rtb[i];
        spin_lock(&rmgr->spinlock);
        if (rmgr->coremask & coremask) {
            session = &rmgr->vtb[0];
            spin_unlock(&rmgr->spinlock);
            return session;
        }
        spin_unlock(&rmgr->spinlock);
    }
    return NULL;
}

cmda78_session_t*    cmda78_get_idle_session(void) {
    int32_t i = 0, j = 0;
    cmd_r52mgr_t *rmgr = NULL;
    cmda78_session_t *session = NULL;
    cmda78_mgr_t* mgr = cmda78_get_mgr();
    for (j = 0; j < CMDA78_SESSION_MAX; ++j) {
        for (i = 0; i < CMD_R52_MGR_MAX; ++i) {
            rmgr = &mgr->rtb[i];
            spin_lock(&rmgr->spinlock);
            session = &rmgr->vtb[j];
            spin_unlock(&rmgr->spinlock);
            if (session->status == CMD_SESSION_STATUS_IDLE) {
                return session;
            }
        }
    }

    return NULL;
}

cmdMsg_t*         cmda78_dequeue_cmdMsg(void) {
    cnode_t *cmd_node = NULL;
    cmdMsg_Data_t* cmdMsg_data = NULL;
    cmda78_mgr_t* mgr = cmda78_get_mgr();
    spin_lock(&mgr->spinlock);
    cmd_node = clist_pop_back(&(mgr->cmd_free));
    spin_unlock(&mgr->spinlock);
    if (cmd_node == NULL) {
        return NULL;
    }
    cmdMsg_data = (cmdMsg_Data_t*) container_of(cmd_node, cmdMsg_Data_t, node);
    return (cmdMsg_t *)&(cmdMsg_data->cMsg);
}

cmdMsg_t*         cmda78_acquire_cmdMsg(cmd_r52mgr_t *rmgr) {
    cnode_t *cmd_node = NULL;
    cmdMsg_Data_t* cmdMsg_data = NULL;
    spin_lock(&rmgr->spinlock);
    cmd_node = clist_pop_back(&(rmgr->cmd_queue));
    spin_unlock(&rmgr->spinlock);
    if (cmd_node == NULL) {
        return NULL;
    }
    cmdMsg_data = (cmdMsg_Data_t*) container_of(cmd_node, cmdMsg_Data_t, node);
    return (cmdMsg_t *)&(cmdMsg_data->cMsg);
}

int32_t           cmda78_release_cmdMsg(cmdMsg_t* cmdMsg) {
    cmdMsg_Data_t* cmdMsg_data = (cmdMsg_Data_t*) container_of(cmdMsg, cmdMsg_Data_t, cMsg);
    cmda78_mgr_t* mgr = cmda78_get_mgr();
    int32_t  code = 0;
    spin_lock(&mgr->spinlock);
    code = clist_push_back(&(mgr->cmd_free), &(cmdMsg_data->node));
    spin_unlock(&mgr->spinlock);
    return code;
}

int32_t           cmda78_queue_cmdMsg(cmd_r52mgr_t *rmgr, cmdMsg_t* cmdMsg) {
    cmdMsg_Data_t* cmdMsg_data = (cmdMsg_Data_t*) container_of(cmdMsg, cmdMsg_Data_t, cMsg);
    int32_t  code = 0;
    spin_lock(&rmgr->spinlock);
    code = clist_push_back(&(rmgr->cmd_queue), &(cmdMsg_data->node));
    spin_unlock(&rmgr->spinlock);
    return code;
}

int32_t           cmda78_cancel_cmdMsg(cmdMsg_t* cmdMsg) {
    cmdMsg_Data_t* cmdMsg_data = (cmdMsg_Data_t*) container_of(cmdMsg, cmdMsg_Data_t, cMsg);
    cmda78_mgr_t* mgr = cmda78_get_mgr();
    int32_t  code = 0;
    spin_lock(&mgr->spinlock);
    code = clist_push_back(&(mgr->cmd_free), &(cmdMsg_data->node));
    spin_unlock(&mgr->spinlock);
    return code;
}

void print_byte_array(const char *label, const uint8_t *arr, size_t len) {
    char buf[512] = {0};
    int32_t pos = 0;
    printk("%s (Length: %zu):\n", label, len);
    printk("  Hex: ");
    for (size_t i = 0; i < len; i++) {
        pos += sprintf(buf + pos, "%02X", arr[i]);
    }
    printk("%s", buf);
    printk("\n");
}

static uint32_t cmda78_check(cmdMsg_t *cmdMsg, cmda78_session_t **session)
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
        printk("QUEUE:ptr=%08x magic=%x ver=%d type=%x size=%u sid=%x seq=%x crc32=%x rc32Now:%x\n", \
            (uint32_t)(uintptr_t)cmdMsg, cmdMsg->magic, cmdMsg->version, cmdMsg->cmdType, \
            cmdMsg->cmdSize, cmdMsg->sessionID, cmdMsg->seqNum, cmdMsg->crc32, crc32Now);
        print_byte_array("cmdMsg", (const uint8_t *)cmdMsg, cmdMsg->cmdSize);
        goto RETURN_ERROR;
    }

    *session = cmda78_get_session(cmdMsg->sessionID);
    if (*session == NULL) {
        retCode = CMD_ERR_INVALID_SESSIONID;
        goto RETURN_ERROR;
    }
    retCode = CMD_ERR_SUCCESS;

RETURN_ERROR:
    return retCode;
}


int32_t cmda78_proc_cmdMsg(cmdMsg_t *cmdMsg) {
    cmda78_session_t *session = NULL;
    uint32_t retCode = CMD_ERR_SUCCESS;

    retCode = cmda78_check(cmdMsg, &session);
    if (retCode != CMD_ERR_SUCCESS) {
        cmda78_release_cmdMsg(cmdMsg);
        return retCode;
    }

    if (cmda78_session_check(session, cmdMsg) < 0) {
        cmda78_release_cmdMsg(cmdMsg);
        return CMD_ERR_INVALID_SEQUENCEID;
    }

    if (cmdMsg->cmdType <= CMD_SYSTEM_MAX) {
        return cmda78_session_system(session, cmdMsg);
    }

    return cmda78_session_vcodec(session, cmdMsg);
}

int32_t              cmda78_add_cmdMsg(cmda78_session_t *session, cmdMsg_t *cmdMsg) {
    cmda78_mgr_t* mgr = cmda78_get_mgr();
    uint32_t r52ID = ((session->sessionID & 0xFFFF0000) >> 16);
    cmd_r52mgr_t *rmgr = &mgr->rtb[r52ID];
    int32_t code = 0;
    spin_lock(&session->spinlock);
    cmdMsg->seqNum       = session->seqSNum++;
    code = cmda78_queue_cmdMsg(rmgr, cmdMsg);
    spin_unlock(&session->spinlock);
    atomic_inc(&rmgr->refcount);
    wake_up_interruptible(&rmgr->workwaitqueue);
    return code;
}