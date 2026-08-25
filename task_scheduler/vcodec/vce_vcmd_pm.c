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
**                          *.c vce vcmd pm source code                         **
*********************************************************************************/

#include "vce_vcmd_pm.h"
#include "vcx_vcmd_priv.h"
#include "vcx_cmdbuf_obj.h"


#ifdef CONFIG_ENC_PM
/**
 * @brief suspend for vcmd driver power management
 */
int vcmd_vce_pm_suspend(void *handler)
{
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)handler;
	struct hantrovcmd_dev *dev;
	u32 aborted_id;
	int i;
	unsigned long flags;

	for (i = 0; i < vcmd_mgr->subsys_num; i++) {
		dev = &vcmd_mgr->dev_ctx[i];

		spin_lock_irqsave(dev->spinlock, flags);
		if (dev->state == VCMD_STATE_WORKING) {
			spin_unlock_irqrestore(dev->spinlock, flags);
			vcmd_abort(vcmd_mgr, dev, &aborted_id);
			spin_lock_irqsave(dev->spinlock, flags);
			if (dev->state != VCMD_STATE_IDLE) {
				vcmd_klog(LOGLVL_ERROR, "suspend failed for dev [%d].", dev->core_id);
				return -EBUSY;
			}
		}
		dev->state = VCMD_STATE_POWER_OFF;
		spin_unlock_irqrestore(dev->spinlock, flags);
	}
	return 0;
}

/**
 * @brief resume for vcmd driver power management
 */
int vcmd_vce_pm_resume(void *handler)
{
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)handler;
	struct hantrovcmd_dev *dev;
	int i;
	unsigned long flags;

#ifdef SUPPORT_AXIFE
	struct vcmd_subsys_info *subsys;

	for (i = 0; i < vcmd_mgr->subsys_num; i++) {
		subsys = &vcmd_mgr->core_array[i];
		if (subsys->hwregs[SUB_MOD_AXIFE0])
			AXIFEEnable(subsys->hwregs[SUB_MOD_AXIFE0], 1);
		if (subsys->hwregs[SUB_MOD_AXIFE1])
			AXIFEEnable(subsys->hwregs[SUB_MOD_AXIFE1], 1);
	}
#endif
#ifdef SUPPORT_MMU
	MMUSetup(vcmd_mgr->mmu_hwregs);
#endif
	vcmd_reset_asic(vcmd_mgr);

	for (i = 0; i < vcmd_mgr->subsys_num; i++) {
		dev = &vcmd_mgr->dev_ctx[i];
		spin_lock_irqsave(dev->spinlock, flags);
		if (dev->state == VCMD_STATE_POWER_OFF) {
			dev->state = VCMD_STATE_POWER_ON;
			vcmd_start(dev, 0);
		}
		spin_unlock_irqrestore(dev->spinlock, flags);
	}
	return 0;
}

#endif