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
**                         *.c vcx watchdog source code                         **
*********************************************************************************/

#include "vcx_kthread.h"
#include "vcx_watchdog.h"
#include "vcx_cmdbuf_obj.h"
#include <limits.h>





/****************************************************************
 * watchdog functions
 ***************************************************************/

#ifdef SUPPORT_WATCHDOG
/**
 * @brief hook function for system-driver to do further process
 *  for tiggered watchdog.
 */
void hook_vcmd_watchdog(struct hantrovcmd_dev *dev, int succeed)
{
	if (succeed) {
		vcmd_klog(LOGLVL_ERROR, "axife flush succeed!!\n");
	} else {
		/*TODO*/
		vcmd_klog(LOGLVL_ERROR, "axife flush failed, need to re-power sub-system!\n");
	}
}

/**
 * @brief reset vcmd arbiter when VCARB hang
 */
void watchdog_reset_vcmd_arbiter(struct hantrovcmd_dev *dev)
{
	u32 arb = 0;
	int loop_cnt = 0;
	unsigned long flags;

	spin_lock_irqsave(dev->spinlock, flags);

	arb = vcmd_read_reg((const void *)dev->hwregs,
						VCMD_REGISTER_ARB_OFFSET);
	arb &= 0xfffffff8;
	vcmd_write_reg((const void *)dev->hwregs, VCMD_REGISTER_ARB_OFFSET, arb);
	spin_unlock_irqrestore(dev->spinlock, flags);

	do {
		if (dev->arb_reset_irq == 1) {
			dev->arb_reset_irq = 0;
			vcmd_klog(LOGLVL_BRIEF, "VCARB is hang, reset it!\n");
			return;
		}
		vTaskDelay(pdMS_TO_TICKS(10));	//	mdelay(10); // wait 10ms
	} while (++loop_cnt < 100);

	vcmd_klog(LOGLVL_ERROR, "Reseted VCARB, but failed!\n");
}

/**
 * @brief watchdog wait vcmd abort done
 */
int watchdog_wait_vcmd_aborted(struct hantrovcmd_dev *dev)
{
	int loop_cnt = 0;
	u32 state;

	do {
		state = vcmd_get_register_value((const void *)dev->hwregs,
									dev->reg_mirror, HWIF_VCMD_WORK_STATE);
		if (state == HW_WORK_STATE_IDLE) {
			//aborted
			return 0;
		}
		vTaskDelay(pdMS_TO_TICKS(10));	//	mdelay(10); // wait 10ms
	} while (++loop_cnt < 100);

	vcmd_klog(LOGLVL_ERROR, "%s, can't go to IDLE!\n", __func__);

	return -1;
}

/**
 * @brief stop vcmd when watchdog triggered
 */
int watchdog_stop_vcmd(struct hantrovcmd_dev *dev)
{
	u32 state;

	state = vcmd_get_register_value((const void *)dev->hwregs,
				dev->reg_mirror, HWIF_VCMD_WORK_STATE);

	if (state == HW_WORK_STATE_IDLE || state == HW_WORK_STATE_PEND) {
		dev->state = VCMD_STATE_POWER_ON;
		return 0;
	}

	//if state is not in IDLE/PEND, abort VCMD by sw.
	if (dev->hw_feature.vcarb_ver == VCARB_VERSION_2_0) {
		vcmd_set_reg_mirror(dev->reg_mirror, HWIF_VCMD_ABORT_MODE, 0x0);
	} else {
		dev->abort_mode = 0x1;
		vcmd_set_reg_mirror(dev->reg_mirror, HWIF_VCMD_ABORT_MODE, 0x1);
	}
	vcmd_write_register_value((const void *)dev->hwregs,
					dev->reg_mirror,
					HWIF_VCMD_START_TRIGGER, 0);
	dev->state = VCMD_STATE_POWER_ON;
	if (dev->hw_feature.vcarb_ver == VCARB_VERSION_2_0)
		return 0;

	//wait vcmd core aborted and vcmd enters IDLE mode.
	if (watchdog_wait_vcmd_aborted(dev) == 0)
		return 0;

	return -1;
}


/**
 * @brief stop vce when watchdog triggered
 */
static int watchdog_stop_vce(volatile u8 *reg_base)
{
	u32 loop_count = 20;
	u32 irq;

	if (abort_vce(reg_base)) {
		// wait vce core disable.
		do {
			if (((u32)ioread32((void __iomem *)(reg_base + 0x14)) & 0x1) == 0) {
				/* clear reg1 by writing 1 */
				irq = (u32)ioread32((void __iomem *)(reg_base + 0x4));
				iowrite32(irq, (void __iomem *)(reg_base + 0x4));
				return 0;
			}
			vTaskDelay(pdMS_TO_TICKS(10));//mdelay(10); // wait 10ms
		} while (loop_count--);
		vcmd_klog(LOGLVL_ERROR, "hantroenc: too long before vce core disable\n");
		return -1;
	}
	return 0;
}

/**
 * @brief stop vcd when watchdog triggered
 */
static int watchdog_stop_vcd(volatile u8 *reg_base)
{
	u32 status;//#define HANTRODEC_IRQ_STAT_DEC       1  #define HANTRODEC_IRQ_STAT_DEC_OFF   (HANTRODEC_IRQ_STAT_DEC * 4)
	if (abort_vcd(reg_base)) {
		vTaskDelay(pdMS_TO_TICKS(10));//mdelay(10); //delay 10ms
		status = (u32)ioread32((void __iomem *)(reg_base + 0x04));//status = (u32)ioread32((void __iomem *)(reg_base + HANTRODEC_IRQ_STAT_DEC_OFF));
		//Stop VCD by setting reg1 bit0 to 0.
		if ((status & 0x1) == 0)
			iowrite32(0, (void __iomem *)(reg_base + 0x04));//iowrite32(0, (void __iomem *)(reg_base + HANTRODEC_IRQ_STAT_DEC_OFF));
	}

	return 0;
}

/**
 * @brief process when watchdog triggered.
 */
void _vcmd_watchdog_process(struct hantrovcmd_dev *dev)
{
	struct vcmd_subsys_info *subsys = dev->subsys_info;
	int succeed = 1, ret = 0;
	unsigned long flags;

	spin_lock_irqsave(dev->spinlock, flags);
	if (dev->hw_feature.vcarb_ver <= VCARB_VERSION_2_0) {
		ret = watchdog_stop_vcmd(dev);
		if (subsys->sub_module_type == VCMD_TYPE_ENCODER) {
		    watchdog_stop_vce(subsys->hwregs[SUB_MOD_MAIN]);
		}
		if (subsys->sub_module_type == VCMD_TYPE_DECODER) {
		    watchdog_stop_vcd(subsys->hwregs[SUB_MOD_MAIN]);
		}
	}
	if (dev->hw_feature.vcarb_ver == VCARB_VERSION_2_0) {
		spin_unlock_irqrestore(dev->spinlock, flags);
		vTaskDelay(pdMS_TO_TICKS(10));//mdelay(10); // wait 10ms
		/* if not own arbier, it will receive arberr interrupt */
		if (dev->arb_err_irq) {
			dev->arb_err_irq = 0;
			return;
		}
		spin_lock_irqsave(dev->spinlock, flags);
		ret = watchdog_wait_vcmd_aborted(dev);
		spin_unlock_irqrestore(dev->spinlock, flags);
		if (ret == 0)
			watchdog_reset_vcmd_arbiter(dev);
		spin_lock_irqsave(dev->spinlock, flags);
	} else if (dev->hw_feature.vcarb_ver == VCARB_VERSION_3_0) {
		vcmd_start(dev, 0);
		spin_unlock_irqrestore(dev->spinlock, flags);
		return;
	}
	if (ret < 0)
		succeed = 0;

#ifdef SUPPORT_AXIFE
	if (subsys->sub_module_type == VCMD_TYPE_ENCODER) {
		if (succeed)
			succeed = (AXIFEFlush(subsys->hwregs[SUB_MOD_AXIFE0]) != -1);
		if (succeed && subsys->reg_off[SUB_MOD_AXIFE1] != 0xffff)
			succeed = (AXIFEFlush(subsys->hwregs[SUB_MOD_AXIFE1]) != -1);
	}
	if (subsys->sub_module_type == VCMD_TYPE_DECODER) {
		if (succeed && AXIFEFlush(subsys->hwregs[SUB_MOD_AXIFE]) == -1)
			succeed = 0;
	}
#else
	succeed = 0;
#endif
	hook_vcmd_watchdog(dev, succeed);
	spin_unlock_irqrestore(dev->spinlock, flags);
	if (succeed) {
		vTaskDelay(pdMS_TO_TICKS(10));//mdelay(10); // wait 10ms
		if (dev->state == VCMD_STATE_IDLE) {
			spin_lock_irqsave(dev->spinlock, flags);
			if (dev->abort_mode == 1)
				dev->abort_mode = 0;
			vcmd_start(dev, 0);
			spin_unlock_irqrestore(dev->spinlock, flags);
		}
	}
}

/**
 * @brief wait jobs count that need to be finished from dev work_list
 */
u32 dev_wait_job_num(struct hantrovcmd_dev *dev)
{
	struct cmdbuf_obj *obj;
	bi_list_node *node = dev->work_list.head;
	u32 num = 0;

	while (node) {
		obj = (struct cmdbuf_obj *)node->data;
		if (obj->cmdbuf_run_done == 0) {
			num++;
			if (obj->has_jmp_cmd == 0 || obj->jmp_ie)
				return num;
		}
		node = node->next;
	}

	return num;
}

/**
 * @brief timer callback function of watchdog
 */

static void _vcmd_watchdog_cb(TimerHandle_t xTimer)
{
	vcmd_mgr_t *vcmd_mgr;
	struct hantrovcmd_dev *dev;

 // 1. 获取用户 ID，区分是哪个定时器
    dev = (struct hantrovcmd_dev *) pvTimerGetTimerID(xTimer);
//	dev = container_of(timer, struct hantrovcmd_dev, timeout_timer);
	vcmd_mgr = (vcmd_mgr_t *)dev->handler;

	dev->kthread_actions |= KT_ACT_WATCHDOG;
	_vcmd_kthread_wakeup(vcmd_mgr, KT_ACT_WATCHDOG);
}

void _vcmd_watchdog_create(struct hantrovcmd_dev *dev, unsigned int timeout) {
    dev->watchdog_timer =  xTimerCreate(
        "watchdog timer",
        pdMS_TO_TICKS(timeout),
        pdFALSE, // 单次模式
        (void *)dev,
        _vcmd_watchdog_cb
    );
    dev->watchdog_active = 0;
}

/**
 * @brief init vcmd watchdog
 */
void _vcmd_watchdog_start(struct hantrovcmd_dev *dev, int irq)
{
	if (irq == 0) {
       xTimerReset(dev->watchdog_timer, portMAX_DELAY);
	} else  {
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		BaseType_t xResult;
		// 方法 A: 简单粗暴法 - 直接重置
		// xTimerResetFromISR 的行为：
		// - 如果定时器未运行，它等同于 Start。
		// - 如果定时器正在运行，它将重新开始计时。
		xResult = xTimerResetFromISR(dev->watchdog_timer, &xHigherPriorityTaskWoken);

		if (xResult != pdPASS) {
			// 失败原因通常是命令队列已满 (configTIMER_QUEUE_LENGTH 太小)
			// 在实际产品中，可能需要增加队列长度或记录错误
		}

		// 3. 关键步骤：检查是否需要切换任务
		// 如果 Timer Service Task 的优先级高于当前被中断的任务，
		// xHigherPriorityTaskWoken 将被设置为 pdTRUE
	    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
    dev->watchdog_active = 1;
}

/**
 * stop vcmd watchdog
 */
void _vcmd_watchdog_stop(struct hantrovcmd_dev *dev, int irq)
{
	if (dev->watchdog_active == 0) {
		return;
	}
	if (irq == 0) {
		xTimerStop(dev->watchdog_timer, portMAX_DELAY);
	} else {
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
// 无效信号：立即停止定时器，防止回调执行
		xTimerStopFromISR(dev->watchdog_timer, &xHigherPriorityTaskWoken);
	    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}

	dev->watchdog_active = 0;
}

static unsigned int _vcmd_watchdog_ChangePeriod(struct hantrovcmd_dev *dev, unsigned int timeout, int irq) {
	if (irq == 0) {
		if (xTimerChangePeriod(dev->watchdog_timer, pdMS_TO_TICKS(timeout), portMAX_DELAY) == pdPASS) {
			return 0;
		}
	} else {
		// 1. 初始化唤醒标志
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;

		// 2. 【关键】在中断中修改定时器周期为 10ms
		if (xTimerChangePeriodFromISR(dev->watchdog_timer, pdMS_TO_TICKS(timeout), &xHigherPriorityTaskWoken) != pdPASS) {
			// 处理失败：队列可能满了
			return -1;
		}

		// 3. 【关键】如果唤醒了高优先级任务，执行上下文切换
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
	return 0;
}

/**
 * feed and start vcmd watchdog
 */
void _vcmd_watchdog_feed(struct hantrovcmd_dev *dev, int irq)
{
	u32 num = 0;
	num = dev_wait_job_num(dev);
	if (num == 0) {
		if (dev->watchdog_active)
			_vcmd_watchdog_stop(dev, irq);
	} else {
		unsigned int timeout = num * ONE_JOB_WAIT_TIME;
		if (dev->watchdog_active == 0)
			_vcmd_watchdog_start(dev, irq);

		if (dev->hw_feature.vcarb_ver == VCARB_VERSION_2_0)
            timeout = VCMD_ARBITER_RESET_TIME;
		else if (dev->hw_feature.vcarb_ver == VCARB_VERSION_3_0)
            timeout = vcodec_get_config()->sw_timeout_time;
		else
			timeout = num * ONE_JOB_WAIT_TIME;
		_vcmd_watchdog_ChangePeriod(dev, timeout, irq);
	}
}

void _vcmd_watchdog_delete(struct hantrovcmd_dev *dev) {
	if (dev->watchdog_timer == NULL)
       return;
	xTimerDelete(dev->watchdog_timer, 0);
	dev->watchdog_active = 0;
}

#endif //SUPPORT_WATCHDOG
