#ifndef __KREF_H
#define __KREF_H

#include "atomic_t.h"

#ifdef __cplusplus
extern "C" {
#endif


/* 引用计数结构体，完全对齐Linux struct kref */
struct kref {
    atomic_t refcount;
};


static inline void kref_init(struct kref *kref)
{
    atomic_set(&kref->refcount, 1);
}

/* 增加引用计数 */
static inline void kref_get(struct kref *kref)
{
    atomic_inc(&kref->refcount);
}

/* 减少引用计数，计数到0调用release回调 */
typedef void (*kref_release_t)(struct kref *kref);
BaseType_t  kref_put(struct kref *kref, kref_release_t release);

/* 获取当前引用计数值（调试用） */
static inline uint32_t kref_read(struct kref *kref)
{
    return atomic_read(&kref->refcount);
}


static inline void kref_set(struct kref *kref, uint32_t val)
{
    atomic_set(&kref->refcount, val);
}

#ifdef __cplusplus
}
#endif

#endif