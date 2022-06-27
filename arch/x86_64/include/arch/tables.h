#ifndef ARCH_TABLES_H
#define ARCH_TABLES_H

#include "arch/irqchip.h"

#ifndef ARCH_INTERNAL
#error "Attempt to include internal code in a generic code file"
#endif

struct __attribute__((packed)) table_ptr
{
  uint16_t limit;
  uint64_t base;
};

struct __attribute__((packed)) tss_entry
{
  uint16_t length;
  uint16_t base_low16;
  uint8_t base_mid8;
  uint8_t flags1;
  uint8_t flags2;
  uint8_t base_high8;
  uint32_t base_upper32;
  uint32_t reserved;
};

#ifdef LIMINE_EARLYCONSOLE
  #define GDT_KERNEL_CODE  0x28
  #define GDT_KERNEL_DATA  0x30
  #define GDT_USER_CODE    0x40
  #define GDT_USER_DATA    0x38
  #define GDT_TSS_SELECTOR 0x48
#else
  #define GDT_KERNEL_CODE  0x08
  #define GDT_KERNEL_DATA  0x10
  #define GDT_USER_CODE    0x20
  #define GDT_USER_DATA    0x18
  #define GDT_TSS_SELECTOR 0x28
#endif

struct __attribute__((packed)) gdt
{
  uint64_t null_entry;

#ifdef LIMINE_EARLYCONSOLE

  // 16-bit kernel code/data
  uint64_t ocode_entry;
  uint64_t odata_entry;

  // 32-bit kernel code/data
  uint64_t lcode_entry;
  uint64_t ldata_entry;

#endif // LIMINE_EARLYCONSOLE

  // 64-bit kernel code/data
  uint64_t kcode_entry;
  uint64_t kdata_entry;

  // 64-bit user data/code (reversed for syscall)
  uint64_t udata_entry;
  uint64_t ucode_entry;

  struct tss_entry tss;
};

struct __attribute__((packed)) idt_entry
{
  uint16_t offset_low;
  uint16_t selector;
  uint8_t ist;
  uint8_t flags;
  uint16_t offset_mid;
  uint32_t offset_hi;
  uint32_t reserved;
};

struct __attribute__((packed)) tss
{
  uint32_t reserved;
  uint64_t rsp0;
  uint64_t rsp1;
  uint64_t rsp2;
  uint32_t reserved1;
  uint32_t reserved2;
  uint64_t ist1;
  uint64_t ist2;
  uint64_t ist3;
  uint64_t ist4;
  uint64_t ist5;
  uint64_t ist6;
  uint64_t ist7;
  uint64_t reserved3;
  uint16_t reserved4;
  uint16_t iopb;
};

// Stage to create the CPU tables (aka the GDT and IDT)
void tables_install();

// Helpers for loading the TSS and dumping CPU context
void load_tss(uintptr_t address);
void dump_context(struct cpu_context* regs);

#endif // ARCH_TABLES_H
