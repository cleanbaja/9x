#ifndef LIB_LOG_H
#define LIB_LOG_H

#include <stddef.h>
#include <stdarg.h>

#include "lib/log.h"

#define PANIC(ctx, m, ...) ({              \
  raw_log("\nKERNEL PANIC on CPU #0\n"); \
  if (m) { raw_log(m, ##__VA_ARGS__); }    \
  if (ctx) { dump_regs(ctx); }             \
  for(;;) { __asm__ volatile ("hlt"); }    \
})

void log(char* fmt, ...);
void raw_log(char* fmt, ...);
int vsnprintf(char* buffer, size_t count, const char* format, va_list va);

#endif // LIB_LOG_H

