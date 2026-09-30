/*
 * Copyright (c) 2022, Vastai Tech. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "va_log.h"
#include "va_utils.h"

static const char *logLevelString[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
static const char *moduleString[] = {"DEC", "ENC"};

struct logContext {
    uint32_t logRegistered;
    enum vmppLogLevel logLevel;
    vmppLogCallback logCallback;
    const void *logUsrParameters;
};

static struct logContext logCtxs[2] = {{0, vmpp_LOG_INFO, NULL, NULL},
                                       {0, vmpp_LOG_INFO, NULL, NULL}};

void registerLogContext(enum VMPP_MOD module, vmppLogContext *logCtx)
{
    if (!logCtxs[module].logRegistered) {
        logCtxs[module].logRegistered = 1;        
        logCtxs[module].logCallback = logCtx->logCallback;
        logCtxs[module].logUsrParameters = logCtx->usrParameters;
    }
    logCtxs[module].logLevel = logCtx->logLevel;
}
const char *levelString(int level) { return logLevelString[level]; }
const char *modString(int mod) { return moduleString[mod]; }
int currentLogLevel(enum VMPP_MOD module) { return logCtxs[module].logLevel; }
int isCustomLogEnable(enum VMPP_MOD module) {return logCtxs[module].logRegistered && logCtxs[module].logCallback; };

void doCustomLog(enum VMPP_MOD module, int level, const char *file, const char *func, int line,
                 const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char msg[MAX_LOG_LEN] = "";
    vsnprintf(msg, MAX_LOG_LEN - 1, fmt, args);
    logCtxs[module].logCallback(logCtxs[module].logUsrParameters, level, moduleString[module],
                                file, func, line, msg);
    va_end(args);
}

void sdk_log_cb(int mod, int loglevel, const char* func, int line, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char msg[MAX_LOG_LEN] = "";
    vsnprintf(msg, MAX_LOG_LEN - 1, fmt, args);

    /*  mod = 0: decoder
        mod = 1: encoder
    */
    switch (loglevel) {
        case vmpp_LOG_DEBUG:
            LOG_DEBUG2(mod, "%s", "", func, line, msg);
            break;
        case vmpp_LOG_INFO:
            LOG_INFO2(mod, "%s", "", func, line, msg);
            break;
        case vmpp_LOG_WARN:
            LOG_WARN2(mod, "%s", "", func, line, msg);
            break;
        case vmpp_LOG_ERROR:
            LOG_ERROR2(mod, "%s", "", func, line, msg);
            break;
        default:
            LOG_DEBUG2(mod, "%s", "", func, line, msg);
            break;
    }
    va_end(args);
}
