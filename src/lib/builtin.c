#include <lib/builtin.h>
#include <vm/vm.h>

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

int
strcmp(const char* s1, const char* s2)
{
    while(*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

char* strdup(const char* s) {
  char* str = kmalloc(strlen(s));
  memcpy(str, s, strlen(s));
  return str;
}

char* strchr(const char *s, int c) {
  size_t i = 0;
  while(s[i]) {
    if(s[i] == c)
      return (char*)&s[i];
    
    i++;
  }

  if(c == 0)
    return (char*)&s[i];

  return NULL;
}

// Stolen from mlibc...
char *strtok_r(char *s, const char *del, char **m) {
  // We use *m = null to memorize that the entire string was consumed.
  char *tok;
  if(s) {
    tok = s;
  } else if(*m) {
    tok = *m;
  } else {
    return NULL;
  }

  // Skip initial delimiters.
  // After this loop: *tok is non-null iff we return a token.
  while(*tok && strchr(del, *tok))
    tok++;

  // Replace the following delimiter by a null-terminator.
  // After this loop: *p is null iff we reached the end of the string.
  char* p = tok;
  while(*p && !strchr(del, *p))
    p++;

  if(*p) {
    *p = 0;
    *m = p + 1;
  } else {
    *m = NULL;
  }

  if(p == tok)
    return NULL;

  return tok;
}

