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
**                           include cmd a78 proc header                        **
*********************************************************************************/

#ifndef _COMMANDA78_PROC_H_
#define _COMMANDA78_PROC_H_

#include "cmdef.h"

#ifdef __cplusplus
extern "C" {
#endif


int32_t    cmda78_send(cmdMsg_t *cmdMsg);

//int32_t cmd_proc(void);

//uint32_t   crc32_calc(const uint8_t *buffer, size_t bufferLength);

int32_t    cmda78_thread_create(void* arg);

int32_t    cmda78_thread_stop(void* arg);

int32_t    cmda78_thread_wakeup(uint32_t r52CoreID);

int32_t    cmda78_thread_wakeup_from_isr(uint32_t r52CoreID, BaseType_t *pxHigherPriorityTaskWoken);


#ifdef __cplusplus
}
#endif

#endif /*_COMMANDA78_PROC_H_*/
