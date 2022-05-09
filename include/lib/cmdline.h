#ifndef LIB_CMDLINE_H
#define LIB_CMDLINE_H

#include <stdint.h>
#include <stdbool.h>

// Loads a command line into the internal list, if the internal list wasn't empty
void cmdline_load(const char* str);

const char* cmdline_get(const char* key);
uint32_t cmdline_get32(const char* key, uint32_t expected);
uint64_t cmdline_get64(const char* key, uint64_t expected);
bool  cmdline_get_bool(const char* key, bool expected);

#endif // LIB_CMDLINE_H

