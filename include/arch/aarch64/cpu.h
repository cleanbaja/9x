#pragma once

#define cpu_read_sysreg(regname) ({               \
  unsigned long ret;                              \
  asm volatile ("mrs %0, " #regname : "=r"(ret)); \
  ret;                                            \
})

#define cpu_write_sysreg(regname) do {            \
  asm volatile ("msr " #regname ", %0" :: "r"(ret)); \
} while(0);

#define cpunum() ({                               \
  unsigned long ret;                              \
  asm volatile ("mrs %0, MPIDR_EL1" : "=r"(ret)); \
  (ret & 0x7FFFFF);                               \
})

#define cpu_pause() asm volatile ("yield");
