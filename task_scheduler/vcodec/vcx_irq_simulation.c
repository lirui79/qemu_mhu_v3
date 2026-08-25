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
**                       *.c vcx irq simulation source code                     **
*********************************************************************************/

#include "vcx_vcmd_irq.h"
#include "vcx_irq_simulation.h"


/****************************************************************
 * irq_simulation functions
 ***************************************************************/

#ifdef IRQ_SIMULATION

struct timer_manager {
	void *vcmd_mgr;
	void *obj;
	TimerHandle_t timer;
};

static struct timer_manager irq_simul_ctx[SLOT_NUM_CMDBUF];

/**
 * @brief trigger isr processing for irq simulation.
 */
void _irq_simul_trigger_isr(TimerHandle_t xTimer)
{
	struct timer_manager *mgr = NULL;
	struct cmdbuf_obj *obj = NULL;
    mgr = (struct timer_manager *) pvTimerGetTimerID(xTimer);

	if (mgr->obj) {
		obj = (struct cmdbuf_obj *)mgr->obj;
	} else {
		vcmd_klog(LOGLVL_ERROR, "tigger isr failed, the cmdbuf obj is NULL\n");
		return;
	}

	vcmd_klog(LOGLVL_FLOW, "trigger core 0 irq\n");
	hantrovcmd_isr(obj->core_id, mgr->vcmd_mgr);
	mgr->obj = NULL;
	mgr->timer = NULL;
	xTimerDelete(xTimer, 0);
}

/**
 * @brief add timer for irq simulation.
 */
void _irq_simul_add_timer(struct cmdbuf_obj *obj)
{
	u64 random_num =  (u32)((u64)100 * obj->workload / VCMD_WORKLOAD_UNIT + 50);
	irq_simul_ctx[obj->cmdbuf_id].obj = (void *)obj;
    irq_simul_ctx[obj->cmdbuf_id].timer =  xTimerCreate(
        "irq sim timer",
        pdMS_TO_TICKS(random_num),
        pdFALSE, // 单次模式
        (void *)&irq_simul_ctx[obj->cmdbuf_id],
        _irq_simul_trigger_isr
    );

	vcmd_klog(LOGLVL_FLOW, "random_num=%lld\n", random_num);

}

/**
 * @brief init timers for irq simulation.
 */
void _irq_simul_init(void *vcmd_mgr)
{
	u32 i;

	for (i = 0; i < SLOT_NUM_CMDBUF; i++) {
		irq_simul_ctx[i].obj = NULL;
		irq_simul_ctx[i].vcmd_mgr = vcmd_mgr;
	}
}

#endif //IRQ_SIMULATION