#include <lib/cmdline.h>
#include <lib/libc.h>
#include <misc/limine.h>
#include <stddef.h>

static char cmdline_data[CMDLINE_MAX_LEN];
static uint32_t cmdline_size;
static uint32_t cmdline_count;

volatile static struct limine_kernel_file_request kfile_req = {
  .id = LIMINE_KERNEL_FILE_REQUEST,
  .revision = 0
};

void cmdline_load(const char* ptr) {
  // Command lines can be NULL sometimes...
  if (ptr == NULL) return;
  unsigned i = cmdline_size;
  unsigned max = CMDLINE_MAX_LEN - 2;
  bool found_equal = false;

  while (i < max) {
    unsigned c = *ptr++;
    if (c == 0) {
      if (found_equal) {  // last option was null delimited
        ++cmdline_count;
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
      if ((i == 0) || (cmdline_data[i - 1] == 0)) {
        continue;
      } else {
        if (!found_equal && i < max) {
          cmdline_data[i++] = '=';
        }
        c = 0;
        found_equal = false;
        ++cmdline_count;
      }
    }
    cmdline_data[i++] = c;
  }

  if (!found_equal && i > 0 && cmdline_data[i - 1] != '\0' && i < max) {
    cmdline_data[i++] = '=';
    ++cmdline_count;
  }

  // ensure a double-\0 terminator
  cmdline_data[i++] = 0;
  cmdline_data[i] = 0;
  cmdline_size = i;
}

char* cmdline_get(const char* key) {
  if (!key) return cmdline_data;

  unsigned sz = strlen(key);
  char *ptr = cmdline_data;
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
  if (*ptr == '=') ptr++;

  return ptr;
}

bool cmdline_get_bool(const char* key, bool wanted) {
  const char *value = cmdline_get(key);
  if (value == NULL) return wanted;

  if ((strcmp(value, "0") == 0) || (strcmp(value, "false") == 0) ||
      (strcmp(value, "off") == 0)) {
    return false;
  }

  return true;
}

uint32_t cmdline_get_uint(const char* key, uint32_t wanted) {
  const char *value_raw = cmdline_get(key);
  if (value_raw == NULL || *value_raw == '\0')
      return wanted;

  char *end = NULL;
  long int value = strtol(value_raw, &end, 0);
  if (*end != '\0') return wanted;

  return value;
}

void cmdline_init() {
  char* cmdline_raw = kfile_req.response->kernel_file->cmdline;
  if (cmdline_raw != NULL) {
    cmdline_load(cmdline_raw);
  }
}
