#include <lib/cmdline.h>
#include <lib/builtin.h>
#include <lib/kcon.h>

static char* cmdline_raw = NULL;
static size_t npairs = 0;

void cmdline_load(char* str) {
  if (cmdline_raw != NULL || str == NULL)
    return; // We already have a cmdline, or the arg is NULL!

  klog("Kernel Command Line -> %s", str);  

  size_t len = strlen(str);
  for (size_t i = 0; i < len; i++) {
    if (str[i] == ' ' || str[i] == '\0') {
      str[i] = '\0';
      npairs++;
    }
  }

  if (len != 0) npairs++;
  cmdline_raw = str;
}

bool cmdline_is_present(const char* key) {
  char* index = cmdline_raw;
  for (size_t i = 0; i < npairs; i++) {
    if (strcmp(index, key) == 0)
      return true;

    index += strlen(index) + 1;
  }

  return false;
}

char* cmdline_get_str(const char* key) {
 char* index = cmdline_raw;
  for (size_t i = 0; i < npairs; i++) {
    if (strcmp(index, key) == 0)
      return index + strlen(key) + 1; // Skip the equals sign

    index += strlen(index) + 1;
  }

  return NULL;
}

