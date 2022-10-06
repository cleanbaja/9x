#pragma once

#include <stdint.h>

#define cpu_pause() asm volatile ("yield");

#define cpu_read_sysreg(regname) ({               \
  unsigned long ret;                              \
  asm volatile ("mrs %0, " #regname : "=r"(ret)); \
  ret;                                            \
})

#define cpu_write_sysreg(regname, val) do {          \
  asm volatile ("msr " #regname ", %0" :: "r"(val)); \
} while(0);

#define cpunum() ({                               \
  unsigned long ret;                              \
  asm volatile ("mrs %0, MPIDR_EL1" : "=r"(ret)); \
  (ret & 0x7FFFFF);                               \
})

// TLB management functions
void tlb_flush_page(int asid, uintptr_t addr);
void tlb_flush_asid(int asid);
void tlb_flush_global();

void cpu_init();
