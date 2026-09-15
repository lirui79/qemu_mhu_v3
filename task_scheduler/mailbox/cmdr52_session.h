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
**                      include command r52 session header                      **
*********************************************************************************/

#ifndef _COMMANDR52_SESSION_H_
#define _COMMANDR52_SESSION_H_


#include "cmdef.h"
#include "spinlock_t.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CMD_SESSION_MAX  32

/*
typedef enum {
    CMD_SESSION_STATUS_IDLE = 0,
    CMD_SESSION_STATUS_RUN,
    CMD_SESSION_STATUS_EXIT,
    CMD_SESSION_STATUS_STOP
} cmdr52_session_status_t;*/



//session
typedef struct {
    uint64_t               procObj;
    uint32_t               sessionID;// r52id + session_idx
    uint32_t               seqRNum;// sequence number, from 0 to 0xFFFFFFFF
    uint32_t               seqSNum;// sequence number, from 0 to 0xFFFFFFFF
    uint32_t               status;
    uint32_t               total_workload;
    spinlock_t             spinlock;
} cmdr52_session_t;

int32_t        cmdr52_session_init(cmdr52_session_t *session, uint32_t sessionID);

int32_t        cmdr52_session_check(cmdr52_session_t *session, cmdMsg_t *cmdMsg);

int32_t        cmdr52_session_system(cmdr52_session_t *session, cmdMsg_t *cmdMsg);

int32_t        cmdr52_session_vcodec(cmdr52_session_t *session, cmdMsg_t *cmdMsg);

int32_t        cmdr52_session_send(cmdr52_session_t *session, cmdMsg_t *cmdMsg);



#ifdef __cplusplus
}
#endif

#endif /*_COMMANDR52_SESSION_H_*/
