#pragma once

#include <stdint.h>

static inline uint64_t asm_rdmsr(uint32_t msr) {
  uint32_t hi = 0, lo = 0;

  asm volatile(
    "rdmsr;"
    : "=a"(lo), "=d"(hi)
    : "c"(msr)
    : "memory");

  uint64_t value = ((uint64_t)hi << 32) | lo;
  return value;
}

static inline void asm_wrmsr(uint32_t msr, uint64_t val) {
  uint32_t lo = val & UINT32_MAX;
  uint32_t hi = (val >> 32) & UINT32_MAX;

  asm volatile(
    "wrmsr;"
    :
    : "c"(msr), "a"(lo), "d"(hi)
    : "memory");
}

#define cpu_pause() asm volatile ("pause")
#define cpunum() asm_rdmsr(0xC0000103) 

