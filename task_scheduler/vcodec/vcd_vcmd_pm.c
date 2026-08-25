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
**                          *.c vcd vcmd pm source code                         **
*********************************************************************************/

#include "vcd_vcmd_pm.h"
#include "vcx_vcmd_priv.h"
#include "vcx_cmdbuf_obj.h"


#ifdef CONFIG_DEC_PM

/*
 * @brief suspend for vcmd driver power management
 */
int vcmd_vcd_pm_suspend(void *handler)
{
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)handler;
	struct hantrovcmd_dev *dev;
	u32 aborted_id;
	int i;
	unsigned long flags;
	bi_list_node *node;
	struct cmdbuf_obj *obj;

	for (i = 0; i < vcmd_mgr->subsys_num; i++) {
		dev = &vcmd_mgr->dev_ctx[i];

		spin_lock_irqsave(dev->spinlock, flags);
		if (dev->state == VCMD_STATE_WORKING) {
			vcmd_get_executing_cmdbuf(vcmd_mgr, dev, &node);
			obj = (struct cmdbuf_obj *)node->data;
			spin_unlock_irqrestore(dev->spinlock, flags);

			vcmd_abort_mode_set(vcmd_mgr, dev, obj);
			vcmd_abort(vcmd_mgr, dev, &aborted_id);

			spin_lock_irqsave(dev->spinlock, flags);
			if (dev->abort_mode == 1)
				dev->abort_mode = 0;
			if (dev->state != VCMD_STATE_IDLE) {
				vcmd_klog(LOGLVL_ERROR, "suspend failed for dev [%d].", dev->core_id);
				spin_unlock_irqrestore(dev->spinlock, flags);
				return -EBUSY;
			}
			if (!obj->cmdbuf_run_done && obj->slice_run_done)
				obj->executing_status = CMDBUF_EXE_STATUS_SLICE_DECODING_SUSPEND;
		}
		dev->state = VCMD_STATE_POWER_OFF;
		spin_unlock_irqrestore(dev->spinlock, flags);
	}
	return 0;
}

/**
 * @brief resume for vcmd driver power management
 */
int vcmd_vcd_pm_resume(void *handler)
{
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)handler;
	struct hantrovcmd_dev *dev;
	int i;
	unsigned long flags;
	struct cmdbuf_obj *obj;

#ifdef SUPPORT_MMU
	volatile u8 *mmu_hwregs[MAX_SUBSYS_NUM][2];

	for (i = 0; i < MAX_SUBSYS_NUM; i++) {
		mmu_hwregs[i][0] = vcmd_mgr->core_array[i].hwregs[SUB_MOD_MMU];
		mmu_hwregs[i][1] = vcmd_mgr->core_array[i].hwregs[SUB_MOD_MMU_WR];
	}
	MMUSetup(mmu_hwregs);
#endif

#ifdef SUPPORT_AXIFE
	for (i = 0; i < vcmd_mgr->subsys_num; i++)
		AXIFEEnable(vcmd_mgr->core_array[i].hwregs[SUB_MOD_AXIFE]);
#endif

	vcmd_reset_asic(vcmd_mgr);

	for (i = 0; i < vcmd_mgr->subsys_num; i++) {
		dev = &vcmd_mgr->dev_ctx[i];
		spin_lock_irqsave(dev->spinlock, flags);
		obj = &vcmd_mgr->objs[dev->aborted_cmdbuf_id];
		if (dev->state == VCMD_STATE_POWER_OFF &&
				obj->executing_status != CMDBUF_EXE_STATUS_SLICE_DECODING_SUSPEND) {
			dev->state = VCMD_STATE_POWER_ON;
			vcmd_start(dev, 0);
		}
		spin_unlock_irqrestore(dev->spinlock, flags);
	}
	return 0;
}

#endif