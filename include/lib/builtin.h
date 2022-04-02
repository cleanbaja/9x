#ifndef LIB_BUILTIN_H
#define LIB_BUILTIN_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

// Memset functions
void
memset(void* ptr, uint64_t val, int len);
void
memset64(void* ptr, uint64_t val, int len);

// Other memory-related functions
void
memcpy(void* dest, const void* src, int len);
int
memcmp(const void* ptr1, const void* ptr2, int len);
void*
memmove(void *dest, const void *src, size_t n);

// String functions
int
strlen(const char* str);
int
strcmp(const char* s1, const char* s2);
int
snprintf(char* buffer, size_t count, const char* format, ...);
int
vsnprintf(char* buffer, size_t count, const char* format, va_list va);

// Bitwise functions
#define BIT_SET(bitmap, __bit) (bitmap[(__bit) / 8] |= (1 << ((__bit) % 8)))
#define BIT_CLEAR(bitmap, __bit) (bitmap[(__bit) / 8] &= ~(1 << ((__bit) % 8)))
#define BIT_TEST(bitmap, __bit) ((bitmap[(__bit) / 8] >> ((__bit) % 8)) & 1)

// Stack tracing functions
void
strace_unwind();
void
strace_load(uint64_t ptr);
uintptr_t
strace_get_symbol(char* name);

#endif // LIB_BUILTIN_H

