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
**                            include  atomic_t  header                         **
*********************************************************************************/

#ifndef _ATOMIC_T_H_
#define _ATOMIC_T_H_

#ifdef __FREERTOS__
#include "osal_freertos.h" /* needed for the _IOW etc stuff used later */
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    volatile uint32_t val;
} atomic_t;

static inline void atomic_set(atomic_t *a, uint32_t v)
{
    taskENTER_CRITICAL();
    a->val = v;
    taskEXIT_CRITICAL();
}

static inline uint32_t atomic_read(atomic_t *a)
{
    uint32_t ret;
    taskENTER_CRITICAL();
    ret = a->val;
    taskEXIT_CRITICAL();
    return ret;
}

static inline void atomic_inc(atomic_t *a)
{
    taskENTER_CRITICAL();
    a->val++;
    taskEXIT_CRITICAL();
}

/* ISR 安全版本:taskENTER_CRITICAL_FROM_ISR() 不检查
 * ullPortInterruptNesting,可在中断上下文使用。
 * (taskENTER_CRITICAL() 在 ISR 中会触发 vPortEnterCritical 的
 *  configASSERT(ullPortInterruptNesting==0),即 port.c:374/0x176) */
static inline void atomic_inc_from_isr(atomic_t *a)
{
    UBaseType_t uxSaved = taskENTER_CRITICAL_FROM_ISR();
    a->val++;
    taskEXIT_CRITICAL_FROM_ISR(uxSaved);
}

static inline void atomic_dec(atomic_t *a)
{
    taskENTER_CRITICAL();
    a->val--;
    taskEXIT_CRITICAL();
}

// 先取值再自增
static inline uint32_t atomic_fetch_inc(atomic_t *a)
{
    uint32_t ret = 0;
    taskENTER_CRITICAL();
    ret = a->val++;
    taskEXIT_CRITICAL();
    return ret;
}

// 先取值再自增
static inline uint32_t atomic_fetch_dec(atomic_t *a)
{
    uint32_t ret = 0;
    taskENTER_CRITICAL();
    ret = a->val--;
    taskEXIT_CRITICAL();
    return ret;
}

// 先取值再自增
static inline uint32_t atomic_fetch_add(atomic_t *a, uint32_t v)
{
    uint32_t ret = 0;
    taskENTER_CRITICAL();
    ret = a->val;
    a->val += v;
    taskEXIT_CRITICAL();
    return ret;
}

// 先取值再自增
static inline uint32_t atomic_fetch_sub(atomic_t *a, uint32_t v)
{
    uint32_t ret = 0;
    taskENTER_CRITICAL();
    ret = a->val;
    a->val -= v;
    taskEXIT_CRITICAL();
    return ret;
}



#ifdef __cplusplus
}
#endif

#endif //_ATOMIC_T_H_