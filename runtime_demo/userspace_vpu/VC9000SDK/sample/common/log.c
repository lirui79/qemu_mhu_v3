#include "log.h"

enum logLevel globalLogLevel = LOG_LEVEL_INFO;
FILE *globalLogFile = NULL;
int errorAssertEnabled = 0;
static const char *logLevelString[] = { "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL" };

void setLogLevel(enum logLevel level)
{
    if (level >= LOG_LEVEL_TRACE && level <= LOG_LEVEL_FATAL) {
        globalLogLevel = level;
    }
}

void setLogFile(const char *path)
{
    if (!globalLogFile) {
        globalLogFile = fopen(path, "wb");
    }
}

void closeLogFile()
{
    if (globalLogFile) {
        fclose(globalLogFile);
        globalLogFile = NULL;
    }
}

const char *getLogLevelString(int level)
{
    return logLevelString[level];
}

void setLogErrorAssert(int enable)
{
    errorAssertEnabled = enable;
}
