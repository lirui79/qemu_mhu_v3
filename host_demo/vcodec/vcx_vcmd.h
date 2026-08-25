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
**                        include vcx vcmd headers                              **
*********************************************************************************/

#ifndef _VCX_VCMD_H_
#define _VCX_VCMD_H_

#include "vcx_vcmd_priv.h"

/*need to consider how many memory should be allocated for status.*/
struct exchange_parameter {
	/* the instance ctx */
	void *owner;
	//input ;executing_time=encoded_image_size*(rdoLevel+1)*(rdoq+1);
	u64 executing_time;
	u64 interrupt_ctrl; //input ;executing_time=encoded_image_size*(rdoLevel+1)*(rdoq+1);
	/*input input vce=0,IM=1,vcd=2, jpege=3, jpegd=4 */
	u32 module_type;
	/*input, reserve is not used; link and run is input.*/
	u32 cmdbuf_size;
	/* output, it is unique in driver.*/
	u32 cmdbuf_id;
	/* just used for polling. */
	u32 core_id;
	/* core_mask for user to select cores: [0,15]core mask, [16,31]client type. */
	u32 core_mask;
	/* input, bit[0]: priority    - normal=0, high/live=1
	 *        bit[1]: has_end_cmd - last cmd is JMP (0) or END (1) command
	 */
	u32 input_mask;
};


#ifdef __cplusplus
extern "C" {
#endif

void      vce_proc_add_done_job(vcmd_mgr_t *vcmd_mgr, struct cmdbuf_obj *obj);

void      vcd_proc_add_done_job(vcmd_mgr_t *vcmd_mgr, struct cmdbuf_obj *obj);


int32_t   wait_cmdbuf_ready(vcmd_mgr_t *vcmd_mgr, struct proc_obj *po, u16 cmdbuf_id, u16 *done_id);

struct proc_obj *create_process_object(void);

void      free_process_object(struct proc_obj *po);

long      reserve_cmdbuf(vcmd_mgr_t *vcmd_mgr, struct proc_obj *po, struct exchange_parameter *param);

long      release_cmdbuf(vcmd_mgr_t *vcmd_mgr, struct proc_obj *po, u16 cmdbuf_id);

long      link_and_run_cmdbuf(vcmd_mgr_t *vcmd_mgr, struct proc_obj *po, struct exchange_parameter *param);



#ifdef __cplusplus
}
#endif

#endif //_VCX_VCMD_H_
