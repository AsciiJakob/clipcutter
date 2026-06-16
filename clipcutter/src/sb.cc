#include "sb.h"
#include <cstdlib>
#include <cstdio>
#include <cstdarg>
#include <cstring>

bool SB_init(SB* sb, size_t initialCapacity) {
    sb->buf = (char*) malloc(initialCapacity);
    if (!sb->buf) return false;

    sb->buf[0] = '\0';
    sb->len = 0;
    sb->cap = initialCapacity;

    return true;
}

// grow arena if string length + needed space doesn't fit.
// return false on error.
bool SB_grow(SB* sb, size_t needed) {
    size_t new_cap = sb->cap;
    while (new_cap < sb->len + needed + 1)
        new_cap *= 2;

    if (new_cap == sb->cap) return true;

    char* p = (char*) realloc(sb->buf, new_cap);
    if (!p) return false;
    sb->buf = p;
    sb->cap = new_cap;
    return true;
}

bool SB_prependf(SB* sb, const char* fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    int needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (needed < 0) return false;
    if (!SB_grow(sb, needed)) return false;

    char* tmp = (char*) malloc(needed + 1);
    if (!tmp) return false;

    va_start(ap, fmt);
    vsnprintf(tmp, needed + 1, fmt, ap);
    va_end(ap);

    memmove(sb->buf + needed, sb->buf, sb->len + 1);
    memcpy(sb->buf, tmp, needed);

    free(tmp);
    sb->len += needed;
    return true;
}

// append format str to str.
// return false on error.
bool SB_appendf(SB* sb, const char* fmt, ...) {
    va_list ap;

    // find out how much space is needed
    va_start(ap, fmt);
    int needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (needed < 0) return false;
    if (!SB_grow(sb, needed)) return false;

    va_start(ap, fmt);
    vsnprintf(sb->buf + sb->len, sb->cap - sb->len, fmt, ap);
    va_end(ap);

    sb->len += needed;
    return true;
}

void SB_free(SB* sb) {
    free(sb->buf);
    sb->buf = nullptr;
    sb->len = 0;
    sb->cap = 0;
}

