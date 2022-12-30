#pragma once

#include <arch/trap.h>

#define assert(expr) ({                                                            \
  if ( !(expr) ) {                                                                 \
    panic(NULL, "ASSERTION \"" #expr "\" FAILED AT %s:%d!\n", __FILE__, __LINE__); \
    __builtin_trap();                                                              \
  }                                                                                \
})

__attribute__((noreturn)) void panic(struct cpu_regs* context, const char* fmt, ...);

