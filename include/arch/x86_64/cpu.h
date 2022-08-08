#pragma once

#include <stdint.h>

static inline uint64_t asm_rdmsr(uint32_t msr) {
  uint32_t hi = 0, lo = 0;

  asm volatile(
    "mov %2, %%ecx;"
    "rdmsr;"
    "mov %%eax, %1;"
    "mov %%edx, %0;"
    : "=g"(lo), "=g"(hi)
    : "g"(msr)
    : "eax", "ecx", "edx");

  uint64_t value = ((uint64_t)hi << 32) | lo;
  return value;
}

static inline void asm_wrmsr(uint32_t msr, uint64_t val) {
  uint32_t lo = val & UINT32_MAX;
  uint32_t hi = (val >> 32) & UINT32_MAX;

  asm volatile(
    "mov %0, %%ecx;"
    "mov %1, %%eax;"
    "mov %2, %%edx;"
    "wrmsr;"
    :
    : "g"(msr), "g"(lo), "g"(hi)
    : "eax", "ecx", "edx");
}

#define cpu_pause() asm volatile ("pause")
#define cpunum() asm_rdmsr(0xC0000103) 

