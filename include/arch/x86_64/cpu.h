#pragma once

#include <stdint.h>

#define AMD64_TSC_AUX 0xC0000103
#define AMD64_EFER    0xC0000080
#define AMD64_PAT     0x277

#define CPU_FEAT_PCID (1ull << 0)

#define has_feat(ft) (cpu_features & ft)
#define cpu_pause()  asm volatile ("pause")
#define cpunum()     cpu_rdmsr(AMD64_TSC_AUX) 

#define CREATE_CR_ASM(N)                                                  \
  static inline uint64_t cpu_read_cr##N(void)                             \
  {                                                                       \
    uint64_t value = 0;                                                   \
    asm volatile("mov %%cr" #N ", %0" : "=r"(value));                     \
    return value;                                                         \
  }                                                                       \
                                                                          \
  static inline void cpu_write_cr##N(uint64_t value)                      \
  {                                                                       \
    asm volatile("mov %0, %%cr" #N ::"a"(value));                         \
  }

static inline uint64_t cpu_rdmsr(uint32_t msr) {
  uint32_t hi = 0, lo = 0;

  asm volatile(
    "rdmsr;"
    : "=a"(lo), "=d"(hi)
    : "c"(msr)
    : "memory");

  return (uint64_t)(((uint64_t)hi << 32) | lo);
}

static inline void cpu_wrmsr(uint32_t msr, uint64_t val) {
  uint32_t lo = val & UINT32_MAX;
  uint32_t hi = (val >> 32) & UINT32_MAX;

  asm volatile(
    "wrmsr;"
    :
    : "c"(msr), "a"(lo), "d"(hi)
    : "memory");
}

CREATE_CR_ASM(0)
CREATE_CR_ASM(2)
CREATE_CR_ASM(3)
CREATE_CR_ASM(4)

// TLB management functions
void tlb_flush_page(int asid, uintptr_t addr);
void tlb_flush_asid(int asid);
void tlb_flush_global();

void cpu_init();
extern uint64_t cpu_features;
