#include <lib/builtin.h>
#include <limits.h>
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

// Stolen from OpenBSD...
uint32_t strtol(const char* nptr, char** endptr, int base) {
  const char* s;
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
    if (c == '+')
      c = *s++;
  }
  if ((base == 0 || base == 16) && c == '0' && (*s == 'x' || *s == 'X')) {
    c = s[1];
    s += 2;
    base = 16;
  }
  if (base == 0)
    base = c == '0' ? 8 : 10;

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
    if (isdigit(c))
      c -= '0';
    else if (isalpha(c))
      c -= isupper(c) ? 'A' - 10 : 'a' - 10;
    else
      break;
    if (c >= base)
      break;
    if (any < 0)
      continue;
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
  if (endptr != 0)
    *endptr = (char*)(any ? s - 1 : nptr);
  return (acc);
}

uint64_t strtoll(const char* nptr, char** endptr, int base) {
  const char* s;
  uint64_t acc, cutoff;
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
    if (c == '+')
      c = *s++;
  }
  if ((base == 0 || base == 16) && c == '0' && (*s == 'x' || *s == 'X')) {
    c = s[1];
    s += 2;
    base = 16;
  }
  if (base == 0)
    base = c == '0' ? 8 : 10;

  /*
   * Compute the cutoff value between legal numbers and illegal
   * numbers.  That is the largest legal value, divided by the
   * base.  An input number that is greater than this value, if
   * followed by a legal input character, is too big.  One that
   * is equal to this value may be valid or not; the limit
   * between valid and invalid numbers is then based on the last
   * digit.  For instance, if the range for long longs is
   * [-9223372036854775808..9223372036854775807] and the input base
   * is 10, cutoff will be set to 922337203685477580 and cutlim to
   * either 7 (neg==0) or 8 (neg==1), meaning that if we have
   * accumulated a value > 922337203685477580, or equal but the
   * next digit is > 7 (or 8), the number is too big, and we will
   * return a range error.
   *
   * Set any if any `digits' consumed; make it negative to indicate
   * overflow.
   */
  cutoff = neg ? INT64_MIN : INT64_MAX;
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
    if (isdigit(c))
      c -= '0';
    else if (isalpha(c))
      c -= isupper(c) ? 'A' - 10 : 'a' - 10;
    else
      break;
    if (c >= base)
      break;
    if (any < 0)
      continue;
    if (neg) {
      if (acc < cutoff || (acc == cutoff && c > cutlim)) {
        any = -1;
        acc = INT64_MIN;
      } else {
        any = 1;
        acc *= base;
        acc -= c;
      }
    } else {
      if (acc > cutoff || (acc == cutoff && c > cutlim)) {
        any = -1;
        acc = INT64_MAX;
      } else {
        any = 1;
        acc *= base;
        acc += c;
      }
    }
  }
  if (endptr != 0)
    *endptr = (char*)(any ? s - 1 : nptr);
  return (acc);
}
