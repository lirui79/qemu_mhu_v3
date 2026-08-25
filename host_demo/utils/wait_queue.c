#include "wait_queue.h"

void init_waitqueue_head(wait_queue_head_t *wq) {
     if (wq == NULL) return;
     vListInitialise(&wq->xList);
     wq->xMutex = xSemaphoreCreateMutex();
}

void destroy_waitqueue_head(wait_queue_head_t *wq) {
    if (wq == NULL) return;
    if (wq->xMutex != NULL) {
        vSemaphoreDelete(wq->xMutex);
        wq->xMutex = NULL;
    }
}

// 普通任务上下文：唤醒队列上的第一个任务
BaseType_t wake_up_interruptible(wait_queue_head_t *wq) {
    BaseType_t xWoken = pdFALSE;
    if (wq == NULL || wq->xMutex == NULL) {
        return pdFALSE;
    }

    xSemaphoreTake(wq->xMutex, portMAX_DELAY);
    if (listLIST_IS_EMPTY(&wq->xList) == pdFALSE) {
        ListItem_t *pxItem = listGET_HEAD_ENTRY(&wq->xList);
        WaitQueueNode_t *pxNode = NULL;
        uxListRemove(pxItem);
        pxNode = (WaitQueueNode_t *)listGET_LIST_ITEM_OWNER(pxItem);
        if (pxNode && pxNode->xTaskToWake) {
            xTaskNotifyGive(pxNode->xTaskToWake);
        }
    }
    xSemaphoreGive(wq->xMutex);
    return pdTRUE;
}

// 普通任务上下文：唤醒队列上的所有任务
BaseType_t wake_up_interruptible_all(wait_queue_head_t *wq) {
    if (wq == NULL || wq->xMutex == NULL) {
        return pdFALSE;
    }
    xSemaphoreTake(wq->xMutex, portMAX_DELAY);
    while (listLIST_IS_EMPTY(&wq->xList) == pdFALSE) {
        ListItem_t *pxItem = listGET_HEAD_ENTRY(&wq->xList);
        WaitQueueNode_t *pxNode = NULL;
        uxListRemove(pxItem);
        pxNode = (WaitQueueNode_t *)listGET_LIST_ITEM_OWNER(pxItem);
        if (pxNode && pxNode->xTaskToWake) {
            xTaskNotifyGive(pxNode->xTaskToWake);
        }
    }
    xSemaphoreGive(wq->xMutex);
    return pdTRUE;
}

// ================= 中断上下文唤醒版本 (ISR-safe) =================

// 中断上下文：唤醒队列上的第一个任务
BaseType_t wake_up_interruptible_from_isr(wait_queue_head_t *wq, BaseType_t *pxHigherPriorityTaskWoken) {
    BaseType_t flags = 0;
    if (wq == NULL) {
        return pdFALSE;
    }
    flags = taskENTER_CRITICAL_FROM_ISR();
    if (listLIST_IS_EMPTY(&wq->xList) == pdFALSE) {
        ListItem_t *pxItem = listGET_HEAD_ENTRY(&wq->xList);
        uxListRemove(pxItem);
        WaitQueueNode_t *pxNode = (WaitQueueNode_t *)listGET_LIST_ITEM_OWNER(pxItem);
        if (pxNode && pxNode->xTaskToWake) {
            vTaskNotifyGiveFromISR(pxNode->xTaskToWake, pxHigherPriorityTaskWoken);
        }
    }
    taskEXIT_CRITICAL_FROM_ISR(flags);
    return pdTRUE;
}

// 中断上下文：唤醒队列上的所有任务
BaseType_t wake_up_interruptible_all_from_isr(wait_queue_head_t *wq, BaseType_t *pxHigherPriorityTaskWoken) {
    BaseType_t flags = 0;
    if (wq == NULL) {
        return pdFALSE;
    }
    flags = taskENTER_CRITICAL_FROM_ISR();
    while (listLIST_IS_EMPTY(&wq->xList) == pdFALSE) {
        ListItem_t *pxItem = listGET_HEAD_ENTRY(&wq->xList);
        uxListRemove(pxItem);
        WaitQueueNode_t *pxNode = (WaitQueueNode_t *)listGET_LIST_ITEM_OWNER(pxItem);
        if (pxNode && pxNode->xTaskToWake) {
            vTaskNotifyGiveFromISR(pxNode->xTaskToWake, pxHigherPriorityTaskWoken);
        }
    }
    taskEXIT_CRITICAL_FROM_ISR(flags);
    return pdTRUE;
}