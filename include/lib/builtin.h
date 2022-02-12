#ifndef LIB_BUILTIN_H
#define LIB_BUILTIN_H

#include <stdint.h>

void
memset(void* ptr, uint64_t val, int len);
void
memset64(void* ptr, uint64_t val, int len);
void
memcpy(void* dest, void* src, int len);

#endif // LIB_BUILTIN_H
