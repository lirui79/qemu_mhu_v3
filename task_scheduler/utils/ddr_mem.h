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
**                        *.h ddr memory allocator                              **
*********************************************************************************/

#ifndef __DDR_MEM_H__
#define __DDR_MEM_H__

#include <stdint.h>

/* 从共享 DDR 缓冲段(.ddr_buf,link.ld __ddr_buf_start/__ddr_buf_end)分配。
 * 用于 BQueue/CQueue 的大数据区,减轻本地 RAM 堆压力。
 * ddr_alloc 为简单 bump 分配(不做合并),ddr_free 仅支持按分配逆序释放,
 * 与 BQueueCreate/BQueueDelete 的创建/销毁顺序一致。 */
void *ddr_alloc(uint32_t size);
void  ddr_free(void *ptr);

/* 调试:返回剩余可用字节数 */
uint32_t ddr_avail(void);

#endif /* __DDR_MEM_H__ */
