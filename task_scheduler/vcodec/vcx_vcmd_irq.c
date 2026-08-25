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
**                         *.c vcx vcmd irq source code                         **
*********************************************************************************/

#include "vcx_kthread.h"
#include "vcx_watchdog.h"
#include "vcx_vcmd_irq.h"
#include "vcx_cmdbuf_obj.h"
#include "vcx_vcmd_dbgfs.h"
#include "vce_abnormal_irq.h"
#include "vcd_abnormal_irq.h"
#include "vcx_vcmd_dbg_log.h"



/**
 * @brief process abnormal interrupt
 */
static void process_abnormal_irq(vcmd_mgr_t *vcmd_mgr,
									struct hantrovcmd_dev *dev,
									struct cmdbuf_obj *obj)
{
	u32 intr_src;

	/* check vcmd interrupt source. */
	intr_src = vcmd_read_reg((const void *)dev->hwregs,
								VCMD_REGISTER_SW_EXT_INT_SRC_OFFSET);
	if (dev->subsys_info->sub_module_type == VCMD_TYPE_ENCODER) {
	/* abnormal interrupt source from VCE */
       if (intr_src & ENC_ABN_IRQ_MASK)
           process_vce_abn_irq(vcmd_mgr, dev, obj);
	}
	if (dev->subsys_info->sub_module_type == VCMD_TYPE_DECODER) {
    /* abnormal interrupts source from VCD */
       if (intr_src & dev->vcd_abn_irq_mask)
           process_vcd_abn_irq(vcmd_mgr, dev, obj);
	}

#ifdef AXI2TO1_SUPPORT
	/* abnormal interrupt source from AXI2TO1 */
	if (intr_src & AXI2TO1_ABN_IRQ_MASK)
		process_axi2to1_abn_irq(vcmd_mgr, dev);
	/*TODO*/
	/* 1. set the IP core exceptions interrup type as abnormal
	 * 2. IP core checked the exceptoions, then need do process_subsystem_exceptions
	 */
#endif
}

/**
 * @brief mark cmdbuf obj as run_done with specified exe_status,
 * remove it from device work_list, and add it to owner po's job_done_list.
 */
static int isr_process_node(vcmd_mgr_t *vcmd_mgr, bi_list_node *node,
							u32 exe_status)
{
	struct cmdbuf_obj *obj = (struct cmdbuf_obj *)node->data;
	struct hantrovcmd_dev *dev = &vcmd_mgr->dev_ctx[obj->core_id];

	if (obj->cmdbuf_run_done == 0) {
		obj->cmdbuf_run_done = 1;
		obj->executing_status = exe_status;
		dev_delink_job(dev, node);
		proc_add_done_job(vcmd_mgr, obj);
#ifdef SUPPORT_DBGFS
		if (exe_status == CMDBUF_EXE_STATUS_OK) {
			_dbgfs_remove_cmdbuf(dev->dbgfs_info, obj->cmdbuf_id);
			_dbgfs_record_vcx_cycles(dev->dbgfs_info, obj->cmdbuf_id,
						obj->module_type, dev->hw_feature.vcarb_ver);
		}
#endif
		return 0;
	} else {
		vcmd_klog(LOGLVL_BRIEF, "%s cmdbuf[%d] is already done!!\n",
				__func__, obj->cmdbuf_id);
		return -1;
	}
}


#ifndef TIMEOUT_IRQ_TIMER
/**
 * @brief reset core to the specified vcmd hw device.
 */
static void vcmd_reset_current_asic(struct hantrovcmd_dev *dev)
{
	u32 status;

	volatile u8 *hwregs = dev->hwregs;

	if (hwregs) {
		//disable interrupt at first
		vcmd_write_reg((const void *)hwregs,
				   VCMD_REGISTER_INT_CTL_OFFSET, 0x0000);
		//reset core
		vcmd_write_reg((const void *)hwregs,
				   VCMD_REGISTER_CONTROL_OFFSET, 0x0004);
		//read status register
		status = vcmd_read_reg((const void *)hwregs,
					   VCMD_REGISTER_INT_STATUS_OFFSET);
		//clean status register
		vcmd_write_reg((const void *)hwregs,
				   VCMD_REGISTER_INT_STATUS_OFFSET, status);
		//when reset core need clear reg[3]
		vcmd_write_reg((const void *)hwregs,
						VCMD_REGISTER_EXE_CMDBUF_COUNT_OFFSET, 0x0000);
	}
}
#endif


/**
 * @brief interrupt service routine of vcmd driver.
 */
irqreturn_t hantrovcmd_isr(int irq, void *handler)
{
	unsigned int handled = 0;
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)handler;
	struct hantrovcmd_dev *dev = NULL;
	u32 irq_status = 0;
	unsigned long flags;
	bi_list_node *node;
	ptr_t exe_cmdbuf_ba;
	u32 curr_id = 0;
	struct cmdbuf_obj *curr_obj, *obj;
	bi_list_node *curr_node;

	u32 i;
	volatile u8 *hwregs;

	if (vcmd_mgr->vcmd_irq_enabled == 0) {
		/* all vcmd_irq==-1, there is no IRQ, use irq as dev id */
		if (irq < vcmd_mgr->subsys_num)
			dev = &vcmd_mgr->dev_ctx[irq];
	} else { /* there is assigned IRQ */
		for (i = 0; i < vcmd_mgr->subsys_num; i++) {
			if (vcmd_mgr->dev_ctx[i].subsys_info->irq == irq) {
				dev = &vcmd_mgr->dev_ctx[i];
				break;
			}
		}
	}

	if (dev == NULL)
		return IRQ_HANDLED;

#ifdef SUPPORT_DBGFS
	_dbgfs_reset_exe_cmdbuf_num(dev->dbgfs_info);
	_dbgfs_record_cmdbuf_num(dev->dbgfs_info);
#endif

	hwregs = dev->hwregs;

	spin_lock_irqsave(dev->spinlock, flags);
	if (dev->state == VCMD_STATE_POWER_OFF) {
		spin_unlock_irqrestore(dev->spinlock, flags);
		return IRQ_HANDLED;
	}
	irq_status = vcmd_read_reg((const void *)hwregs,
				   VCMD_REGISTER_INT_STATUS_OFFSET);

	if (dev->hw_version_id >= HW_ID_1_5_0 && irq_status == 0) {
		curr_id = vcmd_get_register_value((const void *)dev->hwregs,
							dev->reg_mirror,
							HWIF_VCMD_CMDBUF_EXE_ID);
		if (curr_id >= SLOT_NUM_CMDBUF) {
			vcmd_klog(LOGLVL_ERROR, "%s error cmdbuf_id %d !!\n",
						__func__, curr_id);
			spin_unlock_irqrestore(dev->spinlock, flags);
			return IRQ_HANDLED;
		}
		curr_obj = &vcmd_mgr->objs[curr_id];

		process_abnormal_irq(vcmd_mgr, dev, curr_obj);

		irq_status = vcmd_read_reg((const void *)dev->hwregs,
									VCMD_REGISTER_INT_STATUS_OFFSET);
		if (!irq_status) {
			spin_unlock_irqrestore(dev->spinlock, flags);
			return IRQ_HANDLED;
		}
	}

#ifdef VCMD_DEBUG_INTERNAL
	_dbg_log_dev_regs(dev, 1);
#endif

	if (irq_status == 0) {
		//no interrupt source from dev
		//vcmd_klog(LOGLVL_BRIEF, "%s warning, irq_status is zero!\n", __func__);
		spin_unlock_irqrestore(dev->spinlock, flags);
		return IRQ_HANDLED;
	}

	vcmd_klog(LOGLVL_FLOW, "%s: received IRQ!\n", __func__);
	vcmd_klog(LOGLVL_FLOW, "irq_status of core[%u] is: 0x%x\n", dev->core_id, irq_status);

	vcmd_write_reg((const void *)hwregs,
						VCMD_REGISTER_INT_STATUS_OFFSET, irq_status);
	dev->reg_mirror[VCMD_REGISTER_INT_STATUS_OFFSET / 4] = irq_status;

	node = dev->work_list.head;

	if (node == NULL) {
		//dev is not in use, should not run to here
		vcmd_klog(LOGLVL_ERROR, "%s:received IRQ but core has nothing to do.\n", __func__);
		spin_unlock_irqrestore(dev->spinlock, flags);
		return IRQ_HANDLED;
	}

	// get current node/obj/id
	if (dev->hw_version_id < HW_ID_1_0_C) {
		exe_cmdbuf_ba = VCMDGetAddrRegisterValue((const void *)hwregs,
												dev->reg_mirror,
												HWIF_VCMD_CMDBUF_EXE_ADDR);
		//get the executing cmdbuf node.
		curr_node = get_cmdbuf_node_by_addr(vcmd_mgr, dev, exe_cmdbuf_ba);
		if (curr_node == NULL) {
			vcmd_klog(LOGLVL_ERROR, "%s no node bind to executing cmd ba 0x%llx!!\n",
											__func__, exe_cmdbuf_ba);
			spin_unlock_irqrestore(dev->spinlock, flags);
			return IRQ_HANDLED;
		}
		curr_obj = (struct cmdbuf_obj *)node->data;
		curr_id = curr_obj->cmdbuf_id;
	} else {
		if (irq_status & VCMD_IRQ_ERR_MASK) {
			//if error, read curr_id from register directly.
			curr_id = vcmd_get_register_value((const void *)hwregs,
									dev->reg_mirror,
									HWIF_VCMD_CMDBUF_EXE_ID);

		} else {
			//otherwise, read curr_id from vcmd reg_mem
			curr_id = *(dev->reg_mem_va + REG_ID_CMDBUF_EXE_ID);
		}

		if (curr_id >= SLOT_NUM_CMDBUF) {
			vcmd_klog(LOGLVL_ERROR, "%s error cmdbuf_id %d !!\n",  __func__, curr_id);
			spin_unlock_irqrestore(dev->spinlock, flags);
			return IRQ_HANDLED;
		}

		curr_node = &vcmd_mgr->nodes[curr_id];
		curr_obj = &vcmd_mgr->objs[curr_id];

#ifdef VCMD_DEBUG_INTERNAL
		_dbg_log_dev_regs(dev, 0);
#endif

	}

	if (curr_obj->core_id != dev->core_id) {
		vcmd_klog(LOGLVL_ERROR, "%s error cmdbuf_id, core_id[%d] is not dev id[%d] !!\n",
					  __func__, curr_id, dev->core_id);
		spin_unlock_irqrestore(dev->spinlock, flags);
		return IRQ_HANDLED;
	}

	/* process nodes between head node and curr_node (exclusive):
	 * 1. mark run_done = 1 & executing status = OK,
	 * 2. remove from dev work list.
	 * 3. add to owner's job_done_list
	 */
	if (curr_obj->cmdbuf_run_done == 0) {
		node = dev->work_list.head;
		while (node && node != curr_node) {
			obj = (struct cmdbuf_obj *)node->data;
			isr_process_node(vcmd_mgr, node, CMDBUF_EXE_STATUS_OK);

			node = node->next;
		}

		if (node == NULL) {
			vcmd_klog(LOGLVL_ERROR, "%s not find node[%d] in dev work_list!!\n",
						__func__, curr_obj->cmdbuf_id);
			spin_unlock_irqrestore(dev->spinlock, flags);
			return IRQ_HANDLED;
		}
	} else {
		// only occurs when for error irq
		if (!(irq_status & VCMD_IRQ_ERR_MASK)) {
			vcmd_klog(LOGLVL_ERROR, "%s normal irq trigger to already done cmdbuf[%d]\n",
						__func__, curr_obj->cmdbuf_id);
		}
	}

	if (dev->hw_feature.vcarb_ver == VCARB_VERSION_2_0 &&
		(irq_status & VCMD_IRQ_ARBITER_RESET)) {
		// vcmd arbiter reset err
		dev->arb_reset_irq = 1;
		spin_unlock_irqrestore(dev->spinlock, flags);
		return IRQ_HANDLED;
	}

	if (dev->hw_feature.vcarb_ver &&
		(irq_status & VCMD_IRQ_ARBITER_ERR)) {
		// vcmd arbiter err if not own arbiter
		dev->arb_err_irq = 1;
		spin_unlock_irqrestore(dev->spinlock, flags);
		return IRQ_HANDLED;
	}

	if (dev->hw_feature.has_cmdbuf_timeout &&
			irq_status & VCMD_IRQ_CMDBUF_TIMEOUT) {
		// cmdbuf execution timeout, need do subsystem reset process
		dev->state = VCMD_STATE_IDLE;
		isr_process_node(vcmd_mgr, curr_node, CMDBUF_EXE_STATUS_CMDBUF_TIMEOUT);
		dev->kthread_actions |= KT_ACT_CMDBUF_TIMEOUT;
		spin_unlock_irqrestore(dev->spinlock, flags);
		_vcmd_kthread_wakeup_irq(vcmd_mgr, KT_ACT_CMDBUF_TIMEOUT);
		return IRQ_HANDLED;
	}

	//curr_node process
	if (irq_status & VCMD_IRQ_ABORT) {
#ifdef TIMEOUT_IRQ_TIMER
		/* if vcmd abort waited, del vcmd timeout timer */
		//_vcmd_timeout_del_timer(dev);
        _vcmd_timeout_stop_timer(dev);
#endif
#ifdef SUPPORT_WATCHDOG
		_vcmd_watchdog_stop(dev, 1);
#endif
		//abort error
		dev->state = VCMD_STATE_IDLE;
		dev->aborted_cmdbuf_id = curr_id;

		if (dev->abort_mode == 0) {
			// curr node is done
			isr_process_node(vcmd_mgr, curr_node, CMDBUF_EXE_STATUS_OK);
		} else {
			// curr node is aborted
			if (dev->subsys_info->sub_module_type == VCMD_TYPE_ENCODER) {
                curr_obj->executing_status = CMDBUF_EXE_STATUS_ABORTED;
			}
			if (dev->subsys_info->sub_module_type == VCMD_TYPE_DECODER) {
                /* if current obj is aborted imedialy in slice decoding pm suspend, set the
                 * executing_status as SLICE_DECODING_SUSPEND.
                 */
				if (curr_obj->executing_status != CMDBUF_EXE_STATUS_SLICE_DECODING_SUSPEND)
				   curr_obj->executing_status = CMDBUF_EXE_STATUS_ABORTED;
			}
			
		}

		spin_unlock_irqrestore(dev->spinlock, flags);

		//to notify owner which triggered the abort
		//wake_up_interruptible_all(dev->abort_waitq);
		{
			BaseType_t xHigherPriorityTaskWoken = pdFALSE;
			xSemaphoreGiveFromISR(dev->abort_waitq, &xHigherPriorityTaskWoken);
			portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
		}

		handled++;
		return IRQ_HANDLED;
	}

	/* before v1.6.0: not report bus err interrupt, because it will trigger abort
	 *                and ensure vcmd is idle.
	 * v1.6.0 - : when bus err, it will abort vcmd but not report abort interrupt,
	 *            and waiting bus clean, report bus err to CPU.
	 */
	if (irq_status & VCMD_IRQ_BUS_ERR) {
		//bus error, don't need to reset where to record status?
		dev->state = VCMD_STATE_IDLE;
		isr_process_node(vcmd_mgr, curr_node, CMDBUF_EXE_STATUS_BUSERR);

		/* will do futher process in kernel thread*/
		dev->kthread_actions |= KT_ACT_HW_BUS_ERR;
		//vcmd_start(dev, 1);
		spin_unlock_irqrestore(dev->spinlock, flags);
		_vcmd_kthread_wakeup_irq(vcmd_mgr, KT_ACT_HW_BUS_ERR);

		handled++;
		return IRQ_HANDLED;
	}

	if (irq_status & VCMD_IRQ_TIMEOUT) {
		//time out
#ifdef TIMEOUT_IRQ_TIMER
		if ((irq_status & VCMD_IRQ_END) == 0) {
			// start a timer to wait abort irq
			//_vcmd_timeout_add_timer(dev);
            _vcmd_timeout_start_timer(dev);
		}
#else //TIMEOUT_IRQ_TIMER
		//reset dev and re-start from curr node
		dev->state = VCMD_STATE_IDLE;

		vcmd_reset_current_asic(dev);
		vcmd_start(dev, 1);
#endif //TIMEOUT_IRQ_TIMER
		spin_unlock_irqrestore(dev->spinlock, flags);

		handled++;
		return IRQ_HANDLED;
	}
	if (irq_status & VCMD_IRQ_CMD_ERR) {
#ifdef TIMEOUT_IRQ_TIMER
		/* if vcmd cmderr waited, del vcmd timeout timer */
		//_vcmd_timeout_del_timer(dev);
        _vcmd_timeout_stop_timer(dev);
#endif
		//command error, re-start from next node
		dev->state = VCMD_STATE_IDLE;
		isr_process_node(vcmd_mgr, curr_node, CMDBUF_EXE_STATUS_CMDERR);

		vcmd_start(dev, 1);
		spin_unlock_irqrestore(dev->spinlock, flags);

		handled++;
		return IRQ_HANDLED;
	}

	//JMP or END interrupt
	isr_process_node(vcmd_mgr, curr_node, CMDBUF_EXE_STATUS_OK);
	if (irq_status & VCMD_IRQ_END) {
#ifdef TIMEOUT_IRQ_TIMER
		/* if vcmd end waited, del vcmd timeout timer */
		//_vcmd_timeout_del_timer(dev);
        _vcmd_timeout_stop_timer(dev);
#endif
		//end command interrupt, start next node
		dev->state = VCMD_STATE_IDLE;
		vcmd_start(dev, 1);
	} else {
#ifdef SUPPORT_WATCHDOG
		_vcmd_watchdog_feed(dev, 1);
#endif
	}
	spin_unlock_irqrestore(dev->spinlock, flags);
	handled++;

#ifdef SUPPORT_DBGFS
	_dbgfs_update_index(dev->dbgfs_info);
#endif

	if (!handled)
		vcmd_klog(LOGLVL_BRIEF, "IRQ received, but not hantro's!\n");

	return IRQ_HANDLED;
}
