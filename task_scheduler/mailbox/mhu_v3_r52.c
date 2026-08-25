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
**                         *.c mhu v3 r52 source code                           **
*********************************************************************************/

#include "cmdef.h"
#include "system.h"
//#include "bqueue.h"
//#include "atomic_t.h"
//#include "wait_queue.h"
#include "mhu_v3_r52.h"
//#include <string.h>


int32_t mhu_v3_recv_data(uint32_t r52id, uint8_t *cmdMsg, uint32_t *cmdSize) {
    uint32_t ch = 2 * r52id;// 0 r52   0- channel a78 -> r52   1- channel r52 -> a78 ; 1 r52   2- channel a78 -> r52   3- channel r52 -> a78 
    uint32_t rvsz = mhu_recv_data(ch, cmdMsg, CMD_MSG_MAX_SIZE);
    *cmdSize = rvsz;
    if (rvsz <= 0) {
        ts_printf("failed to receive hw info, ret %u\n", rvsz);
        return -1;
    }
/*
    BaseType_t retCode = 0;
    mhu_t *mhu = mhu_get(r52id,  0);
    cmdMsg_t *cmdMsg = NULL;
    retCode = wait_event_interruptible(mhu->workwaitqueue, atomic_read(&mhu->refcount) > 0);
    if (retCode == pdFALSE) {
        return -1;
    }
    cmdMsg = (cmdMsg_t *)BQueueAcquire(mhu->cmd_queue);
    if (cmdMsg == NULL) {
        return -1;
    }
    memcpy(data, cmdMsg, cmdMsg->cmdSize);
    BQueueRelease(mhu->cmd_queue, (void*)cmdMsg);
    atomic_dec(&mhu->refcount);
    ts_printf("%s:%s:%d\n", __FILE__, __func__, __LINE__);*/
    return 0;
}


int32_t mhu_v3_send_data(uint32_t r52id, const uint8_t *cmdMsg, uint32_t cmdSize) {
    uint32_t ch = 2 * r52id + 1;// 0 r52   0- channel a78 -> r52   1- channel r52 -> a78 ; 1 r52   2- channel a78 -> r52   3- channel r52 -> a78 
    uint32_t snsz = mhu_send_data(ch, (void*)cmdMsg, cmdSize);
    if (snsz != cmdSize) {
        ts_printf("Failed to send create process cmd, ret %u\n", snsz);
        return -1;
    }

/*
    mhu_t *mhu = mhu_get(r52id, 1);
    BQueueQueue(mhu->cmd_queue, (void*)data);
    atomic_inc(&mhu->refcount);
    wake_up_interruptible(&mhu->workwaitqueue);
    ts_printf("%s:%s:%d\n", __FILE__, __func__, __LINE__);*/
    return 0;
}
