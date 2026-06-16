#ifndef SB_H
#define SB_H
// growable string builder
typedef struct {
    char*  buf;
    size_t len;   // bytes written (excluding null terminator)
    size_t cap;   // allocated capacity
} SB;

bool SB_init(SB* sb, size_t initialCapacity);
bool SB_grow(SB* sb, size_t needed);
bool SB_appendf(SB* sb, const char* fmt, ...);
bool SB_prependf(SB* sb, const char* fmt, ...);
void SB_free(SB* sb);

#endif
