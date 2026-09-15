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
**                      include command r52 manager header                      **
*********************************************************************************/

#ifndef _CMD_MANAGER_H_
#define _CMD_MANAGER_H_


#include "cmdef.h"
#include "bqueue.h"
#include "spinlock_t.h"
#include "atomic_t.h"
#include "wait_queue.h"
#include "cmdr52_session.h"
#include "vcx_vcmd_priv.h"


#ifdef __cplusplus
extern "C" {
#endif


#define CORE52_MGR_MAX  2

typedef struct {
    uint32_t           r52coreID;// r52 core ID  from 0
    uint32_t           vtb_size;//
    cmdr52_session_t   vtb[CMD_SESSION_MAX];// vcodec session table
    spinlock_t         spinlock;
    wait_queue_head_t  workwaitqueue;
    atomic_t           refcount;
} core52_mgr_t;

typedef struct {
    uint32_t           ctb_size;//
    core52_mgr_t       ctb[CORE52_MGR_MAX];
    vcmd_mgr_t*        mtb[VCMD_MGR_ID_MAX];// vcmd manager  0-vcmd mgr enc, 1- vcmd mgr dec
    TaskHandle_t       recv_thread[VCMD_MGR_ID_MAX];
    TaskHandle_t       work_thread;
    TaskHandle_t       wait_thread[VCMD_MGR_ID_MAX];// vcmd mgr wait thread for irq cmdbuf done
    BQueueHandle_t     cmd_queue;// command queue
    wait_queue_head_t  workwaitqueue;
    atomic_t           refcount;
} cmdr52_mgr_t;


int32_t              cmdr52_start_mgr(void);

int32_t              cmdr52_exit_mgr(void);

vcmd_mgr_t*          cmdr52_get_vcmd_mgr(uint32_t mgrID);

int32_t              cmdr52_mgr_init(cmdr52_mgr_t *mgr, uint32_t r52coreID);

cmdr52_mgr_t*        cmdr52_mgr_get(void);

cmdr52_session_t*    cmdr52_mgr_get_session(uint32_t sessionID);

cmdr52_session_t*    cmdr52_mgr_get_idle_session(uint32_t r52coreID);

cmdMsg_t*            cmdr52_mgr_dequeue_cmdMsg(void);

cmdMsg_t*            cmdr52_mgr_acquire_cmdMsg(void);

int32_t              cmdr52_mgr_release_cmdMsg(cmdMsg_t* cmdMsg);

int32_t              cmdr52_mgr_queue_cmdMsg(cmdMsg_t* cmdMsg);

int32_t              cmdr52_mgr_cancel_cmdMsg(cmdMsg_t* cmdMsg);

int32_t              cmdr52_mgr_proc_cmdMsg(cmdMsg_t *cmdMsg);

#ifdef __cplusplus
}
#endif

#endif /*_CMD_MANAGER_H_*/
