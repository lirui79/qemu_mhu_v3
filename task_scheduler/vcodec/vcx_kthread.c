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
**                          *.c vcx kthread source code                         **
*********************************************************************************/

#include "vcx_kthread.h"
#include "vcx_watchdog.h"
#include "vcx_cmdbuf_obj.h"
#include <limits.h>


/********************************************************************
 * watchdog and kthread functions
 ********************************************************************/


/**
 * @brief sw process flow for v1.6.x bus err.
 */
static void _vcmd_bus_err_process(struct hantrovcmd_dev *dev)
{
	struct vcmd_subsys_info *subsys = dev->subsys_info;
	unsigned long flags;

	spin_lock_irqsave(dev->spinlock, flags);
	if (dev->hw_feature.vcarb_ver == VCARB_VERSION_3_0) {
		// PF do vcd/vce abort and AXIFE flush
		vcmd_start(dev, 0);
		spin_unlock_irqrestore(dev->spinlock, flags);
		return;
	}
	if (subsys->sub_module_type == VCMD_TYPE_ENCODER) {
		abort_vce(subsys->hwregs[SUB_MOD_MAIN]);
	}
	if (subsys->sub_module_type == VCMD_TYPE_DECODER) {
		abort_vcd(subsys->hwregs[SUB_MOD_MAIN]);
	}
#ifdef SUPPORT_AXIFE
	if (AXIFEFlush(subsys->hwregs[SUB_MOD_AXIFE]) == -1) //	if (AXIFEFlush(subsys->hwregs[SUB_MOD_AXIFE0]) == -1) //VCMD_TYPE_ENCODER
		vcmd_klog(LOGLVL_ERROR, "%s: AXIFE flush failed!", __func__);
#endif
	if (dev->state == VCMD_STATE_IDLE)
		vcmd_start(dev, 0);
	spin_unlock_irqrestore(dev->spinlock, flags);
}

#ifdef AXI2TO1_SUPPORT
/**
 * @brief process subsystem exceptions
 */
static int process_subsystem_exceptions(struct hantrovcmd_dev *dev)
{
	int ret;
	u32 val;
	unsigned long flags;

	volatile u8 *axi2to1_hwregs = dev->subsys_info->hwregs[SUB_MOD_AXI2TO1];

	if (!axi2to1_hwregs) {
		vcmd_klog(LOGLVL_ERROR, "AXI2TO1 is not exsit!\n");
		return -1;
	}

	spin_lock_irqsave(dev->spinlock, flags);
	ret = AXI2TO1_flush(axi2to1_hwregs);
	if (ret < 0) {
		spin_unlock_irqrestore(dev->spinlock, flags);
		return ret;
	}

	if (dev->hw_version_id < HW_ID_1_6_0) {
		// VCMD reset
		val = vcmd_read_reg((const void *)dev->hwregs, 0x40);
		val |= (0x1 << 1);
		vcmd_write_reg((const void *)dev->hwregs, 0x40, val);

		// IP core and AXI2TO1 reset
		val = SUBSYSTEM_RESET(IP_RESET_CORE);
		val |= SUBSYSTEM_RESET(IP_RESET_AXI2TO1);
		val |= SUBSYSTEM_RESET(IP_RESET_DEC400);
		val |= SUBSYSTEM_RESET(IP_RESET_AXIF);
		val |= SUBSYSTEM_RESET(IP_RESET_MMU);
		vcmd_write_reg((const void *)dev->hwregs, 0x40, val);

		// release IP core and AXI2TO1 reset
		vcmd_write_reg((const void *)dev->hwregs, 0x40, 0);
	} else {
		// IP core and AXI2TO1 reset
		val = vcmd_read_reg((const void *)dev->hwregs, 0x40);
		val |= SUBSYSTEM_RESET(IP_RESET_CORE);
		val |= SUBSYSTEM_RESET(IP_RESET_AXI2TO1);
		val |= SUBSYSTEM_RESET(IP_RESET_DEC400);
		val |= SUBSYSTEM_RESET(IP_RESET_AXIF);
		val |= SUBSYSTEM_RESET(IP_RESET_MMU);
		vcmd_write_reg((const void *)dev->hwregs, 0x40, val);
		// VCMD reset
		val = (0x1 << 1);
		vcmd_write_reg((const void *)dev->hwregs, 0x40, val);

		// after vcmd reset, need not to release ip reset
	}

	/* recover vcmd registers and continue to encode/decode */
	dev->state = VCMD_STATE_POWER_ON;
	vcmd_start(dev, 0);
	spin_unlock_irqrestore(dev->spinlock, flags);

	return 0;
}
#endif

/**
 * @brief process external vcmd timeout.
 */
static void hook_vcmd_external_timeout(void *_dev)
{
	unsigned long flags;
	struct hantrovcmd_dev *dev = (struct hantrovcmd_dev *)_dev;

	spin_lock_irqsave(dev->spinlock, flags);
	dev->state = VCMD_STATE_IDLE;
	spin_unlock_irqrestore(dev->spinlock, flags);

	vcmd_klog(LOGLVL_ERROR, "vcmd abort is not waited, the timeout is from external system!\n");

	//need to do sub-system reset
}

/**
 * @brief vcmd kernel thread main function
 */
static void _vcmd_kthread_fn(void *pvParameters)
{
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)pvParameters;
	struct hantrovcmd_dev *dev = NULL;
    int i;

	while (1) {
        uint32_t ulNotifiedValue;
        xTaskNotifyWait(0, ULONG_MAX, &ulNotifiedValue, portMAX_DELAY);
        for (i = 0; i < vcmd_mgr->subsys_num; i++) {
            dev = &vcmd_mgr->dev_ctx[i];

            if (ulNotifiedValue & KT_ACT_HW_TIMEOUT) {
                // 处理数据就绪
                // if external timeout, will do system reset
                if (dev->kthread_actions & KT_ACT_HW_TIMEOUT) {
                    dev->kthread_actions = 0;
                    hook_vcmd_external_timeout(dev);
                }
            }

            if (ulNotifiedValue & KT_ACT_HW_BUS_ERR) {
                if (dev->kthread_actions & KT_ACT_HW_BUS_ERR) {
                    dev->kthread_actions = 0;
                    _vcmd_bus_err_process(dev);
                }
            }
#ifdef AXI2TO1_SUPPORT
            if (ulNotifiedValue & (KT_ACT_CMDBUF_TIMEOUT | KT_ACT_AXI2TO1_EXCEPTION)) {
                // if has exceptions, will reset subsystem
                if (dev->kthread_actions & (KT_ACT_CMDBUF_TIMEOUT | KT_ACT_AXI2TO1_EXCEPTION)) {
                    dev->kthread_actions = 0;
                    process_subsystem_exceptions(dev);
                }
            }
#endif
#ifdef SUPPORT_WATCHDOG
            if (ulNotifiedValue & KT_ACT_WATCHDOG) {
                if (dev->kthread_actions & KT_ACT_WATCHDOG) {
                    dev->kthread_actions = 0;
                    _vcmd_watchdog_process(dev);
                }
            }
#endif
        }

/*
		if (wait_event_interruptible(vcmd_mgr->kthread_waitq,
				_vcmd_kthread_actions(vcmd_mgr, &dev))) {
			vcmd_klog(LOGLVL_ERROR, "%s: signaled!!!\n", __func__);
			return -ERESTARTSYS;
		}
		if (dev == NULL)
			continue;

		if (dev->kthread_actions & KT_ACT_HW_TIMEOUT) {
			dev->kthread_actions = 0;
			// if external timeout, will do system reset 
			hook_vcmd_external_timeout(dev);
			continue;
		}
		if (dev->kthread_actions & KT_ACT_HW_BUS_ERR) {
			dev->kthread_actions = 0;
			_vcmd_bus_err_process(dev);
			continue;
		}
#ifdef AXI2TO1_SUPPORT
		if (dev->kthread_actions &
				(KT_ACT_CMDBUF_TIMEOUT | KT_ACT_AXI2TO1_EXCEPTION)) {
			dev->kthread_actions = 0;
			// if has exceptions, will reset subsystem
			process_subsystem_exceptions(dev);
			continue;
		}
#endif
#ifdef SUPPORT_WATCHDOG
		if (dev->kthread_actions & KT_ACT_WATCHDOG) {
			dev->kthread_actions = 0;
			_vcmd_watchdog_process(dev);
			continue;
		}
#endif
*/
	}
}

/**
 * @brief wake up vcmd kernel thread
 */
void _vcmd_kthread_wakeup(vcmd_mgr_t *vcmd_mgr, unsigned int value)
{    
    // 【正确】在回调中发送通知给任务
    // 注意：这里使用的是 xTaskNotify，而不是 xTaskNotifyFromISR
    xTaskNotify(vcmd_mgr->kthread, value, eNoAction); 
}

void _vcmd_kthread_wakeup_irq(vcmd_mgr_t *vcmd_mgr, unsigned int value)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE; // 1. 必须初始化为 pdFALSE
    // 使用句柄精准唤醒 SensorTask
    if (vcmd_mgr->kthread == NULL) {
        return;
    }

    // 2. 发送通知
    // 示例：唤醒任务，不传具体数据
    xTaskNotifyFromISR(
        vcmd_mgr->kthread,   // 目标任务句柄
        value,                   // 值（eNoAction 下无关紧要）
        eNoAction,           // 动作
        &xHigherPriorityTaskWoken // 传入地址
    );

    // 3. 【必须】检查是否需要上下文切换
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
//	wake_up_interruptible_all(&vcmd_mgr->kthread_waitq);
}

/**
 * @brief create kernel thread for vcmd driver
 */
void _vcmd_kthread_create(vcmd_mgr_t *vcmd_mgr)
{
/*	vcmd_mgr->stop_kthread = 0;
	init_waitqueue_head(&vcmd_mgr->kthread_waitq);
	vcmd_mgr->kthread =
		kthread_run(_vcmd_kthread_fn, (void *)vcmd_mgr, "vcmd_kthread");
	if (IS_ERR(vcmd_mgr->kthread)) {
		vcmd_klog(LOGLVL_ERROR, "create vcmd kthread failed\n");
		return;
	}*/
    BaseType_t xStatus;

    // 2. 创建任务 1：优先级 1，栈深 128 字 (512 字节)，传入参数 1
    xStatus = xTaskCreate(
        _vcmd_kthread_fn,           // 任务函数
        "vcmd_kthread",       // 名称
        128,                // 栈深度 (Words)
        (void *)vcmd_mgr,          // 参数: LED ID 1
        1,                  // 优先级
        &vcmd_mgr->kthread           // 句柄
    );

    if (xStatus != pdPASS) {
       vcmd_klog(LOGLVL_ERROR, "create vcmd kthread failed\n");
       vcmd_mgr->kthread = NULL;
    }
}

/**
 * @brief stop kernel thread vcmd driver
 */
void _vcmd_kthread_stop(vcmd_mgr_t *vcmd_mgr)
{
	if (NULL != (vcmd_mgr->kthread)) {
        vTaskDelete(vcmd_mgr->kthread);//		kthread_stop(vcmd_mgr->kthread);
		vcmd_mgr->kthread = NULL;
	}
}


