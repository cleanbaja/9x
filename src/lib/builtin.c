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
memcpy(void* dest, void* src, int len)
{
  uint8_t* dest_ptr = (uint8_t*)dest;
  uint8_t* src_ptr = (uint8_t*)src;

  for (int i = 0; i < len; i++) {
    dest_ptr[i] = src_ptr[i];
  }
}
