#ifndef INTERNAL_ASM_H
#define INTERNAL_ASM_H

#include <stdbool.h>
#include <stdint.h>

// General asm routines
#define asm_halt()                                                             \
  ({                                                                           \
    __asm__ volatile("cli");                                                   \
    for (;;) {                                                                 \
      __asm__ volatile("hlt");                                                 \
    }                                                                          \
  })

#define asm_invlpg(k) ({ __asm__ volatile("invlpg %0" ::"m"(k) : "memory"); })
#define asm_swapgs()  ({__asm__ volatile("swapgs" ::: "memory");})

// GDT/IDT asm routines
extern void* asm_dispatch_table[256];
extern void
asm_load_gdt(void* g, uint16_t codeseg, uint16_t dataseg);
#define asm_load_idt(ptr) __asm__ volatile("lidt %0" ::"m"(ptr))

// CR0-4 & MSR asm routines
#define ASM_MAKE_CRN(N)                                                        \
  static inline uint64_t asm_read_cr##N(void)                                  \
  {                                                                            \
    uint64_t value = 0;                                                        \
    __asm__ volatile("mov %%cr" #N ", %0" : "=r"(value));                      \
    return value;                                                              \
  }                                                                            \
                                                                               \
  static inline void asm_write_cr##N(uint64_t value)                           \
  {                                                                            \
    __asm__ volatile("mov %0, %%cr" #N ::"a"(value));                          \
  }

ASM_MAKE_CRN(0)
ASM_MAKE_CRN(1)
ASM_MAKE_CRN(2)
ASM_MAKE_CRN(3)
ASM_MAKE_CRN(4)
ASM_MAKE_CRN(8)

#define IA32_EFER           0xC0000080
#define IA32_KERNEL_GS_BASE 0xC0000102
#define IA32_USER_GS_BASE   0xC0000101
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

  __asm__ volatile("mov %[msr], %%ecx;"
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

  __asm__ volatile(
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

// Syscall entrypoint
extern void asm_syscall_entry();

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
    __asm__ volatile("in %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint16_t asm_inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile("in %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint32_t asm_ind(uint16_t port) {
    uint32_t ret;
    __asm__ volatile("in %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void asm_outb(uint16_t port, uint8_t data) {
    __asm__ volatile("out %0, %1" :: "a"(data), "Nd"(port));
}

static inline void asm_outw(uint16_t port, uint16_t data) {
    __asm__ volatile("out %0, %1" :: "a"(data), "Nd"(port));
}

static inline void asm_outd(uint16_t port, uint32_t data) {
    __asm__ volatile("out %0, %1" :: "a"(data), "Nd"(port));
}

// A quick way to get the current CPUs number
static inline uint32_t __get_cpunum() {
  uint32_t ret;
  asm volatile("mov %%gs:0, %0" : "=r"(ret));
  return ret;
}
#endif // INTERNAL_ASM_H
