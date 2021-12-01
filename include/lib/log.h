#ifndef LIB_LOG_H
#define LIB_LOG_H

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#include "sys/tables.h"

void log(char* fmt, ...);
void raw_log(char* fmt, ...);
int vsnprintf(char* buffer, size_t count, const char* format, va_list va);

static inline uintptr_t ctx_to_base(ctx_t* context) {
	return context->rbp;
}

// Stack tracing functions
void strace_unwind();
void strace_load(uint64_t ptr);

#define PANIC(ctx, m, ...) ({                  \
  raw_log("\nKERNEL PANIC on CPU #0\n");       \
  if (m) { raw_log(m, ##__VA_ARGS__); }        \
  if (ctx) {                                   \
    dump_regs(ctx);                            \
    strace_unwind(ctx_to_base(ctx));	       \
  } else {                                     \
    strace_unwind(0); \
  }                                            \
  for(;;) { __asm__ volatile ("hlt"); }        \
})

// 3F,D,A,G,C,I,E

#endif // LIB_LOG_H

