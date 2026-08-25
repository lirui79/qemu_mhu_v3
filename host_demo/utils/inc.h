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
**                               include  inc header                            **
*********************************************************************************/

#ifndef _UTILS_INC_H_
#define _UTILS_INC_H_


#include <stdint.h>
#include <stddef.h>

#ifdef __FREERTOS__
#include "osal_freertos.h" /* needed for the _IOW etc stuff used later */
#endif


#define ANY_CMDBUF_ID                       (0xFFFF)
#define SLOT_NUM_CMDBUF						(256)


typedef enum {
    CMD_SESSION_STATUS_IDLE = 0,
    CMD_SESSION_STATUS_RUN,
    CMD_SESSION_STATUS_EXIT,
    CMD_SESSION_STATUS_USE
} cmda78_session_status;

#endif //_UTILS_INC_H_