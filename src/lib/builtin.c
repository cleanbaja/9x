#include <lib/builtin.h>

void
memset64(void* ptr, uint64_t val, int len)
{
  uint64_t* real_ptr = (uint64_t*)ptr;

  for (int i = 0; i < (len / 8); i++) {
    real_ptr[i] = val;
  }
}

void
memset(void* ptr, uint64_t val, int len)
{
  uint8_t* real_ptr = (uint8_t*)ptr;

  for (int i = 0; i < len; i++) {
    real_ptr[i] = val;
  }
}

void
memcpy(void* dest, const void* src, int len)
{
  uint8_t* dest_ptr = (uint8_t*)dest;
  const uint8_t* src_ptr = (const uint8_t*)src;

  for (int i = 0; i < len; i++) {
    dest_ptr[i] = src_ptr[i];
  }
}

int
memcmp(const void* ptr1, const void* ptr2, int len)
{
  const uint8_t* ptr1_raw = (const uint8_t*)ptr1;
  const uint8_t* ptr2_raw = (const uint8_t*)ptr2;

  for (int i = 0; i < len; i++) {
    if (ptr1_raw[i] != ptr2_raw[i]) {
      return 1; 
    }
  }

  return 0;
}

void*
memmove(void *dest, const void *src, size_t n) {
    uint8_t *dest_ptr = (uint8_t *)dest;
    const uint8_t *src_ptr = (const uint8_t *)src;

    if (src > dest) {
        for (size_t i = 0; i < n; i++) {
            dest_ptr[i] = src_ptr[i];
        }
    } else if (src < dest) {
        for (size_t i = n; i > 0; i--) {
            dest_ptr[i-1] = src_ptr[i-1];
        }
    }

    return dest;
}

int
strlen(const char* str)
{
  int i = 0;
  while (*str++ != '\0') {
    i++;
  }
  return i;
}
