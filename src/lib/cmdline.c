#include <lib/cmdline.h>
#include <lib/builtin.h>
#include <lib/kcon.h>

#define CMDLINE_BUF_SZ 4096
char __kernel_cmdline[CMDLINE_BUF_SZ];
unsigned __kernel_cmdline_size;
unsigned __kernel_cmdline_count;

// Simple command-line parser I stole from zircon
// https://fuchsia.googlesource.com/
void cmdline_load(const char* data) {
  unsigned i = __kernel_cmdline_size;
  unsigned max = CMDLINE_BUF_SZ - 2;

  // Stivale2 can give us a NULL cmdline, so be ready...
  if (data == NULL)
    return;

  bool found_equal = false;
  while (i < max) {
    unsigned c = *data++;
    if (c == 0) {
      if (found_equal) {  // last option was null delimited
        ++__kernel_cmdline_count;
      }
      break;
    }
    if (c == '=') {
      found_equal = true;
    }
    if ((c < ' ') || (c > 127)) {
      if ((c == '\n') || (c == '\r') || (c == '\t')) {
        c = ' ';
      } else {
        c = '.';
      }
    }
    if (c == ' ') {
      // spaces become \0's, but do not double up
      if ((i == 0) || (__kernel_cmdline[i - 1] == 0)) {
        continue;
      } else {
        if (!found_equal && i < max) {
          __kernel_cmdline[i++] = '=';
        }
        c = 0;
        found_equal = false;
        ++__kernel_cmdline_count;
      }
    }
    __kernel_cmdline[i++] = c;
  }
  if (!found_equal && i > 0 && __kernel_cmdline[i - 1] != '\0' && i < max) {
    __kernel_cmdline[i++] = '=';
    ++__kernel_cmdline_count;
  }

  // ensure a double-\0 terminator
  __kernel_cmdline[i++] = 0;
  __kernel_cmdline[i] = 0;
  __kernel_cmdline_size = i;
}

const char* cmdline_get(const char* key) {
  if (!key)
    return __kernel_cmdline;

  unsigned sz = strlen(key);
  const char* ptr = __kernel_cmdline;
  for (;;) {
    if (!memcmp(ptr, key, sz) && (ptr[sz] == '=' || ptr[sz] == '\0')) {
      break;
    }
    ptr = strchr(ptr, 0) + 1;
    if (*ptr == 0) {
      return NULL;
    }
  }

  ptr += sz;
  if (*ptr == '=')
    ptr++;

  return ptr;
}

bool cmdline_get_bool(const char* key, bool expected) {
  const char* value = cmdline_get(key);
  if (value == NULL)
    return expected;

  if ((strcmp(value, "0") == 0) || (strcmp(value, "false") == 0) ||
      (strcmp(value, "off") == 0)) {
    return false;
  }

  return true;
}

uint32_t cmdline_get32(const char* key, uint32_t expected) {
  const char* value_str = cmdline_get(key);
  if (value_str == NULL || *value_str == '\0')
    return expected;

  char* end;
  long int value = strtol(value_str, &end, 0);
  if (*end != '\0')
    return expected;

  return value;
}

uint64_t cmdline_get64(const char* key, uint64_t expected) {
  const char* value_str = cmdline_get(key);
  if (value_str == NULL || *value_str == '\0')
    return expected;

  char* end;
  long long value = strtoll(value_str, &end, 0);
  if (*end != '\0')
    return expected;

  return value;
}
