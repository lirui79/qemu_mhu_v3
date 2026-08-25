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

//#define  VCMD_ALLOC_MEM

#ifdef __FREERTOS__
#include "osal_freertos.h" /* needed for the _IOW etc stuff used later */
#endif


//#define ANY_CMDBUF_ID                       (0xFFFF)
/* R52 本地 RAM 已扩到 16MB:vcmd_mgr_r52[2] 静态 bss = SLOT×48B×2 管理器,
 * 256 槽需 24.6KB,空间充足。A76 侧 host_demo 的 utils/inc.h 同样保持 256。 */
#define SLOT_NUM_CMDBUF						(256)


typedef enum {
    CMD_SESSION_STATUS_IDLE = 0,
    CMD_SESSION_STATUS_RUN,
    CMD_SESSION_STATUS_EXIT,
    CMD_SESSION_STATUS_USE
} cmda78_session_status;

#endif //_UTILS_INC_H_