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
**                       include freeRTOS headers                               **
*********************************************************************************/

#ifndef _OSAL_FREE_RTOS_H_
#define _OSAL_FREE_RTOS_H_


#include "FreeRTOS.h"
#include "task.h"
#include "atomic.h"
#include "semphr.h"
#include "timers.h"
#include "spinlock.h"
#include "system.h"
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void *)0)
#endif
#endif


#define EBUSY       16
#define EINVAL      22  //EINVAL‌ 是 Linux 内核和用户空间中标准的错误码，代表 ‌“Invalid argument”（无效参数）‌。

#define ERESTARTSYS     512  // 有信号处理函数时重启，否则转为 EINTR
#define ERESTARTNOINTR  513  // 无论是否有信号，都强制重启（不转为 EINTR）
#define ERESTART_RESTARTBLOCK 514 // 需要修改参数后重启（如 nanosleep）
#define ERESTARTNOHAND  515  // 无信号处理函数时重启，否则转为 EINTR
#ifndef ETIMEDOUT
#define ETIMEDOUT 138
#endif

/**
 * \defgroup base_type Numeric Data Types
 *
 * \brief This section provides the definitions of numeric data types.
 *
 * @{
 */
/** \brief Signed 8-bit integer. */
typedef int8_t i8;
/** \brief Unsigned 8-bit integer. */
typedef uint8_t u8;
/** \brief Signed 16-bit integer. */
typedef int16_t i16;
/** \brief Unsigned 16-bit integer. */
typedef uint16_t u16;
/** \brief Signed 32-bit integer. */
typedef int32_t i32;
/** \brief Unsigned 32-bit integer. */
typedef uint32_t u32;
/** \brief Signed 64-bit integer. */
typedef int64_t i64;
/** \brief Unsigned 64-bit integer. */
typedef uint64_t u64;

/** \brief Buffer address. */
typedef u64 ptr_t;

/**@}*/

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define vmalloc  pvPortMalloc
#define vfree    vPortFree

#define __iomem

#define container_of(ptr, type, member) ({              \
    const typeof( ((type *)0)->member ) *__mptr = (ptr); \
    (type *)( (char *)__mptr - offsetof(type, member) ); })


#ifdef __cplusplus
extern "C" {
#endif
static inline uint32_t ioread32(const volatile uint32_t *addr)
{
    return *addr;
}

// 向 32 位寄存器写入值（ARM R52 专用）
static inline void iowrite32(uint32_t value, volatile uint32_t *addr)
{
    *addr = value;
}


#ifdef __cplusplus
}
#endif


#endif /* _OSAL_FREE_RTOS_H_ */