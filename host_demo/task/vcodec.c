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
**                           *.c vcodec source code                             **
*********************************************************************************/

#include "cmdef.h"
#include "vcodec.h"
//#include "cmdr52_mgr.h"
#include "cmdnode.h"
#include "cmda78_mgr.h"

static  int32_t  vcmd_mgr_init(vcmd_mgr_t *vcmd_mgr, uint32_t vcmd_mgr_id) {
	vcmd_mgr_set_vcmd_mgr_id(vcmd_mgr, vcmd_mgr_id);
    init_waitqueue_head(&vcmd_mgr->job_waitq);
	spin_lock_init(&vcmd_mgr->job_lock);
	for (int i = 0 ; i < SLOT_NUM_CMDBUF; i++) {
		vcmd_mgr->objs[i].owner = 0;
		vcmd_mgr->objs[i].interrupt_ctrl = 0;
		vcmd_mgr->objs[i].cmdbuf_size = 256;
		vcmd_mgr->objs[i].module_type = 0;
		vcmd_mgr->objs[i].core_mask = 0;
		vcmd_mgr->objs[i].core_id = 0;
		vcmd_mgr->objs[i].cmdbuf_id = i;
		vcmd_mgr->objs[i].cmdbuf_run_done = 0;
		vcmd_mgr->objs[i].slice_run_done = 0;
		vcmd_mgr->objs[i].line_buffer_run_done = 0;
		vcmd_mgr->objs[i].po = NULL;
		//vcmd_mgr->objs[i].session = NULL;
	}
	 ts_printf("%s:%s:%d started\n", __FILE__, __func__, __LINE__);
	return  0;
}

// A78 core
int32_t              vcodeca78_init(void) {
    static vcmd_mgr_t vcmd_mgr_a78[2];
	vcmd_mgr_init(&vcmd_mgr_a78[0], VCMD_MGR_ID_ENC);
	vcmd_mgr_init(&vcmd_mgr_a78[1], VCMD_MGR_ID_DEC);
	cmdnode_init();
	cmda78_init_mgr();
	cmda78_set_vcmd_mgr(VCMD_MGR_ID_ENC, &vcmd_mgr_a78[0]);
	cmda78_set_vcmd_mgr(VCMD_MGR_ID_DEC, &vcmd_mgr_a78[1]);
	 ts_printf("%s:%s:%d started\n", __FILE__, __func__, __LINE__);
    return  cmda78_start_mgr();
}

void                 vcodeca78_exit(void) {

}

/*
//R52 core
int32_t              vcodecr52_init(void) {
    static vcmd_mgr_t vcmd_mgr_r52[2];
	vcmd_mgr_init(&vcmd_mgr_r52[0], VCMD_MGR_ID_ENC);
	vcmd_mgr_init(&vcmd_mgr_r52[1], VCMD_MGR_ID_DEC);

	cmdr52_mgr_init(cmdr52_mgr_get(), 0);
    cmdr52_mgr_get()->mtb[VCMD_MGR_ID_ENC] = &vcmd_mgr_r52[0];
    cmdr52_mgr_get()->mtb[VCMD_MGR_ID_DEC] = &vcmd_mgr_r52[1];
	return  cmdr52_start_mgr();
}


void                 vcodecr52_exit(void) {

}
*/