#include "time.h"
#include <cstdarg>
#include <cstdio>
#include "log.h"
#define UNUSED(x) (void)(x)

// has to be 1:1 mapped to LOG_LEVEL in the log.h file
const char* LOG_LEVEL_STRINGS[_LOG_LEVEL_COUNT] = {
    "Fatal",
    "Error",
    "Warn",
    "Info",
    "Trace",
    "Debug"
};

void log_message(LOG_LEVEL level, const char* filePath, int lineNum, const char* fmt, ...) {
    UNUSED(filePath);
    UNUSED(lineNum);

    va_list args;
    va_start(args, fmt);
    char logBuffer[2048];
    vsnprintf(logBuffer, sizeof(logBuffer), fmt, args);
    va_end(args);

#ifndef CC_BUILD_DEBUG
    if (level == LOG_LEVEL_DEBUG || level == LOG_LEVEL_TRACE) {
        return;
    }
#endif

    char timeStr[80];
    { 
        time_t     now = time(0);
        struct tm  tstruct;
        tstruct = *localtime(&now);
        // strftime(timeStr, sizeof(timeStr), "%Y-%m-%d.%X", &tstruct);
        strftime(timeStr, sizeof(timeStr), "%X", &tstruct);
    }

    const char* type = LOG_LEVEL_STRINGS[level];
    FILE* stream = (level <= LOG_LEVEL_WARN) ? stderr : stdout;

    #ifdef LOG_SHOW_FILEPATHS
    fprintf(stream, "%s [%s] [%s:%d] %s\n", timeStr, type, filePath, lineNum, logBuffer);
    #else
    fprintf(stream, "%s [%s] %s\n", timeStr, type, logBuffer);
    #endif

    if (stream == stderr)
        fflush(stream);
}
