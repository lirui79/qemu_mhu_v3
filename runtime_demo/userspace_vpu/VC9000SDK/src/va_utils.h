/*
 * Copyright (c) 2022, Vastai Tech. All rights reserved
 *
 * The information contained herein is confidential
 * property of Company. The user, copying, transfer or
 * disclosure of such information is prohibited except
 * by express written agreement with VASTAITECH.
 */

#ifndef __VA_UTILS_H__
#define __VA_UTILS_H__

#include "vmpp_common.h"

// system headers
#include <stdatomic.h>

#define UNUSED_PARAMETER(param) (void)param

static inline uint32_t atomic_set_u32(volatile uint32_t *ptr, uint32_t value)
{
    return __atomic_exchange_n(ptr, value, __ATOMIC_SEQ_CST);
}

static inline uint32_t atomic_get_u32(const volatile uint32_t *ptr)
{
    return __atomic_load_n(ptr, __ATOMIC_SEQ_CST);
}

static inline uint32_t atomic_add_fetch_u32(volatile uint32_t *ptr)
{
    return __atomic_add_fetch(ptr, 1, __ATOMIC_SEQ_CST);
}

static inline uint32_t is_handle_valid(vmppHandle handle) { return handle != NULL; }

uint64_t va_gettime_ns(void);

#endif /* __VA_UTILS_H__ */