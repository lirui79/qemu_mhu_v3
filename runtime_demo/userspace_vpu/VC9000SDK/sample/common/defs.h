#ifndef __DEFS_H__
#define __DEFS_H__

#include <stdint.h>
#include <stddef.h>

/* clang-format off */

#define DEBUG_MODE
#define UNUSED(param)           (void)param
#define MAX_PATH_LEN            (1024)
#define MAX_STREAM_SIZE         (16 * 1024 * 1024)
//#define MAX_STREAM_SIZE_4_JPEG  (512 * 1024 * 1024)
#define MAX_STREAM_SIZE_4_JPEG  (16 * 1024 * 1024)
#define DEFAULT_PERIOD_FRAMES   (100)
#define MD5_HASH_LEN            (16)
#define PERF_PERIOD_FRAMES      (1000)

typedef unsigned long long u64;

/* clang-format on */

#endif  // __DEFS_H__