#include "kref.h"

#ifdef __FREERTOS__
#include "osal_freertos.h" /* needed for the _IOW etc stuff used later */
#endif

BaseType_t kref_put(struct kref *kref, kref_release_t release)
{
    uint32_t refcount = atomic_fetch_sub(&kref->refcount, 1);
    if (release == NULL) {
        return pdFALSE;
    }

    /* 减之前等于1，说明减后为0，需要释放 */
    if (refcount == 1) {
        release(kref);
        return pdTRUE;
    }

    return pdFALSE;
}