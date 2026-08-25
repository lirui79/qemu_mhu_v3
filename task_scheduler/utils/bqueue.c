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
**                         *.c buffer queue source code                         **
*********************************************************************************/

#include "cqueue.h"
#include "bqueue.h"
#include "ddr_mem.h"

#ifdef __FREERTOS__
#include "osal_freertos.h" /* needed for the _IOW etc stuff used later */
#endif

typedef struct {
    uint8_t*        data;
    CQueueHandle_t  queue;
    CQueueHandle_t  free;
    uint32_t        qSize;
    uint32_t        iSize;
} BQueue_t;


BQueueHandle_t BQueueCreate(uint32_t qSize, uint32_t iSize) {
    BQueue_t* queue = NULL;
    uint8_t*  item  = NULL;
    uint32_t  i     = 0;
    if (qSize < 1 || iSize < 1) {
        return NULL;
    }

    queue = (BQueue_t*)vmalloc(sizeof(BQueue_t));
    if (queue == NULL) {
        return NULL;
    }

    queue->qSize = qSize;
    queue->iSize = iSize;
    /* 大数据区放共享 DDR,避免占用本地 RAM FreeRTOS 堆
     * (BQueue 结构体/TCB 等小对象仍走 vmalloc) */
    queue->data  = (uint8_t*)ddr_alloc(qSize * iSize);
    if (queue->data == NULL) {
        vfree(queue);
        return NULL;
    }

    queue->free  = CQueueCreate(qSize,  sizeof(uint8_t*));
    if (queue->free == NULL) {
        vfree(queue);
        return NULL;
    }
    queue->queue  = CQueueCreate(qSize, sizeof(uint8_t*));
    if (queue->queue == NULL) {
        CQueueDelete(queue->free);
        vfree(queue);
        return NULL;
    }

    for (i = 0; i < qSize; ++i) {
        item = queue->data + (i * iSize);
        CQueueEnqueue(queue->free, &item);
    }

    return queue;
}

void BQueueDelete(BQueueHandle_t handle) {
    BQueue_t* queue = (BQueue_t*)handle;
    if (queue == NULL) {
        return;
    }
    CQueueDelete(queue->queue);
    CQueueDelete(queue->free);
    if (queue->data != NULL) {
        ddr_free(queue->data);
    }
    vfree(queue);
}

uint32_t  BQueueSize(BQueueHandle_t handle) {
    BQueue_t* queue = (BQueue_t*)handle;
    if (queue == NULL) {
        return 0;
    }
    return queue->qSize;
}

uint32_t  BQueueLength(BQueueHandle_t handle) {
    BQueue_t* queue = (BQueue_t*)handle;
    if (queue == NULL) {
        return 0;
    }
    return CQueueLength(queue->queue);
}


uint32_t  BQueueItemSize(BQueueHandle_t handle) {
    BQueue_t* queue = (BQueue_t*)handle;
    if (queue == NULL) {
        return 0;
    }
    return queue->iSize;
}


void*          BQueueAcquire(BQueueHandle_t handle) {
    BQueue_t* queue = (BQueue_t*)handle;
    void **slot = NULL;
    if (queue == NULL) {
        return NULL;
    }
    /* CQueue 是"值拷贝"语义:Enqueue 时传入的 &item 被拷贝进队列,
     * Dequeue 返回存储槽位地址,需解引用才能拿到真正的 item 指针。 */
    slot = (void**)CQueueDequeue(queue->queue);
    return slot ? *slot : NULL;
}

int32_t        BQueueRelease(BQueueHandle_t handle, void* item) {
    BQueue_t* queue = (BQueue_t*)handle;
    if (queue == NULL) {
        return -1;
    }
    /* 传 &item,让 CQueue 拷贝 item 的地址值(而不是 item 指向的内容) */
    return CQueueEnqueue(queue->free, &item);
}

void*          BQueueDequeue(BQueueHandle_t handle) {
    BQueue_t* queue = (BQueue_t*)handle;
    void **slot = NULL;
    if (queue == NULL) {
        return NULL;
    }
    slot = (void**)CQueueDequeue(queue->free);
    return slot ? *slot : NULL;
}

int32_t        BQueueQueue(BQueueHandle_t handle, void* item) {
    BQueue_t* queue = (BQueue_t*)handle;
    if (queue == NULL) {
        return -1;
    }
    return CQueueEnqueue(queue->queue, &item);
}

int32_t        BQueueCancel(BQueueHandle_t handle, void* item) {
    BQueue_t* queue = (BQueue_t*)handle;
    if (queue == NULL) {
        return -1;
    }
    return CQueueEnqueue(queue->free, &item);
}

void*          BQueueAcquireFromISR(BQueueHandle_t handle) {
    BQueue_t* queue = (BQueue_t*)handle;
    void **slot = NULL;
    if (queue == NULL) {
        return NULL;
    }
    slot = (void**)CQueueDequeueFromISR(queue->queue);
    return slot ? *slot : NULL;
}

int32_t        BQueueReleaseFromISR(BQueueHandle_t handle, void* item) {
    BQueue_t* queue = (BQueue_t*)handle;
    if (queue == NULL) {
        return -1;
    }
    return CQueueEnqueueFromISR(queue->free, &item);
}

void*          BQueueDequeueFromISR(BQueueHandle_t handle) {
    BQueue_t* queue = (BQueue_t*)handle;
    void **slot = NULL;
    if (queue == NULL) {
        return NULL;
    }
    slot = (void**)CQueueDequeueFromISR(queue->free);
    return slot ? *slot : NULL;
}

int32_t        BQueueQueueFromISR(BQueueHandle_t handle, void* item) {
    BQueue_t* queue = (BQueue_t*)handle;
    if (queue == NULL) {
        return -1;
    }
    return CQueueEnqueueFromISR(queue->queue, &item);
}

int32_t        BQueueCancelFromISR(BQueueHandle_t handle, void* item) {
    BQueue_t* queue = (BQueue_t*)handle;
    if (queue == NULL) {
        return -1;
    }
    return CQueueEnqueueFromISR(queue->free, &item);
}
