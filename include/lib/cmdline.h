#pragma once

#include <stdint.h>
#include <stdbool.h>

#define CMDLINE_MAX_LEN 2048

void cmdline_load(const char* ptr);
char* cmdline_get(const char* key);
bool cmdline_get_bool(const char* key, bool wanted);
uint32_t cmdline_get_uint(const char* key, uint32_t wanted);

void cmdline_init();
