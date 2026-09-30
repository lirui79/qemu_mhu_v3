#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "log.h"
#include "queue.h"
#include "vmpp_common.h"

static inline uint64_t gettime_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec);
}

static inline void print_md5(uint8_t md5[], int size)
{
    int i;
    fprintf(stdout, COLOR_LIGHT_GREEN "MD5: ");
    for (i = 0; i < size; i++) fprintf(stdout, COLOR_LIGHT_GREEN "%02X", md5[i]);
    fprintf(stdout, "\n" COLOR_NONE);
}

static inline char *safe_strncpy(char *dst, const char *src, size_t dst_size, size_t cpy_size)
{
    size_t size = cpy_size;
    if (!dst || !src) {
        LOG_WARN("dst(0x%p) or src(x%p) is null", dst, src);
        return NULL;
    }
    if (dst_size <= 0 || cpy_size <= 0) {
        LOG_WARN("invalid cpy_size(%d) or dst_size(%d)", (int)cpy_size, (int)dst_size);
        return dst;
    }

    if (dst_size <= cpy_size) {
        LOG_WARN("cpy_size(%d) >= dst_size(%d)", (int)cpy_size, (int)dst_size);
        size = dst_size - 1;
    }
    strncpy(dst, src, size);
    dst[size] = '\0';
    return dst;
}

void read_files_from_dir(struct vmpp_queue *files, const char *directory);
int get_available_devices(struct vmpp_queue *devices);
int release_available_devices(struct vmpp_queue *devices, int dev_count);

/**
 * @brief  Get the first video device found on the board.
 * @param  [out] device  -  buffer receiving the device name, such as /dev/vastai_video0
 * @param  [in]  size    -  size of the buffer
 * @return 0 on success, negative if no video device is found
 */
int get_default_video_device(char *device, int size);
#endif