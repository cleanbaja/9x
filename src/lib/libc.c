#include <lib/libc.h>

size_t strlen(const char* str) {
  size_t result = 0;

  while (*str++)
    result++;

  return result;
}

void memcpy(void* dest, const void* src, size_t len) {
#ifdef __x86_64__
  // Use ASM instructions, since they're faster
  asm ("rep movsb" :: "S"(src), "D"(dest), "c"(len));
#else
  const uint8_t* s = (const uint8_t*)src;
  uint8_t* d = (uint8_t*)dest;

  for (size_t i = 0; i < len; i++) {
    d[i] = s[i];
  }
#endif
}

void memset(void* dest, const uint8_t val, size_t len) {
#ifdef __x86_64__
  // Use ASM instructions, since they're faster
  asm ("rep stosb" :: "D"(dest), "a"(val), "c"(len));
#else
  uint8_t* d = (uint8_t*)dest;

  for (size_t i = 0; i < len; i++) {
    d[i] = val;
  }
#endif
}
