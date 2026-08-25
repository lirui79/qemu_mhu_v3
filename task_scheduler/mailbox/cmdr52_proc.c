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
**                        *.c cmdr52 proc source code                           **
*********************************************************************************/


#include "crc32.h"
//#include "vcodec.h"
#include "system.h" /* g_mbx_fc_stat_0 / mhu_poll_rx / mhu_send_fast_event / MHU_FC0_ACK_VALUE */
#include "cmdr52_mgr.h"
#include "mhu_v3_r52.h"
#include "cmdr52_proc.h"

/* R52 每核堆仅 18KB,5 个 vcodec 线程不能用共享的 configMINIMAL_STACK_SIZE(4KB),
 * 统一用 256 words(1KB)专用栈(recv/work/wait 调用链浅,ts_printf 缓冲仅 33B,
 * 1KB 足够;work 线程的 crc32_calc 仅 128B 缓冲)。 */
#define CMDR52_THREAD_STACK_SIZE                        (512)



int32_t cmdr52_send(cmdMsg_t *cmdMsg) {
    int32_t retCode = 0;
    uint32_t r52ID = ((cmdMsg->sessionID & 0xFFFF0000) >> 16);

    ts_printf("%s:%s:%d r52ID:%d started\n", __FILE__, __func__, __LINE__, r52ID);
    cmdMsg->crc32 = crc32_calc((const uint8_t *)cmdMsg, cmdMsg->cmdSize);
// mailbox_send(cmdMsg);
    retCode = mhu_v3_send_data(r52ID, (const uint8_t *)cmdMsg, cmdMsg->cmdSize);
    cmdr52_mgr_release_cmdMsg(cmdMsg);
    return retCode;
}


int32_t    cmdr52_thread_wakeup(uint32_t r52CoreID) {
    core52_mgr_t *core52_mgr = &cmdr52_mgr_get()->ctb[r52CoreID];
    atomic_inc(&core52_mgr->refcount);
    wake_up_interruptible(&core52_mgr->workwaitqueue);
    return 0;
}

int32_t     cmdr52_thread_wakeup_from_isr(uint32_t r52CoreID, BaseType_t *pxHigherPriorityTaskWoken) {
    core52_mgr_t *core52_mgr = &cmdr52_mgr_get()->ctb[r52CoreID];
    atomic_inc_from_isr(&core52_mgr->refcount);
    wake_up_interruptible_from_isr(&core52_mgr->workwaitqueue, pxHigherPriorityTaskWoken);
    return 0;
}


/**
 * @brief 初始化接收线程管理器状态
 * @param mgr 接收线程管理器结构体指针
 * @return void
 */
static void cmdr52_recv_mgr_init(cmdr52_mgr_t *mgr) {
    atomic_set(&mgr->refcount, 0);
    init_waitqueue_head(&mgr->workwaitqueue);
}

static void cmdr52_recv_thread_func(void *arg) {
    core52_mgr_t *core52_mgr = (core52_mgr_t *)arg;
    cmdr52_mgr_t *mgr = (cmdr52_mgr_t*) cmdr52_mgr_get();
    cmdMsg_t *cmdMsg = NULL;
    BaseType_t retCode = 0;
    int32_t code = 0;
    /* A76→R52 命令走 fast channel 2*r52coreID(core0→0, core1→2) */
    uint32_t ch = 2 * core52_mgr->r52coreID;

    ts_printf("recv thread started\n");

    while (1) {
        /* 平台 MBX 组合中断投递不可靠(实测有时 ISR 不触发),若只依赖
         * irq_callback_fifo 经 ISR 唤醒本线程,A76 的命令会一直锁存在
         * 硬件里无人处理,导致本线程永久睡眠、A76 等响应死锁。显式轮询
         * 硬件并入软件标志(内部经 irq_callback_fifo 置 refcount 并唤醒),
         * 兜住无中断路径。 */
        //mhu_poll_rx();
        /* 用带超时的等待:即便 poll 后仍无就绪(命令尚未到达),也能周期性
         * 回到循环继续轮询。就绪判定 = 中断标志(refcount)或 FIFO 已有
         * payload(fill>0,数据驱动兜底)——即使模型中断边沿丢失,FIFO 里的
         * 数据也能被及时发现并读出,不会永久锁死。 */
        retCode = wait_event_interruptible_timeout(core52_mgr->workwaitqueue,
                                                   (atomic_read(&core52_mgr->refcount) > 0)
                                                   || (mhu_rx_data_fill(ch) > 0),
                                                   pdMS_TO_TICKS(500));

        if (retCode == pdFALSE) {
            continue;   /* 超时未就绪,回到循环顶部继续轮询 */
        }

        if (mhu_rx_data_fill(ch) == 0) {
            if (atomic_read(&core52_mgr->refcount) > 0)
                atomic_dec(&core52_mgr->refcount);
            continue;
        }

        cmdMsg = cmdr52_mgr_dequeue_cmdMsg();
        /* 空队列时 dequeue 返回 NULL:直接跳过,避免把 NULL 当 cmdMsg
         * 传给 mhu(空壳)/BQueueQueue 导致 memcpy(NULL) 崩溃。
         * mhu_v3_recv_data 当前为空壳,不会产生数据,空转等待即可。 */
        if (cmdMsg == NULL) {
            vTaskDelay(100);
            continue;
        }


// mailbox_recv(cmdMsg);
        code = mhu_v3_recv_data(core52_mgr->r52coreID, (uint8_t *)cmdMsg, &cmdMsg->cmdSize);
        ts_printf("%s:%s:%d %d\n", __FILE__, __func__, __LINE__, code);
        if (code != 0) {
            cmdr52_mgr_cancel_cmdMsg(cmdMsg);
            continue;
        }
        if (atomic_read(&core52_mgr->refcount) > 0)
            atomic_dec(&core52_mgr->refcount);

        cmdr52_mgr_queue_cmdMsg(cmdMsg);
        atomic_inc(&mgr->refcount);
        wake_up_interruptible(&mgr->workwaitqueue);
        ts_printf("%s:%s:%d %d\n", __FILE__, __func__, __LINE__, code);
    }

    ts_printf("recv thread exiting\n");
}

/**
 * @brief 创建单个接收线程（带回滚）
 * @param mgr 接收线程管理器结构体指针
 * @param rtb rtb结构体指针
 * @param threadIdx 线程索引（0或1）
 * @return int32_t 成功返回0，失败返回错误码
 * @note 失败时会自动清理已创建的线程
 */
static int32_t cmdr52_create_recv_task(cmdr52_mgr_t *mgr, uint32_t r52coreID) {
    core52_mgr_t *core52_mgr = &mgr->ctb[r52coreID];
    TaskHandle_t xHandle = NULL;
    const char *threadName = (r52coreID == 0) ? "recv_thread0" : "recv_thread1";
    BaseType_t retCode;
    ts_printf("%s:%s:%d\n", __FILE__, __func__, __LINE__);
    retCode = xTaskCreate(cmdr52_recv_thread_func, threadName, CMDR52_THREAD_STACK_SIZE, core52_mgr, tskIDLE_PRIORITY, &xHandle);
    configASSERT(xHandle);
    if (xHandle == NULL) {
        ts_printf("Failed to create recv thread\n");
        vTaskDelete(mgr->wait_thread[1]);
        mgr->wait_thread[1] = NULL;
        vTaskDelete(mgr->wait_thread[0]);
        mgr->wait_thread[0] = NULL;
        return -1;
    }
    mgr->recv_thread[r52coreID] = xHandle;
    return 0;
}

static void cmdr52_work_thread_proc(void *arg) {
    cmdr52_mgr_t *mgr = (cmdr52_mgr_t*) arg;
    cmdMsg_t *cmdMsg = NULL;
    int32_t code = 0;
    BaseType_t retCode = 0;

    ts_printf("work thread started\n");
    while (1) {
        retCode = wait_event_interruptible(mgr->workwaitqueue, atomic_read(&mgr->refcount) > 0);
        ts_printf("%s:%s:%d %d\n", __FILE__, __func__, __LINE__, retCode);
        if (retCode == pdFALSE) {
            ts_printf("cmd work: %s: signaled!!!\n", __func__);
            break;
        }

        cmdMsg = cmdr52_mgr_acquire_cmdMsg();
        if (cmdMsg == NULL) {
            continue;
        }

        ts_printf("QUEUE:ptr=%08x magic=%x ver=%d type=%x size=%u sid=%x seq=%x crc=%x\n", \
            (uint32_t)(uintptr_t)cmdMsg, cmdMsg->magic, cmdMsg->version, cmdMsg->cmdType, \
            cmdMsg->cmdSize, cmdMsg->sessionID, cmdMsg->seqNum, cmdMsg->crc32);
        code = cmdr52_mgr_proc_cmdMsg(cmdMsg);
        ts_printf("%s:%s:%d %d\n", __FILE__, __func__, __LINE__, code);
        cmdr52_mgr_release_cmdMsg(cmdMsg);
        atomic_dec(&mgr->refcount);
    }
    ts_printf("Worker thread exiting\n");
}

/**
 * @brief 创建工作线程（带回滚）
 * @param mgr 接收线程管理器结构体指针
 * @return int32_t 成功返回0，失败返回错误码
 * @note 失败时会自动清理已创建的接收线程
 */
static int32_t cmdr52_create_work_task(cmdr52_mgr_t *mgr) {
    TaskHandle_t xHandle = NULL;
    BaseType_t retCode;
    ts_printf("%s:%s:%d\n", __FILE__, __func__, __LINE__);
    retCode = xTaskCreate(cmdr52_work_thread_proc, "work_thread", CMDR52_THREAD_STACK_SIZE, mgr, tskIDLE_PRIORITY, &xHandle);
    configASSERT(xHandle);
    if (xHandle == NULL) {
        ts_printf("Failed to create work thread\n");
        vTaskDelete(mgr->recv_thread[1]);
        mgr->recv_thread[1] = NULL;
        vTaskDelete(mgr->recv_thread[0]);
        mgr->recv_thread[0] = NULL;
        vTaskDelete(mgr->wait_thread[1]);
        mgr->wait_thread[1] = NULL;
        vTaskDelete(mgr->wait_thread[0]);
        mgr->wait_thread[0] = NULL;
        return -1;
    }
    mgr->work_thread = xHandle;
    return 0;
}

static void cmdr52_wait_thread_func(void *arg) {
    cmdr52_mgr_t *mgr = (cmdr52_mgr_t*) cmdr52_mgr_get();
    vcmd_mgr_t *vcmd_mgr = ((vcmd_mgr_t *)arg);
    int32_t code = 0;

    ts_printf("%s:%s:%d  %u started\n", __FILE__, __func__, __LINE__, vcmd_mgr->vcmd_mgr_id);
    while (1) {

        //cmdr52_wait
        code = vcmd_wait_cmdbuf(vcmd_mgr);

        ts_printf("%s:%s:%d %d\n", __FILE__, __func__, __LINE__, code);
        if (code != 0) {
            continue;
        }
        //atomic_inc(&mgr->refcount);
        //wake_up_interruptible(&mgr->workwaitqueue);
    }

    ts_printf("%s:%s:%d  %u exiting\n", __FILE__, __func__, __LINE__, vcmd_mgr->vcmd_mgr_id);
}


static int32_t cmdr52_create_wait_task(cmdr52_mgr_t *mgr, uint32_t mgrid) {
    TaskHandle_t xHandle = NULL;
    BaseType_t retCode;
    vcmd_mgr_t *vcmd_mgr = cmdr52_get_vcmd_mgr(mgrid);
    const char *threadName = (mgrid == 0) ? "wait_thread0" : "wait_thread1";
    ts_printf("%s:%s:%d %d\n", __FILE__, __func__, __LINE__, mgrid);
    retCode = xTaskCreate(cmdr52_wait_thread_func, threadName, CMDR52_THREAD_STACK_SIZE, vcmd_mgr, tskIDLE_PRIORITY, &xHandle);
    configASSERT(xHandle);
    if (xHandle == NULL) {
        ts_printf("Failed to create wait thread\n");
        return -1;
    }
    mgr->wait_thread[mgrid] = xHandle;

    return 0;
}

int32_t  cmdr52_thread_create(void* arg) {
    cmdr52_mgr_t *mgr = (cmdr52_mgr_t*) arg;
    int32_t ret;

    cmdr52_recv_mgr_init(mgr);

    ret = cmdr52_create_wait_task(mgr, VCMD_MGR_ID_ENC);
    if (ret != 0) {
        return -1;
    }

    ret = cmdr52_create_wait_task(mgr, VCMD_MGR_ID_DEC);
    if (ret != 0) {
        return -2;
    }

    // 创建并启动内核线程，将 dev 作为参数传入
    ret = cmdr52_create_recv_task(mgr, 0);
    if (ret != 0) {
        return -3;
    }

    // 创建并启动内核线程，将 dev 作为参数传入
    ret = cmdr52_create_recv_task(mgr, 1);
    if (ret != 0) {
        return -3;
    }

    ret = cmdr52_create_work_task(mgr);
    if (ret != 0) {
        return -4;
    }

    ts_printf("%s:%s:%d\n", __FILE__, __func__, __LINE__);
    return 0;
}

int32_t  cmdr52_thread_stop(void* arg) {
    cmdr52_mgr_t *mgr = (cmdr52_mgr_t*) arg;

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

    if (mgr->work_thread) {
        // 请求停止并等待线程退出
        vTaskDelete(mgr->work_thread);
        mgr->work_thread = NULL;
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
