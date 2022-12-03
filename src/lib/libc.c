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
  asm volatile ("cld; rep movsb" : "+S"(src), "+D"(dest), "+c"(len) :: "memory");
#else
  const uint8_t* s = (const uint8_t*)src;
  uint8_t* d = (uint8_t*)dest;

  for (size_t i = 0; i < len; i++) {
    d[i] = s[i];
  }
#endif
}

void *memmove(void *dest, const void *src, size_t n) {
  uint8_t *d = (uint8_t *)dest;
  const uint8_t *s = (const uint8_t *)src;

  if (src > dest) {
    for (size_t i = 0; i < n; i++) {
      d[i] = s[i];
    }
  } else if (src < dest) {
    for (size_t i = n; i > 0; i--) {
      d[i-1] = s[i-1];
    }
  }

  return dest;
}

void memset(void* dest, const uint8_t val, size_t len) {
#ifdef __x86_64__
  asm volatile ("cld; rep stosb" : "+D"(dest), "+c"(len) : "a"(val) : "memory");
#else
  uint8_t* d = (uint8_t*)dest;

  for (size_t i = 0; i < len; i++) {
    d[i] = val;
  }
#endif
}

int memcmp(const void *ptr1, const void *ptr2, size_t len) {
  const uint8_t *data1 = (const uint8_t *)ptr1;
  const uint8_t *data2 = (const uint8_t *)ptr2;

  for (int i = 0; i < len; i++) {
    if (data1[i] != data2[i]) {
      return 1;
    }
  }

  return 0;
}

char *strchr(const char *s, int c) {
  size_t i = 0;
  while (s[i]) {
    if (s[i] == c) return (char *)&s[i];

    i++;
  }

  if (c == 0) return (char *)&s[i];

  return NULL;
}

int strcmp(const char *s1, const char *s2) {
  while (1) {
    int res = ((*s1 == 0) || (*s1 != *s2));
    if (__builtin_expect((res), 0)) {
      break;
    }
    ++s1;
    ++s2;
  }
  return (*s1 - *s2);
}

// Copied from OpenBSD...
uint32_t strtol(const char *nptr, char **endptr, int base) {
  const char *s;
  uint32_t acc, cutoff;
  int c;
  int neg, any, cutlim;

  /*
   * Skip white space and pick up leading +/- sign if any.
   * If base is 0, allow 0x for hex and 0 for octal, else
   * assume decimal; if base is already 16, allow 0x.
   */
  s = nptr;
  do {
    c = (unsigned char)*s++;
  } while (isspace(c));
  if (c == '-') {
    neg = 1;
    c = *s++;
  } else {
    neg = 0;
    if (c == '+') c = *s++;
  }
  if ((base == 0 || base == 16) && c == '0' && (*s == 'x' || *s == 'X')) {
    c = s[1];
    s += 2;
    base = 16;
  }
  if (base == 0) base = c == '0' ? 8 : 10;

  /*
   * Compute the cutoff value between legal numbers and illegal
   * numbers.  That is the largest legal value, divided by the
   * base.  An input number that is greater than this value, if
   * followed by a legal input character, is too big.  One that
   * is equal to this value may be valid or not; the limit
   * between valid and invalid numbers is then based on the last
   * digit.  For instance, if the range for longs is
   * [-2147483648..2147483647] and the input base is 10,
   * cutoff will be set to 214748364 and cutlim to either
   * 7 (neg==0) or 8 (neg==1), meaning that if we have accumulated
   * a value > 214748364, or equal but the next digit is > 7 (or 8),
   * the number is too big, and we will return a range error.
   *
   * Set any if any `digits' consumed; make it negative to indicate
   * overflow.
   */
  cutoff = neg ? INT32_MIN : INT32_MAX;
  cutlim = cutoff % base;
  cutoff /= base;
  if (neg) {
    if (cutlim > 0) {
      cutlim -= base;
      cutoff += 1;
    }
    cutlim = -cutlim;
  }
  for (acc = 0, any = 0;; c = (unsigned char)*s++) {
    if (isdigit(c)) c -= '0';
    else if (isalpha(c))
      c -= isupper(c) ? 'A' - 10 : 'a' - 10;
    else
      break;
    if (c >= base) break;
    if (any < 0) continue;
    if (neg) {
      if (acc < cutoff || (acc == cutoff && c > cutlim)) {
        any = -1;
        acc = INT32_MIN;
      } else {
        any = 1;
        acc *= base;
        acc -= c;
      }
    } else {
      if (acc > cutoff || (acc == cutoff && c > cutlim)) {
        any = -1;
        acc = INT32_MAX;
      } else {
        any = 1;
        acc *= base;
        acc += c;
      }
    }
  }
  if (endptr != 0) *endptr = (char *)(any ? s - 1 : nptr);
  return (acc);
}

