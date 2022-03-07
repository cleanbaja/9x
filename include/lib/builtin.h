#ifndef LIB_BUILTIN_H
#define LIB_BUILTIN_H

#include <stdint.h>
#include <stddef.h>

void
memset(void* ptr, uint64_t val, int len);
void
memset64(void* ptr, uint64_t val, int len);

void
memcpy(void* dest, const void* src, int len);
int
memcmp(const void* ptr1, const void* ptr2, int len);
void*
memmove(void *dest, const void *src, size_t n);

int
strlen(const char* str);

#endif // LIB_BUILTIN_H
