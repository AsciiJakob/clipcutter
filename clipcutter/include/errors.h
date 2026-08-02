#ifndef ERRORS_H
#define ERRORS_H

struct CC_FFmpegError {
    char* message;
    char* FFmpegError;
};

char* alloc_error(const char* fmt, ...);
void popup_error(const char* title, char* message);

#endif
