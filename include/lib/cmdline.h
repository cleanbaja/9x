#ifndef LIB_CMDLINE_H
#define LIB_CMDLINE_H

#include <stdbool.h>

// Loads a command line into the internal list, if the internal list wasn't empty
void cmdline_load(char* str);

bool cmdline_is_present(const char* key);
char* cmdline_get_str(const char* key);

#endif // LIB_CMDLINE_H

