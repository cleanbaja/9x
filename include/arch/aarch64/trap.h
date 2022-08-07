#pragma once

#include <stdint.h>

struct cpu_regs {
  uint64_t x[31];
  uint64_t sp, pstate, ip;
  uint64_t ec, esr;
}; 

void trap_init();

