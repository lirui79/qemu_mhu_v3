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


void vcmd_init_objs(vcmd_mgr_t *vcmd_mgr);

void vcmd_init_nodes(vcmd_mgr_t *vcmd_mgr);

void proc_add_done_job(vcmd_mgr_t *vcmd_mgr, struct cmdbuf_obj *obj);

void vcmd_start(struct hantrovcmd_dev *dev, int irq);

int vcmd_abort(vcmd_mgr_t *vcmd_mgr, struct hantrovcmd_dev *dev, u32 *aborted_id);

long reserve_cmdbuf(vcmd_mgr_t *vcmd_mgr, cmdr52_session_t *session,	struct exchange_parameter *param);

int32_t vcmd_release_cmdbuf(vcmd_mgr_t *vcmd_mgr, u16 cmdbuf_id);

long link_and_run_cmdbuf(vcmd_mgr_t *vcmd_mgr, cmdr52_session_t *session, struct exchange_parameter *param);

int32_t vcmd_link_and_rum_cmdbuf(vcmd_mgr_t *vcmd_mgr, cmdr52_session_t *session, cmdReqRunCmdBuf_Body_t *cmd_body);

int32_t vcmd_wait_cmdbuf_ready(vcmd_mgr_t *vcmd_mgr, u16 cmdbuf_id, u16 *done_id);

void vcmd_reset_asic(vcmd_mgr_t *vcmd_mgr);

int abort_vce(volatile u8 *reg_base);

int abort_vcd(volatile u8 *reg_base);

u32 vcmd_abort_mode_set(vcmd_mgr_t *vcmd_mgr, struct hantrovcmd_dev *dev, struct cmdbuf_obj *obj);

void vcmd_get_executing_cmdbuf(vcmd_mgr_t *vcmd_mgr, struct hantrovcmd_dev *dev, bi_list_node **node);

bi_list_node *get_cmdbuf_node_by_addr(vcmd_mgr_t *vcmd_mgr, struct hantrovcmd_dev *dev, ptr_t pa);

int dev_delink_job(struct hantrovcmd_dev *dev, bi_list_node *node);

int32_t vcmd_flush_slice_regs(vcmd_mgr_t *vcmd_mgr, u16 cmdbuf_id);

int32_t vcmd_polling_cmdbuf(vcmd_mgr_t *vcmd_mgr, u16 core_id);

int32_t vcmd_abort_cmdbuf(vcmd_mgr_t *vcmd_mgr, u16 cmdbuf_id);

int32_t vcmd_drop_owner(vcmd_mgr_t *vcmd_mgr, cmdr52_session_t *session, uint64_t ownerID, cmdRspDropOwner_Body_t *cmd_body);


#ifdef __cplusplus
}
#endif

#endif /*_VCX_CMDBUF_OBJ_H_*/
