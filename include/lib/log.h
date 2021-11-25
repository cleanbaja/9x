#ifndef LIB_LOG_H
#define LIB_LOG_H

#include <stddef.h>
#include <stdarg.h>

void log(char* fmt, ...);
int vsnprintf(char* buffer, size_t count, const char* format, va_list va);

#endif // LIB_LOG_H

