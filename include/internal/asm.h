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

#define asm_invlpg(k)                                                          \
  ({ __asm__ volatile("invlpg (%0)" : : "r"(k) : "memory"); })

#define INVL_ADDR 0
#define INVL_PCID 1

static inline void
asm_invpcid(uint64_t mode, int pcid, uintptr_t addr)
{
  struct
  {
    uint64_t pcid;
    uintptr_t address;
  } invl_des;

  invl_des.pcid = pcid;
  invl_des.address = addr;

  __asm__ volatile("invpcid %1, %0" : : "r"(mode), "m"(invl_des) : "memory");
}

// GDT/IDT asm routines
extern void* asm_dispatch_table[256];
extern void
asm_load_gdt(void* g);
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

#define IA32_EFER 0xC0000080
#define IA32_KERNEL_GS_BASE 0xC0000102

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

// Locking asm functions...

// Uses 'pause' between checks (better for locks held shorter)
extern void
asm_spinlock_acquire(
  volatile int* lock);

// Uses 'monitor/mwait' between checks (better for locks held longer)
extern void
asm_sleeplock_acquire(volatile int* lock);

// Reading of the TSC
static inline uint64_t asm_rdtsc() {
    uint32_t edx, eax;
    asm volatile ("rdtsc"
                  : "=a" (eax), "=d" (edx));
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

#endif // INTERNAL_ASM_H
