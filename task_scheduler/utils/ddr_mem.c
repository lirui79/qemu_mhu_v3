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

#include "osal.h" /* needed for the spinlock stuff */

/* link.ld 定义的共享 DDR 缓冲段边界 */
extern uint8_t __ddr_buf_start[];
extern uint8_t __ddr_buf_end[];


#define DDR_ALIGNMENT           8U
#define DDR_ALIGN_MASK          (DDR_ALIGNMENT - 1U)
/* DDR尾部元数据保留区域，业务内存禁止使用 */
#define META_FREE_OFFSET        0x8000U
#define META_ALLOC_OFFSET       0x4000U
#define META_RESERVE_TOTAL      (META_FREE_OFFSET + META_ALLOC_OFFSET)
#define DDR_BLOCK_MAX           0x3FFU
#define DDR_MAGIC               0xA55AA55AU

/* 空闲块：存R52片上RAM */
typedef struct {
    uint8_t*    base;
    uint32_t    size;
    uint32_t    valid;
    uint32_t    magic;
} ddr_block_t;

typedef struct {
    ddr_block_t*          block;
    uint32_t              block_cnt;
    uint32_t              block_max;
    uint32_t              magic;
} ddr_array_t;

/* 已分配块：记录alloc出去的内存段，free时获取size */


static ddr_array_t*          ddr_array_free  = NULL;
static ddr_array_t*          ddr_array_alloc = NULL;


static uint32_t ddr_align_size(uint32_t sz)
{
    return (sz + DDR_ALIGN_MASK) & ~DDR_ALIGN_MASK;
}

static void ddr_init(void) {
    uint8_t *ddr_ptr;
    ddr_block_t *block = NULL;
    uint32_t i = 0 ;
    ddr_ptr = (uint8_t *)(__ddr_buf_end - META_FREE_OFFSET);
    ddr_array_free = (ddr_array_t*) ddr_ptr;
    ddr_array_free->magic = DDR_MAGIC;
    ddr_array_free->block_cnt = 1;
    ddr_array_free->block_max = DDR_BLOCK_MAX;
    ddr_array_free->block = (ddr_block_t*) (ddr_ptr + sizeof(ddr_array_t));

    block = ddr_array_free->block;
    /* 业务可用内存：总DDR大小 减去尾部全部元数据保留空间 */
    block[0].base  = __ddr_buf_start;
    block[0].size  = (uint32_t)(__ddr_buf_end - __ddr_buf_start) - META_RESERVE_TOTAL;
    block[0].valid = 1U;
    block[0].magic = DDR_MAGIC;

    for(i = 1; i < ddr_array_free->block_max; ++i) {
        block[i].base  = NULL;
        block[i].size  = 0U;
        block[i].valid = 0U;
        block[i].magic = 0U;
    }

    /* alloc list 放在 __ddr_buf_end -0x4000 */
    ddr_ptr = (uint8_t *)(__ddr_buf_end - META_ALLOC_OFFSET);
    ddr_array_alloc = (ddr_array_t*) ddr_ptr;
    ddr_array_alloc->magic = DDR_MAGIC;
    ddr_array_alloc->block_cnt = 0;
    ddr_array_alloc->block_max = DDR_BLOCK_MAX;
    ddr_array_alloc->block     = (ddr_block_t*) (ddr_ptr + sizeof(ddr_array_t));

    block = ddr_array_alloc->block;
    for(i = 0; i < ddr_array_alloc->block_max; ++i) {
        block[i].base  = NULL;
        block[i].size  = 0U;
        block[i].valid = 0U;
        block[i].magic = 0U;
    }
}

static void ddr_trace(void) {
    ddr_block_t *block = NULL;
    uint32_t i = 0 ;
    ts_printf("begin:%x end:%x \n", __ddr_buf_start,  __ddr_buf_end);
    ts_printf("free:%x  alloc:%x\n", ddr_array_free,  ddr_array_alloc);
}

/* 合并空闲数组内物理相邻块 */
static void ddr_merge_free(void) {
    uint32_t i = 0;
    ddr_block_t *block = ddr_array_free->block;
    for (i = 0; i < ddr_array_free->block_cnt; ) {
        uint32_t j;
        uint8_t merged = 0;
        for (j = i + 1; j < ddr_array_free->block_cnt; j++) {
            if(!block[i].valid || !block[j].valid) {
                continue;
            }

            if ((block[i].base + block[i].size) == block[j].base) {
                block[i].size += block[j].size;
                uint32_t k;
                for(k = j; k < ddr_array_free->block_cnt -1U; k++){
                    block[k] = block[k+1];
                }
                ddr_array_free->block_cnt--;
                merged = 1;
                break;
            } else if ((block[j].base + block[j].size) == block[i].base) {
                block[j].size += block[i].size;
                uint32_t k;
                for(k = i; k < ddr_array_free->block_cnt -1U; k++) {
                    block[k] = block[k+1];
                }
                ddr_array_free->block_cnt--;
                merged = 1;
                break;
            }
        }
        if(!merged) {
            i++;
        }
    }
}

void *ddr_alloc(uint32_t size) {
    void *ptr = NULL;
    ddr_block_t *block = NULL;
    uint32_t i = 0, j = 0, k = 0, req_sz = 0, remain = 0;

    if (size == 0U) {
        ts_printf("%s:%s:%d alloc failed size = 0\n", __FILE__, __func__, __LINE__);
        return NULL;
    }

    if(ddr_array_free == NULL) {
        ddr_init();//        ddr_trace();
    }

    req_sz = ddr_align_size(size);

    /* first‑fit 遍历空闲数组 */
    for(i = 0U; i < ddr_array_free->block_cnt; i++) {
        block = ddr_array_free->block;
        if(block[i].valid == 0U || block[i].size < req_sz) {
            continue;
        }

        ptr = block[i].base;
        remain = block[i].size - req_sz;

        for(j = 0U; j < ddr_array_alloc->block_max; j++) {
            if(ddr_array_alloc->block[j].valid != 0U) {
                continue;
            }
            ddr_array_alloc->block[j].base  = ptr;
            ddr_array_alloc->block[j].size  = req_sz;
            ddr_array_alloc->block[j].valid = 1U;
            ddr_array_alloc->block[j].magic = DDR_MAGIC;
            ddr_array_alloc->block_cnt++;

            if(remain > 0U) {
                block[i].base  = (uint8_t*)ptr + req_sz;
                block[i].size  = remain;
            } else {
                /* 整块耗尽，删除空闲项 */
                for(k = i; k < ddr_array_free->block_cnt - 1U; k++) {
                    block[k] = block[k + 1];
                }
                ddr_array_free->block_cnt--;
            }
            return ptr;
        }
    }

    ts_printf("%s:%s:%d alloc failed no memory\n", __FILE__, __func__, __LINE__);
    return NULL;
}

void ddr_free(void *ddr) {
    uint8_t *ptr = (uint8_t *)ddr;
    ddr_block_t *block = NULL;
    uint32_t i = 0;

    if ((ddr_array_free == NULL) || (ddr == NULL)) {
        return;
    }
    /* 边界校验：业务内存绝对不能触碰尾部元数据保护区 */
    uint8_t *meta_start = (uint8_t *)__ddr_buf_end - META_RESERVE_TOTAL;
    if((ptr < __ddr_buf_start) || (ptr >= meta_start)) {
        ts_printf("ddr_free bad ptr %p, cross meta region\n", ptr);
        return;
    }

    block = ddr_array_alloc->block;
    for(i = 0U; i < ddr_array_alloc->block_max; i++) {
        if((block[i].valid == 0U) || (block[i].base != ptr)) {
            continue;
        }
        /* 魔术字校验，检测内存损坏 */
        if(block[i].magic != DDR_MAGIC){
            ts_printf("ddr_free magic corrupt ptr=%p\n",ptr);
            return;
        }

        /* 归还到空闲数组 */
        if(ddr_array_free->block_cnt < ddr_array_free->block_max) {
            ddr_array_free->block[ddr_array_free->block_cnt].base = ptr;
            ddr_array_free->block[ddr_array_free->block_cnt].size = block[i].size;
            ddr_array_free->block[ddr_array_free->block_cnt].valid = 1U;
            ddr_array_free->block[ddr_array_free->block_cnt].magic = DDR_MAGIC;
            ddr_array_free->block_cnt++;
            ddr_merge_free();
        }else{
            ts_printf("ddr_free free block table full!\n");
        }

        block[i].valid = 0U;
        block[i].base  = NULL;
        block[i].size  = 0U;
        block[i].magic = 0U;
        ddr_array_alloc->block_cnt--;
        return;
    }

    ts_printf("ddr_free ptr %p not found in alloc list\n", ptr);
}

uint32_t ddr_avail(void) {
    uint32_t total = 0U, i = 0U;
    if (ddr_array_free == NULL) {
        ddr_init();
    }
    for(i = 0U; i < ddr_array_free->block_cnt; i++) {
        if(ddr_array_free->block[i].valid)
            total += ddr_array_free->block[i].size;
    }
    return total;
}
