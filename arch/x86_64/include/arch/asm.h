#ifndef INTERNAL_ASM_H
#define INTERNAL_ASM_H

#include <stdbool.h>
#include <stdint.h>

// General asm routines
#define asm_halt(enable_intrs)                                                 \
  ({                                                                           \
    if (enable_intrs) {                                                        \
      asm volatile ("sti");                                                    \
    } else {                                                                   \
      asm volatile("cli");                                                     \
    }                                                                          \
    for (;;) {                                                                 \
      asm volatile("hlt");                                                     \
    }                                                                          \
  })
#define asm_enable_intr()  ({ asm volatile ("sti"); })
#define asm_disable_intr() ({ asm volatile ("cli"); })
#define asm_invlpg(k) ({ asm volatile("invlpg %0" ::"m"(k) : "memory"); })
#define asm_swapgs()  ({ asm volatile("swapgs" ::: "memory"); })

// CR0-4 & MSR asm routines
#define ASM_MAKE_CRN(N)                                                        \
  static inline uint64_t asm_read_cr##N(void)                                  \
  {                                                                            \
    uint64_t value = 0;                                                        \
    asm volatile("mov %%cr" #N ", %0" : "=r"(value));                      \
    return value;                                                              \
  }                                                                            \
                                                                               \
  static inline void asm_write_cr##N(uint64_t value)                           \
  {                                                                            \
    asm volatile("mov %0, %%cr" #N ::"a"(value));                          \
  }

ASM_MAKE_CRN(0)
ASM_MAKE_CRN(2)
ASM_MAKE_CRN(3)
ASM_MAKE_CRN(4)
ASM_MAKE_CRN(8)

#define IA32_EFER           0xC0000080
#define IA32_KERNEL_GS_BASE 0xC0000102
#define IA32_GS_BASE        0xC0000101
#define IA32_FS_BASE        0xC0000100
#define IA32_STAR           0xC0000081
#define IA32_LSTAR          0xC0000082
#define IA32_SFMASK         0xC0000084
#define IA32_TSC_AUX        0xC0000103

static inline uint64_t
asm_rdmsr(uint32_t msr)
{
  uint32_t msrlow;
  uint32_t msrhigh;

  asm volatile("mov %[msr], %%ecx;"
                   "rdmsr;"
                   "mov %%eax, %[msrlow];"
                   "mov %%edx, %[msrhigh];"
                   : [msrlow] "=g"(msrlow), [msrhigh] "=g"(msrhigh)
                   : [msr] "g"(msr)
                   : "eax", "ecx", "edx");

  uint64_t msrval = ((uint64_t)msrhigh << 32) | msrlow;
  return msrval;
}

static inline void
asm_wrmsr(uint32_t msr, uint64_t val)
{
  uint32_t msrlow = val & UINT32_MAX;
  uint32_t msrhigh = (val >> 32) & UINT32_MAX;

  asm volatile(
    "mov %[msr], %%ecx;"
    "mov %[msrlow], %%eax;"
    "mov %[msrhigh], %%edx;"
    "wrmsr;"
    :
    : [msr] "g"(msr), [msrlow] "g"(msrlow), [msrhigh] "g"(msrhigh)
    : "eax", "ecx", "edx");
}

static inline void asm_wrxcr(uint32_t reg, uint64_t value) {
    uint32_t edx = value >> 32;
    uint32_t eax = (uint32_t)value;
    asm volatile ("xsetbv"
                  :
                  : "a" (eax), "d" (edx), "c" (reg)
                  : "memory");
}

// Checks if interrupts are enabled
static inline bool asm_check_intr() {
  volatile uint64_t rflags = 0;
  asm volatile ("pushf\n\t"
                "pop %0\n\t"
                : "=rm" (rflags) :: "memory", "cc");

  return ((rflags & 0x200) != 0);
}

// Reading of the TSC
static inline uint64_t asm_rdtsc() {
    uint32_t edx, eax;
    asm volatile ("rdtsc"
                  : "=a" (eax), "=d" (edx) :: "memory");
    return ((uint64_t)edx << 32) | eax;
}

// ASM Port I/O
static inline uint8_t asm_inb(uint16_t port) {
    uint8_t ret;
    asm volatile("in %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint16_t asm_inw(uint16_t port) {
    uint16_t ret;
    asm volatile("in %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint32_t asm_ind(uint16_t port) {
    uint32_t ret;
    asm volatile("in %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void asm_outb(uint16_t port, uint8_t data) {
    asm volatile("out %0, %1" :: "a"(data), "Nd"(port));
}

static inline void asm_outw(uint16_t port, uint16_t data) {
    asm volatile("out %0, %1" :: "a"(data), "Nd"(port));
}

static inline void asm_outd(uint16_t port, uint32_t data) {
    asm volatile("out %0, %1" :: "a"(data), "Nd"(port));
}

#endif // INTERNAL_ASM_H
