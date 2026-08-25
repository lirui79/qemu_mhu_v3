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
**                       include debug log headers                              **
*********************************************************************************/

#ifndef _VCX_VCMD_DBG_LOG_H_
#define _VCX_VCMD_DBG_LOG_H_

#ifdef __FREERTOS__
#include "osal_freertos.h" /* needed for the _IOW etc stuff used later */
#endif

#include "vcx_vcmd_priv.h"

#ifdef __cplusplus
extern "C" {
#endif


#ifdef VCMD_DEBUG_INTERNAL

void _dbg_log_instr(u32 offset, u32 instr, u32 *size, char *str);

void _dbg_log_last_cmd(struct cmdbuf_obj *obj);

void _dbg_log_cmdbuf(struct cmdbuf_obj *obj);

void _dbg_log_dev_regs(struct hantrovcmd_dev *dev, u32 dump);

void printk_vcmd_register_debug(const void *hwregs, char *info);

#endif

#ifdef __cplusplus
}
#endif

#endif /*_VCX_VCMD_DBG_LOG_H_*/
