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
**                         include vcx cmdbuf oby header                        **
*********************************************************************************/

#ifndef _VCX_CMDBUF_OBJ_H_
#define _VCX_CMDBUF_OBJ_H_

#ifdef __FREERTOS__
#include "osal_freertos.h" /* needed for the _IOW etc stuff used later */
#endif

#include "cmdef.h"
#include "vcx_vcmd.h"
#include "vcx_vcmd_priv.h"


#ifdef __cplusplus
extern "C" {
#endif

void    vcmd_add_done_job(vcmd_mgr_t *vcmd_mgr, struct cmdbuf_obj *obj);

int     vcmd_get_done_job(vcmd_mgr_t *vcmd_mgr, struct cmdbuf_obj **pobj);

int32_t vcmd_release_cmdbuf(vcmd_mgr_t *vcmd_mgr, u16 cmdbuf_id);

int32_t vcmd_wait_cmdbuf_ready(vcmd_mgr_t *vcmd_mgr, u16 cmdbuf_id, u16 *done_id);


int32_t vcmd_flush_slice_regs(vcmd_mgr_t *vcmd_mgr, u16 cmdbuf_id);


int32_t vcmd_polling_cmdbuf(vcmd_mgr_t *vcmd_mgr, u16 core_id);

int32_t vcmd_abort_cmdbuf(vcmd_mgr_t *vcmd_mgr, u16 cmdbuf_id);





#ifdef __cplusplus
}
#endif

#endif /*_VCX_CMDBUF_OBJ_H_*/
