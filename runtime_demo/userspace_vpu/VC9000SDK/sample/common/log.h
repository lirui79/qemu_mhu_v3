/*
 * Copyright (c) 2022, Vastai Tech. All rights reserved
 *
 * The information contained herein is confidential
 * property of Company. The user, copying, transfer or
 * disclosure of such information is prohibited except
 * by express written agreement with VASTAITECH.
 */

#ifndef __LOG_H__
#define __LOG_H__

#include "defs.h"
#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#ifdef DEBUG_MODE
#define COLOR_WHITE       "\033[0;37m"
#define COLOR_GREEN       "\033[0;32;32m"
#define COLOR_BLUE        "\033[0;32;34m"
#define COLOR_LIGHT_GREEN "\033[1;32m"
#define COLOR_BROWN       "\033[0;33m"
#define COLOR_CYAN        "\033[0;36m"
#define COLOR_LIGHT_CYAN  "\033[1;36m"
#define COLOR_LIGHT_RED   "\033[1;31m"
#define COLOR_YELLOW      "\033[1;33m"
#define COLOR_PURPLE      "\033[1;35m"
#define COLOR_DARK_GRAY   "\033[1;30m"
#define COLOR_NONE        "\033[0m"
#else
#define COLOR_WHITE       ""
#define COLOR_GREEN       ""
#define COLOR_BLUE        ""
#define COLOR_LIGHT_GREEN ""
#define COLOR_BROWN       ""
#define COLOR_CYAN        ""
#define COLOR_LIGHT_CYAN  ""
#define COLOR_LIGHT_RED   ""
#define COLOR_YELLOW      ""
#define COLOR_PURPLE      ""
#define COLOR_DARK_GRAY   ""
#define COLOR_NONE        ""
#endif
#define VA_FILE       strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__
#define VA_TIMESTAMP  "%Y-%m-%d %H:%M:%S"
#define VA_TIMESTAMP2 "%Y%m%d%H%M%S"

/* Consistent with the definition of SDK */
enum logLevel {
    LOG_LEVEL_TRACE,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL
};

extern enum logLevel globalLogLevel;
extern const char *getLogLevelString(int level);
extern void setLogLevel(enum logLevel level);
extern FILE *globalLogFile;
extern void setLogFile(const char *path);
extern void closeLogFile();
extern void setLogErrorAssert(int enable);
extern int errorAssertEnabled;

static inline char *timenow(const char *timestamp)
{
    time_t rawtime = time(NULL);
    struct tm *timeinfo = localtime(&rawtime);
    static char now[64] = { 0 };
    now[strftime(now, sizeof(now), timestamp, timeinfo)] = '\0';
    return now;
}

#define LOG_FMT(fmt) ("%s [%lu:%u]%s[SPL]" COLOR_NONE " %s%-5s" COLOR_NONE " [%s:%d %s] %s" fmt COLOR_NONE "\n")
#define LOG_ARGS(color, level)                                                                                         \
    timenow(VA_TIMESTAMP), syscall(SYS_gettid), getpid(), color, color, getLogLevelString(level), VA_FILE, __LINE__,   \
        __FUNCTION__, color

/* for logging into file */
#define LOG_FMT_FILE(fmt) ("%s [%lu:%u][SPL] %-5s [%s:%d %s] " fmt "\n")
#define LOG_ARGS_FILE(color, level)                                                                                    \
    timenow(VA_TIMESTAMP), syscall(SYS_gettid), getpid(), getLogLevelString(level), VA_FILE, __LINE__, __FUNCTION__

/* no line break */
#define LOG_FMT_NLB(fmt)  ("%s [%lu:%u]%s[SPL]" COLOR_NONE " %s%-5s" COLOR_NONE " [%s:%d %s] %s" fmt COLOR_NONE)
#define LOG_FMT_FNLB(fmt) ("%s [%lu:%u][SPL] %-5s [%s:%d %s] " fmt)

#define LOG_FULL(level, fmt, color, ...)                                                                               \
    do {                                                                                                               \
        if (level >= globalLogLevel) {                                                                                 \
            fprintf(stdout, LOG_FMT(fmt), LOG_ARGS(color, level), ##__VA_ARGS__);                                      \
            fflush(stdout);                                                                                            \
            if (globalLogFile) {                                                                                       \
                fprintf(globalLogFile, LOG_FMT_FILE(fmt), LOG_ARGS_FILE(color, level), ##__VA_ARGS__);                 \
                fflush(globalLogFile);                                                                                 \
            }                                                                                                          \
        }                                                                                                              \
        if (errorAssertEnabled && level >= LOG_LEVEL_ERROR) {                                                          \
            assert(0);                                                                                                 \
        }                                                                                                              \
    } while (0)

#define LOG_IGNORE_LEVEL(level, fmt, color, ...)                                                                       \
    do {                                                                                                               \
        fprintf(stdout, LOG_FMT(fmt), LOG_ARGS(color, level), ##__VA_ARGS__);                                          \
        fflush(stdout);                                                                                                \
        if (globalLogFile) {                                                                                           \
            fprintf(globalLogFile, LOG_FMT_FILE(fmt), LOG_ARGS_FILE(color, level), ##__VA_ARGS__);                     \
            fflush(globalLogFile);                                                                                     \
        }                                                                                                              \
        if (errorAssertEnabled && level >= LOG_LEVEL_ERROR) {                                                          \
            assert(0);                                                                                                 \
        }                                                                                                              \
    } while (0)

#define LOG_NO_LINE_BREAK(level, fmt, color, ...)                                                                      \
    do {                                                                                                               \
        if (level >= globalLogLevel) {                                                                                 \
            fprintf(stdout, LOG_FMT_NLB(fmt), LOG_ARGS(color, level), ##__VA_ARGS__);                                  \
            fflush(stdout);                                                                                            \
            if (globalLogFile) {                                                                                       \
                fprintf(globalLogFile, LOG_FMT_FNLB(fmt), LOG_ARGS_FILE(color, level), ##__VA_ARGS__);                 \
                fflush(globalLogFile);                                                                                 \
            }                                                                                                          \
        }                                                                                                              \
        if (errorAssertEnabled && level >= LOG_LEVEL_ERROR) {                                                          \
            assert(0);                                                                                                 \
        }                                                                                                              \
    } while (0)

#define LOG_TRACE(fmt, ...)            LOG_FULL(LOG_LEVEL_TRACE, fmt, COLOR_DARK_GRAY, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...)            LOG_FULL(LOG_LEVEL_DEBUG, fmt, COLOR_WHITE, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)             LOG_FULL(LOG_LEVEL_INFO, fmt, COLOR_CYAN, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)             LOG_FULL(LOG_LEVEL_WARN, fmt, COLOR_YELLOW, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)            LOG_FULL(LOG_LEVEL_ERROR, fmt, COLOR_LIGHT_RED, ##__VA_ARGS__)
#define LOG(level, color, fmt, ...)    LOG_FULL(level, fmt, color, ##__VA_ARGS__)

#define LOGIL_TRACE(fmt, ...)          LOG_IGNORE_LEVEL(LOG_LEVEL_TRACE, fmt, COLOR_DARK_GRAY, ##__VA_ARGS__)
#define LOGIL_DEBUG(fmt, ...)          LOG_IGNORE_LEVEL(LOG_LEVEL_DEBUG, fmt, COLOR_WHITE, ##__VA_ARGS__)
#define LOGIL_INFO(fmt, ...)           LOG_IGNORE_LEVEL(LOG_LEVEL_INFO, fmt, COLOR_CYAN, ##__VA_ARGS__)
#define LOGIL_WARN(fmt, ...)           LOG_IGNORE_LEVEL(LOG_LEVEL_WARN, fmt, COLOR_YELLOW, ##__VA_ARGS__)
#define LOGIL_ERROR(fmt, ...)          LOG_IGNORE_LEVEL(LOG_LEVEL_ERROR, fmt, COLOR_LIGHT_RED, ##__VA_ARGS__)
#define LOGIL(level, color, fmt, ...)  LOG_IGNORE_LEVEL(level, fmt, color, ##__VA_ARGS__)

#define LOGNLB_TRACE(fmt, ...)         LOG_NO_LINE_BREAK(LOG_LEVEL_TRACE, fmt, COLOR_DARK_GRAY, ##__VA_ARGS__)
#define LOGNLB_DEBUG(fmt, ...)         LOG_NO_LINE_BREAK(LOG_LEVEL_DEBUG, fmt, COLOR_WHITE, ##__VA_ARGS__)
#define LOGNLB_INFO(fmt, ...)          LOG_NO_LINE_BREAK(LOG_LEVEL_INFO, fmt, COLOR_CYAN, ##__VA_ARGS__)
#define LOGNLB_WARN(fmt, ...)          LOG_NO_LINE_BREAK(LOG_LEVEL_WARN, fmt, COLOR_YELLOW, ##__VA_ARGS__)
#define LOGNLB_ERROR(fmt, ...)         LOG_NO_LINE_BREAK(LOG_LEVEL_ERROR, fmt, COLOR_LIGHT_RED, ##__VA_ARGS__)
#define LOGNLB(level, color, fmt, ...) LOG_NO_LINE_BREAK(level, fmt, color, ##__VA_ARGS__)

#endif    //__LOG_H__
