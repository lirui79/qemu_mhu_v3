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
**                         *.c vcx irq timer source code                        **
*********************************************************************************/

#include "vcx_kthread.h"
#include "vcx_irq_timer.h"



#ifdef TIMEOUT_IRQ_TIMER

/**
 * @brief timer callback of vcmd timeout timer
 */
static void _vcmd_timeout_timer_cb(TimerHandle_t xTimer)
{
	vcmd_mgr_t *vcmd_mgr;
	struct hantrovcmd_dev *dev;

 // 1. 获取用户 ID，区分是哪个定时器
    dev = (struct hantrovcmd_dev *) pvTimerGetTimerID(xTimer);
//	dev = container_of(timer, struct hantrovcmd_dev, timeout_timer);
	vcmd_mgr = (vcmd_mgr_t *)dev->handler;

	dev->kthread_actions |= KT_ACT_HW_TIMEOUT;
	_vcmd_kthread_wakeup(vcmd_mgr, KT_ACT_HW_TIMEOUT);
}


void _vcmd_timeout_create_timer(struct hantrovcmd_dev *dev, unsigned int timeout) {
    dev->timeout_timer =  xTimerCreate(
        "irq timer",
        pdMS_TO_TICKS(timeout),
        pdFALSE, // 单次模式
        (void *)dev,
        _vcmd_timeout_timer_cb
    );
    dev->timeout_timer_active = 0;
}

void _vcmd_timeout_stop_timer(struct hantrovcmd_dev *dev) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (dev->timeout_timer_active == 0)
        return;
        // 无效信号：立即停止定时器，防止回调执行
    xTimerStopFromISR(dev->timeout_timer, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    dev->timeout_timer_active = 0;
}

void _vcmd_timeout_start_timer(struct hantrovcmd_dev *dev) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    BaseType_t xResult;
    // 方法 A: 简单粗暴法 - 直接重置
    // xTimerResetFromISR 的行为：
    // - 如果定时器未运行，它等同于 Start。
    // - 如果定时器正在运行，它将重新开始计时。
    xResult = xTimerResetFromISR(dev->timeout_timer, &xHigherPriorityTaskWoken);

    if (xResult != pdPASS) {
        // 失败原因通常是命令队列已满 (configTIMER_QUEUE_LENGTH 太小)
        // 在实际产品中，可能需要增加队列长度或记录错误
    }

    // 3. 关键步骤：检查是否需要切换任务
    // 如果 Timer Service Task 的优先级高于当前被中断的任务，
    // xHigherPriorityTaskWoken 将被设置为 pdTRUE
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

    dev->timeout_timer_active = 1;
}

/**
 * @brief delete vcmd timeout timer
 */
void _vcmd_timeout_delete_timer(struct hantrovcmd_dev *dev)
{
	if (dev->timeout_timer) {
        xTimerDelete(dev->timeout_timer, 0);
	}
	dev->timeout_timer_active = 0;
}

#endif //TIMEOUT_IRQ_TIMER