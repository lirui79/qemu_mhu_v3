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
**                         *.c cmd a78 proc source code                         **
*********************************************************************************/

#include "crc32.h"
#include "vcx_vcmd.h"
#include "cmda78_mgr.h"
#include "mhu_davarae.h"
#include "cmda78_proc.h"


static void doorbell_irq_callback_t(uint32_t irq, uint32_t channel) {
    cmd_r52mgr_t *rmgr = &(cmda78_get_mgr()->rtb[(channel - 1) / 2]);

    if (irq == 0) {

    } else {

    }
    ts_printf("doorbell_irq_callback_t\n");
}

static void fastchan_irq_callback_t(uint32_t irq, uint32_t channel) {
    cmd_r52mgr_t *rmgr = &(cmda78_get_mgr()->rtb[(channel - 1) / 2]);

    if (irq == 0) {

    } else {

    }
    ts_printf("fastchan_irq_callback_t\n");
}

static void fifochan_irq_callback_t(uint32_t irq, uint32_t channel) {
    cmd_r52mgr_t *rmgr = &(cmda78_get_mgr()->rtb[(channel - 1) / 2]);

    if (irq == 0) {
        atomic_inc(&rmgr->refcount);
        wake_up_interruptible(&rmgr->workwaitqueue);
    } else {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        atomic_inc_from_isr(&rmgr->refcount);
        wake_up_interruptible_from_isr(&rmgr->workwaitqueue, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
 
    ts_printf("fifochan_irq_callback_t %u %u\n", irq, channel);
}

void        cmda78_set_callback(void) {
    mhu_set_irq_callback(0, doorbell_irq_callback_t);
    mhu_set_irq_callback(1, fastchan_irq_callback_t);
    mhu_set_irq_callback(2, fifochan_irq_callback_t);
}

static void cmda78_work_thread_proc(void *arg) {
    cmda78_mgr_t *mgr = (cmda78_mgr_t*) arg;
    cmdMsg_t *cmdMsg = NULL;
    BaseType_t retCode = 0;

    ts_printf("%s:%s:%d started\n", __FILE__, __func__, __LINE__);

    while (1) {
        retCode = wait_event_interruptible(mgr->workwaitqueue, atomic_read(&mgr->refcount) > 0);
        ts_printf("%s:%s:%d %d\n", __FILE__, __func__, __LINE__, retCode);
        if (retCode == pdFALSE) {
            continue;
        }

        cmdMsg = cmda78_acquire_cmdMsg();
        if (cmdMsg == NULL) {
            continue;
        }
//        ts_printf("QUEUE:ptr=%08x magic=%x ver=%d type=%x size=%u sid=%x seq=%x crc=%x\n", \
            (uint32_t)(uintptr_t)cmdMsg, cmdMsg->magic, cmdMsg->version, cmdMsg->cmdType, \
            cmdMsg->cmdSize, cmdMsg->sessionID, cmdMsg->seqNum, cmdMsg->crc32);
        retCode = cmda78_proc_cmdMsg(cmdMsg);
        //cmda78_release_cmdMsg(cmdMsg);
        atomic_dec(&mgr->refcount);
        ts_printf("%s:%s:%d %d\n", __FILE__, __func__, __LINE__, retCode);
    } 
    ts_printf("Worker thread exiting\n");
}

static void cmda78_recv_thread_func(void *arg) {
    cmd_r52mgr_t *rmgr = (cmd_r52mgr_t *)arg;
    cmda78_mgr_t *mgr = (cmda78_mgr_t*) cmda78_get_mgr();
    cmdMsg_t *cmdMsg = NULL;
    BaseType_t retCode;
    int32_t code = 0;
    uint32_t ch = 2 * rmgr->r52coreid + 1;

    ts_printf("%s:%s:%d started\n", __FILE__, __func__, __LINE__);
    while (1) {
        /* 平台 MBX 组合中断投递不可靠(实测有时 ISR 不触发),若只依赖
         * irq_callback_fifo 经 ISR 唤醒本线程,R52 的响应会一直锁存在
         * 硬件里无人处理,导致本线程永久睡眠、A76 等响应死锁。显式轮询
         * 硬件并入软件标志(内部经 irq_callback_fifo 置 refcount 并唤醒),
         * 兜住无中断路径。 */
        //mhu_poll_rx();
        /* 用带超时的等待:即便 poll 后 refcount 仍为 0(响应尚未到达),
         * 也能周期性回到循环继续轮询;若响应到达,ISR/poll 会即时唤醒。 */
        retCode = wait_event_interruptible_timeout(rmgr->workwaitqueue,
                                                   (atomic_read(&rmgr->refcount) > 0)
                                                   || (mhu_fifo_rx_fill(MHU_MBX_BASE, ch) >= CMD_MSG_MIN_SIZE),
                                                   pdMS_TO_TICKS(200));
        if (retCode == pdFALSE) {
            continue;   /* 超时未就绪,回到循环顶部继续轮询 */
        }

        /* 仅有中断标志但 FIFO 空(例如 doorbell 通知),无需接收:清标志后跳过 */
        if (mhu_fifo_rx_fill(MHU_MBX_BASE, ch) < CMD_MSG_MIN_SIZE) {
            if (atomic_read(&rmgr->refcount) > 0)
                atomic_dec(&rmgr->refcount);
            continue;
        }

        cmdMsg = cmda78_dequeue_cmdMsg();
// mailbox_recv(cmdMsg);//
        code = mhu_recv_data(ch, (void *)cmdMsg, CMD_MSG_MAX_SIZE);//        ts_printf("%s:%s:%d %d\n", __FILE__, __func__, __LINE__, code);
        if (code <= 0) {
            cmda78_cancel_cmdMsg(cmdMsg);
            continue;
        }
        if (atomic_read(&rmgr->refcount) > 0)
            atomic_dec(&rmgr->refcount);

        cmda78_queue_cmdMsg(cmdMsg);
        atomic_inc(&mgr->refcount);
        wake_up_interruptible(&mgr->workwaitqueue);
    }

    ts_printf("recv thread exiting\n");
}

/**
 * @brief 初始化接收线程管理器状态
 * @param mgr 接收线程管理器结构体指针
 * @return void
 */
static void cmda78_recv_mgr_init(cmda78_mgr_t *mgr) {
    atomic_set(&mgr->refcount, 0);
    init_waitqueue_head(&mgr->workwaitqueue);
}

/**
 * @brief 创建单个接收线程（带回滚）
 * @param mgr 接收线程管理器结构体指针
 * @param rtb rtb结构体指针
 * @param threadIdx 线程索引（0或1）
 * @return int32_t 成功返回0，失败返回错误码
 * @note 失败时会自动清理已创建的线程
 */
static int32_t cmda78_create_recv_task(cmda78_mgr_t *mgr, void *rtb, uint32_t threadIdx) {
    TaskHandle_t xHandle = NULL;
    BaseType_t retCode;
    const char *threadName = (threadIdx == 0) ? "recv_thread0" : "recv_thread1";

    ts_printf("%s:%s:%d\n", __FILE__, __func__, __LINE__);
    retCode = xTaskCreate(cmda78_recv_thread_func, threadName, configMINIMAL_STACK_SIZE, rtb, tskIDLE_PRIORITY, &xHandle);
    configASSERT(xHandle);
    if (xHandle == NULL) {
        ts_printf("Failed to create recv thread %u\n", threadIdx);
        return -1;
    }
    mgr->recv_thread[threadIdx] = xHandle;
    return 0;
}

/**
 * @brief 创建工作线程（带回滚）
 * @param mgr 接收线程管理器结构体指针
 * @return int32_t 成功返回0，失败返回错误码
 * @note 失败时会自动清理已创建的接收线程
 */
static int32_t cmda78_create_work_task(cmda78_mgr_t *mgr) {
    TaskHandle_t xHandle = NULL;
    BaseType_t retCode;

    ts_printf("%s:%s:%d\n", __FILE__, __func__, __LINE__);
    retCode = xTaskCreate(cmda78_work_thread_proc, "work_thread", configMINIMAL_STACK_SIZE, mgr, tskIDLE_PRIORITY, &xHandle);
    configASSERT(xHandle);
    if (xHandle == NULL) {
        ts_printf("Failed to create work thread\n");
        vTaskDelete(mgr->recv_thread[1]);
        mgr->recv_thread[1] = NULL;
        vTaskDelete(mgr->recv_thread[0]);
        mgr->recv_thread[0] = NULL;
        return -1;
    }
    mgr->work_thread = xHandle;
    return 0;
}

int32_t  cmda78_thread_create(void* arg) {
    cmda78_mgr_t *mgr = (cmda78_mgr_t*) arg;
    int32_t ret;

    cmda78_recv_mgr_init(mgr);
    ret = cmda78_create_recv_task(mgr, &mgr->rtb[0], 0);
    if (ret != 0) {
        return -1;
    }

    ret = cmda78_create_recv_task(mgr, &mgr->rtb[1], 1);
    if (ret != 0) {
        return -2;
    }

    ret = cmda78_create_work_task(mgr);
    if (ret != 0) {
        return -3;
    }

    ts_printf("%s:%s:%d\n", __FILE__, __func__, __LINE__);
    return 0;
}

int32_t  cmda78_thread_stop(void* arg) {
    cmda78_mgr_t *mgr = (cmda78_mgr_t*) arg;

    if (mgr->work_thread) {
        // 请求停止并等待线程退出
        vTaskDelete(mgr->work_thread);
        mgr->work_thread = NULL;
    }

    if (mgr->recv_thread[1]) {
        // 请求停止并等待线程退出
        vTaskDelete(mgr->recv_thread[1]);
        mgr->recv_thread[1] = NULL;
    }

    if (mgr->recv_thread[0]) {
        // 请求停止并等待线程退出
        vTaskDelete(mgr->recv_thread[0]);
        mgr->recv_thread[0] = NULL;
    }

    if (mgr->wait_thread[1]) {
        // 请求停止并等待线程退出
        vTaskDelete(mgr->wait_thread[1]);
        mgr->wait_thread[1] = NULL;
    }

    if (mgr->wait_thread[0]) {
        // 请求停止并等待线程退出
        vTaskDelete(mgr->wait_thread[0]);
        mgr->wait_thread[0] = NULL;
    }

    return 0;
}