#ifndef LIB_BUILTIN_H
#define LIB_BUILTIN_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

// Memset functions
void memset(void *ptr, uint64_t val, int len);
void memset64(void *ptr, uint64_t val, int len);

// Other memory-related functions
void memcpy(void *dest, const void *src, int len);
int memcmp(const void *ptr1, const void *ptr2, int len);
void *memmove(void *dest, const void *src, size_t n);

// String functions
int strlen(const char *str);
int strcmp(const char *s1, const char *s2);
int snprintf(char *buffer, size_t count, const char *format, ...);
char *strtok_r(char *s, const char *del, char **m);

// Other string related functions
char *strchr(const char *s, int n);
char *strcpy(char *dst, const char *src);
char *strdup(const char *s);
int vsnprintf(char *buffer, size_t count, const char *format, va_list va);

// Conversion functions
uint32_t strtol(const char *nptr, char **endptr, int base);
uint64_t strtoll(const char *nptr, char **endptr, int base);

// Bitwise functions
#define BIT_SET(bitmap, __bit) (bitmap[(__bit) / 8] |= (1 << ((__bit) % 8)))
#define BIT_CLEAR(bitmap, __bit) (bitmap[(__bit) / 8] &= ~(1 << ((__bit) % 8)))
#define BIT_TEST(bitmap, __bit) ((bitmap[(__bit) / 8] >> ((__bit) % 8)) & 1)

// Character functions
#define isalpha(_c) (_c > 64 && _c < 91) || (_c > 96 && _c < 123)
#define isdigit(_c) (_c > 47 && _c < 58)
#define isspace(_c) ((_c > 8 && _c < 14) || (_c == 32))
#define isupper(_c) (_c > 64 && _c < 91)

// Convienent macros
#define DIV_ROUNDUP(A, B)    \
  ({                         \
    typeof(A) _a_ = A;       \
    typeof(B) _b_ = B;       \
    (_a_ + (_b_ - 1)) / _b_; \
  })
#define ALIGN_UP(A, B)              \
  ({                                \
    typeof(A) _a__ = A;             \
    typeof(B) _b__ = B;             \
    DIV_ROUNDUP(_a__, _b__) * _b__; \
  })
#define ALIGN_DOWN(A, B) \
  ({                     \
    typeof(A) _a_ = A;   \
    typeof(B) _b_ = B;   \
    (_a_ / _b_) * _b_;   \
  })
#define ARRAY_LEN(arr) (sizeof(arr) / sizeof(*arr))

// Stack tracing functions
void strace_unwind();
void strace_load(uint64_t ptr);
uintptr_t strace_get_symbol(char *name);

#endif  // LIB_BUILTIN_H
