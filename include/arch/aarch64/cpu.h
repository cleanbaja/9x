#pragma once

#define cpu_pause() asm volatile ("yield");
#define cpunum() ({                               \
  unsigned long ret;                              \
  asm volatile ("mrs %0, MPIDR_EL1" : "=r"(ret)); \
  (ret & 0x7FFFFF);                               \
})

