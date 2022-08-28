#pragma once

#include <stddef.h>
#include <stdint.h>

void memcpy(void* dest, const void* src, size_t len);
void memset(void* dest, const uint8_t val, size_t len);
int memcmp(const void* ptr1, const void* ptr2, size_t len);

size_t strlen(const char* str);
char *strchr(const char *s, int c);
int strcmp(const char *s1, const char *s2);
uint32_t strtol(const char *nptr, char **endptr, int base);

#define isalpha(_c) (_c > 64 && _c < 91) || (_c > 96 && _c < 123)
#define isdigit(_c) (_c > 47 && _c < 58)
#define isspace(_c) ((_c > 8 && _c < 14) || (_c == 32))
#define isupper(_c) (_c > 64 && _c < 91)
