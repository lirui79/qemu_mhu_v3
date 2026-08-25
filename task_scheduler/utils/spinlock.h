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
**                       include spinlock_t headers                             **
*********************************************************************************/

#ifndef _SPIN_LOCK_T_H_
#define _SPIN_LOCK_T_H_

#include "FreeRTOS.h"
#include "task.h"

#ifdef __cplusplus
extern "C" {
#endif




/* mutex */
typedef   UBaseType_t  spinlock_t;
#define   spin_lock_init(x) do {*(x) = pdFALSE;} while(0)
#define   spin_lock(x)  do { *(x) = 0; taskENTER_CRITICAL();} while(0)
#define   spin_unlock(x)  do { *(x) = 0; taskEXIT_CRITICAL();} while(0)
#define   spin_lock_irqsave(x, flag)   do { *(x) = 0; flag = taskENTER_CRITICAL_FROM_ISR();} while(0)
#define   spin_unlock_irqrestore(x, flag) do { *(x) = 0; taskEXIT_CRITICAL_FROM_ISR(flag);} while(0)

/*
typedef unsigned int spinlock_t;
#define   spin_lock_init(x) (*(x) = 0)
#define   spin_lock(x)  do { *(x) = 0 ;} while(0)
#define   spin_unlock(x)  do { *(x) = 0 ;} while(0)
#define   spin_lock_irqsave(x, flag)  do { flag ;} while(0)
#define   spin_unlock_irqrestore(x, flag) do { flag ;} while(0)
*/

#ifdef __cplusplus
}
#endif


#endif /* _SPIN_LOCK_T_H_ */