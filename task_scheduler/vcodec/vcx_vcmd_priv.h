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
**                          include vcx vcmd priv header                        **
*********************************************************************************/

#ifndef _VCX_VCMD_PRIV_H_
#define _VCX_VCMD_PRIV_H_

#ifdef __FREERTOS__
#include "osal_freertos.h"
#endif
#include "inc.h"
#include "wait_queue.h"



#ifdef __cplusplus
extern "C" {
#endif

enum vcmd_module_type {
	VCMD_TYPE_ENCODER = 0,
	VCMD_TYPE_CUTREE,
	VCMD_TYPE_DECODER,
	VCMD_TYPE_JPEG_ENCODER,
	VCMD_TYPE_JPEG_DECODER,
	MAX_VCMD_TYPE
};

struct proc_obj {
    void              *session;//cmda78_session_t *session;
};


struct cmdbuf_obj {
	u64                owner;
	u64                interrupt_ctrl;
	u32                cmdbuf_size;
	u32                module_type;
    u32                core_mask;
	u16                core_id;
	u16                cmdbuf_id;
	u8                 cmdbuf_run_done;
	u8                 slice_run_done;
	u8                 line_buffer_run_done;
    struct proc_obj   *po;
    void              *session;//cmdr52_session_t  *session;
};


typedef struct {
    struct cmdbuf_obj  objs[SLOT_NUM_CMDBUF];
    uint32_t           vcmd_mgr_id;
    wait_queue_head_t  job_waitq;
    spinlock_t         job_lock;
} vcmd_mgr_t;


void vcmd_mgr_set_vcmd_mgr_id(vcmd_mgr_t *vcmd_mgr, uint32_t vcmd_mgr_id);


#ifdef __cplusplus
}
#endif

#endif //_VCX_VCMD_PRIV_H_