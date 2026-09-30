/*
 * Copyright (c) 2022, Vastai Tech. All rights reserved
 *
 * The information contained herein is confidential
 * property of Company. The user, copying, transfer or
 * disclosure of such information is prohibited except
 * by express written agreement with VASTAITECH.
 */

#ifndef __VA_LOG_H__
#define __VA_LOG_H__

#include "vmpp_common.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
 #include <sys/syscall.h>

#define COLOR_NONE "\033[m"
#define COLOR_WHITE "\033[1;37m"
#define COLOR_GREEN "\033[0;32;32m"
#define COLOR_CYAN "\033[0;36m"
#define COLOR_LIGHT_RED "\033[1;31m"
#define COLOR_YELLOW "\033[1;33m"
#define STRING(x) #x
#define VA_FILE (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define VA_TIMESTAMP "%Y-%m-%d %H:%M:%S"
#define MAX_LOG_LEN 2048
enum VMPP_MOD { DEC, ENC };

extern void registerLogContext(enum VMPP_MOD module, vmppLogContext *logCtx);
extern int currentLogLevel(enum VMPP_MOD module);
extern const char *levelString(int level);
extern const char *modString(int mod);
extern void doCustomLog(enum VMPP_MOD module, int level, const char *file, const char *func,
                        int line, const char *fmt, ...);
extern int isCustomLogEnable(enum VMPP_MOD module);

static inline char *timenow()
{
    time_t rawtime = time(NULL);
    struct tm *timeinfo = localtime(&rawtime);
    static char now[64];
    now[strftime(now, sizeof(now), VA_TIMESTAMP, timeinfo)] = '\0';
    return now;
}
#define LOG_FMT(fmt) ("%s [%lu:%u][%s] %s%-5s\x1b[0m [%s:%d %s] %s" fmt "\x1b[0m\n")
#define LOG_ARGS(module, color, level, file, func, line) \
    timenow(), syscall(SYS_gettid), getpid(), modString(module), color, levelString(level), file, line, func, color

#define LOG_FMT2(fmt) ("%s [%lu:%u][%s] %s%-5s\x1b[0m [%s:%d] %s" fmt "\x1b[0m\n")
#define LOG_ARGS2(module, color, level, func, line) \
    timenow(), syscall(SYS_gettid), getpid(), modString(module), color, levelString(level), func, line, color

#define LOG_FULL(module, level, fmt, color, file, func, line, ...)                                                  \
    do {                                                                                                            \
        if (level >= currentLogLevel(module)) {                                                                     \
            if (isCustomLogEnable(module))                                                                          \
                doCustomLog(module, level, file, func, line, fmt, ##__VA_ARGS__);                                   \
            else {                                                                                                  \
                if (strcmp(file, "") != 0)                                                                          \
                    fprintf(stdout, LOG_FMT(fmt), LOG_ARGS(module, color, level, file, func, line), ##__VA_ARGS__); \
                else                                                                                                \
                    fprintf(stdout, LOG_FMT2(fmt), LOG_ARGS2(module, color, level, func, line), ##__VA_ARGS__);     \
            }                                                                                                       \
        }                                                                                                           \
    } while (0)

#define LOG_FULL_IGNORE_LEVEL(module, level, fmt, color, file, func, line, ...)                             \
    do {                                                                                                    \
        if (isCustomLogEnable(module))                                                                      \
            doCustomLog(module, level, file, func, line, fmt, ##__VA_ARGS__);                               \
        else                                                                                                \
            fprintf(stdout, LOG_FMT(fmt), LOG_ARGS(module, color, level, file, func, line), ##__VA_ARGS__); \
    } while (0)

#define LOG_DEBUG(module, fmt, ...) \
    LOG_FULL(module, vmpp_LOG_DEBUG, fmt, COLOR_WHITE, VA_FILE, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOG_INFO(module, fmt, ...) \
    LOG_FULL(module, vmpp_LOG_INFO, fmt, COLOR_CYAN, VA_FILE, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOG_WARN(module, fmt, ...) \
    LOG_FULL(module, vmpp_LOG_WARN, fmt, COLOR_YELLOW, VA_FILE, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOG_ERROR(module, fmt, ...) \
    LOG_FULL(module, vmpp_LOG_ERROR, fmt, COLOR_LIGHT_RED, VA_FILE, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOG(module, level, color, fmt, ...) \
    LOG_FULL_IGNORE_LEVEL(module, level, fmt, color, VA_FILE, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define LOG_DEBUG2(module, fmt, file, func, line, ...) \
    LOG_FULL(module, vmpp_LOG_DEBUG, fmt, COLOR_WHITE, file, func, line, ##__VA_ARGS__)
#define LOG_INFO2(module, fmt, file, func, line, ...) \
    LOG_FULL(module, vmpp_LOG_INFO, fmt, COLOR_CYAN, file, func, line, ##__VA_ARGS__)
#define LOG_WARN2(module, fmt, file, func, line, ...) \
    LOG_FULL(module, vmpp_LOG_WARN, fmt, COLOR_YELLOW, file, func, line, ##__VA_ARGS__)
#define LOG_ERROR2(module, fmt, file, func, line, ...) \
    LOG_FULL(module, vmpp_LOG_ERROR, fmt, COLOR_LIGHT_RED, file, func, line, ##__VA_ARGS__)
#define LOG2(module, level, color, fmt, file, func, line, ...) \
    LOG_FULL_IGNORE_LEVEL(module, level, fmt, color, file, func, line, ##__VA_ARGS__)

#endif //__VA_LOG_H__
