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
**                        *.c ddr memory allocator                              **
*********************************************************************************/

#include "ddr_mem.h"

#ifdef __FREERTOS__
#include "osal_freertos.h" /* needed for the spinlock stuff */
#endif

/* link.ld 定义的共享 DDR 缓冲段边界 */
extern uint8_t __ddr_buf_start[];
extern uint8_t __ddr_buf_end[];

#define DDR_MAX_BLOCKS 16 /* 同时存活的 DDR 块上限(BQueue 每实例 3 块,足够) */

static uint8_t *ddr_cur    = NULL;   /* 当前分配游标(8 字节对齐) */
static uint8_t *ddr_stack[DDR_MAX_BLOCKS]; /* 已分配块起点栈 */
static uint32_t ddr_nstack = 0;

void *ddr_alloc(uint32_t size) {
    uint8_t *p;
    uintptr_t addr;

    if (size == 0) {
        return NULL;
    }

    if (ddr_cur == NULL) {
        ddr_cur = __ddr_buf_start;
    }

    /* 对齐到 8 字节 */
    addr = ((uintptr_t)ddr_cur + 7u) & ~(uintptr_t)7u;
    p = (uint8_t *)addr;

    if ((p + size) > __ddr_buf_end) {
        return NULL; /* 段耗尽 */
    }
    if (ddr_nstack >= DDR_MAX_BLOCKS) {
        return NULL; /* 块数超限(防御性) */
    }

    ddr_stack[ddr_nstack++] = p;
    ddr_cur = p + size;
    return p;
}

void ddr_free(void *ptr) {
    int32_t i;

    if (ptr == NULL) {
        return;
    }
    /* bump 分配,只有按分配逆序(LIFO)释放才能回退游标。
     * 从栈顶往下找:若 ptr 是栈顶块,弹出并回退;
     * 若 ptr 在栈内但非栈顶,说明该块之后还有存活块,不可回退,
     * 且 bump 分配器无法复用中间空洞,只能忽略(保持分配状态)。 */
    for (i = (int32_t)ddr_nstack - 1; i >= 0; --i) {
        if (ddr_stack[i] != (uint8_t *)ptr) {
            continue;
        }
        if (i == (int32_t)ddr_nstack - 1) {
            ddr_cur = ddr_stack[i]; /* 回退到该块起点 */
        }
        /* 弹出该块(含非栈顶情况,后续 alloc 会重新分配并覆盖该槽) */
        ddr_stack[i] = ddr_stack[ddr_nstack - 1];
        --ddr_nstack;
        return;
    }
    /* 未找到:重复释放或非法指针,忽略 */
}

uint32_t ddr_avail(void) {
    uint8_t *base = (ddr_cur != NULL) ? ddr_cur : __ddr_buf_start;
    if (base >= __ddr_buf_end) {
        return 0;
    }
    return (uint32_t)(__ddr_buf_end - base);
}
