#ifndef LOG_H
#define LOG_H


// has to be 1:1 mapped to LOG_LEVEL_STRINGS in the log.cc file
enum LOG_LEVEL {
    LOG_LEVEL_FATAL,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_TRACE,
    LOG_LEVEL_DEBUG,
    _LOG_LEVEL_COUNT
};

#define log_fatal(fmt, ...) log_message(LOG_LEVEL_FATAL, __FILE__, __LINE__, fmt, __VA_ARGS__);
#define log_error(fmt, ...) log_message(LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, __VA_ARGS__);
#define log_warn(fmt, ...) log_message(LOG_LEVEL_WARN, __FILE__, __LINE__, fmt, __VA_ARGS__);
#define log_info(fmt, ...) log_message(LOG_LEVEL_INFO, __FILE__, __LINE__, fmt, __VA_ARGS__);
#define log_trace(fmt, ...) log_message(LOG_LEVEL_TRACE, __FILE__, __LINE__, fmt, __VA_ARGS__);
#define log_debug(fmt, ...) log_message(LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, __VA_ARGS__);

void log_message(LOG_LEVEL level, const char* filePath, int lineNum, const char* fmt, ...);

#endif
