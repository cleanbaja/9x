#pragma once

#include <stddef.h>
#include <stdint.h>

size_t strlen(const char* str);
void memcpy(void* dest, const void* src, size_t len);
void memset(void* dest, const uint8_t val, size_t len);

