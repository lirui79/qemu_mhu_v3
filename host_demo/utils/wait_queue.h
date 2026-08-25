#ifndef __WAIT_QUEUE_H
#define __WAIT_QUEUE_H


#ifdef __FREERTOS__
#include "osal_freertos.h" /* needed for the _IOW etc stuff used later */
#endif

#include "task.h"
#include "list.h"
#include "semphr.h"


#ifdef __cplusplus
extern "C" {
#endif



// 等待队列节点（显式分配，不依赖 TCB 内部节点）
typedef struct WaitQueueNode {
    ListItem_t xListItem;        // FreeRTOS 链表节点
    TaskHandle_t xTaskToWake;    // 记录需要唤醒的任务句柄
} WaitQueueNode_t;

// 对标 Linux 内核的 wait_queue_head_t
typedef struct {
    List_t xList;                // 内部双向链表，用于挂载等待的任务节点
    SemaphoreHandle_t xMutex;    // 互斥锁，保护链表的并发访问
} wait_queue_head_t;

// 初始化与销毁
void init_waitqueue_head(wait_queue_head_t *wq);
void destroy_waitqueue_head(wait_queue_head_t *wq);


// 核心内部宏：实现 Linux 的 "先判后睡、先醒后检" 机制
#define __wait_event_interruptible(wq, condition, timeticks, ret) \
    do { \
        TickType_t xStartTick = 0; \
        TickType_t xRemaining = timeticks; \
        WaitQueueNode_t *pxNode = NULL; \
        /* 1. 如果条件一开始就满足，直接退出，不进入睡眠 */ \
        if (condition) { \
            ret = pdTRUE; \
            break; \
        } \
        \
        xStartTick = xTaskGetTickCount(); \
        /* 2. 动态分配节点并加入等待队列 */ \
        pxNode = (WaitQueueNode_t *)pvPortMalloc(sizeof(WaitQueueNode_t)); \
        if (pxNode == NULL) { ret = pdFALSE; break; } /* 内存分配失败 */ \
        \
        vListInitialiseItem(&pxNode->xListItem); \
        listSET_LIST_ITEM_OWNER(&pxNode->xListItem, pxNode); \
        pxNode->xTaskToWake = xTaskGetCurrentTaskHandle(); \
        \
        xSemaphoreTake((wq).xMutex, portMAX_DELAY); \
        vTaskSuspendAll(); \
        { \
            vListInsertEnd(&(wq).xList, &pxNode->xListItem); \
        } \
        xTaskResumeAll(); \
        xSemaphoreGive((wq).xMutex); \
        \
        for (;;) { \
            /* 3. 再次检查条件（防止虚假唤醒） */ \
            if (condition) { \
                ret = pdTRUE; \
                break; \
            } \
            \
            /* 4. 进入 Blocked 状态，释放 CPU */ \
            ulTaskNotifyTake(pdTRUE, xRemaining); \
            \
            /* 5. 被唤醒后，计算剩余时间并重新检查条件 */ \
            if (timeticks != portMAX_DELAY) { \
                TickType_t xElapsed = xTaskGetTickCount() - xStartTick; \
                if (xElapsed >= timeticks) { \
                    if (condition) {\
                        ret = pdTRUE; \
                    } else { \
                        ret = pdFALSE; /* 超时返回 0 */ \
                    } \
                    break; \
                } \
                xRemaining = timeticks - xElapsed; \
            } \
        } \
        \
        /* 6. 退出循环时，确保节点已从链表中移除并释放内存 */ \
        if (pxNode != NULL) { \
            xSemaphoreTake((wq).xMutex, portMAX_DELAY); \
            if (listLIST_ITEM_CONTAINER(&pxNode->xListItem) != NULL) { \
                uxListRemove(&pxNode->xListItem); \
            } \
            xSemaphoreGive((wq).xMutex); \
            vPortFree(pxNode); \
        } \
    } while (0)


#define wait_event_interruptible(wq, condition)  \
    ({ \
        BaseType_t _ret = pdTRUE;\
        TickType_t _timeout = portMAX_DELAY;\
        if (!(condition)) \
            __wait_event_interruptible(wq, condition, _timeout, _ret); \
        _ret; \
    })


#define wait_event_interruptible_timeout(wq, condition, timeout) \
    ({ \
        BaseType_t _ret = pdTRUE;\
        if (!(condition)) \
            __wait_event_interruptible(wq, condition, timeout, _ret); \
        _ret; \
    })



// 普通任务上下文唤醒
BaseType_t wake_up_interruptible(wait_queue_head_t *wq);
BaseType_t wake_up_interruptible_all(wait_queue_head_t *wq);

// 中断上下文唤醒（ISR-safe）
BaseType_t wake_up_interruptible_from_isr(wait_queue_head_t *wq, BaseType_t *pxHigherPriorityTaskWoken);
BaseType_t wake_up_interruptible_all_from_isr(wait_queue_head_t *wq, BaseType_t *pxHigherPriorityTaskWoken);

#ifdef __cplusplus
}
#endif

#endif