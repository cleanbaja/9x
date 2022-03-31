#ifndef SYS_TABLES_H
#define SYS_TABLES_H

#include "arch/irq.h"
#include <stdbool.h>

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

#define GDT_KERNEL_CODE_ENTRY 0x00AF9A000000FFFF
#define GDT_KERNEL_DATA_ENTRY 0x008F92000000FFFF
#define GDT_USER_CODE_ENTRY   0x00AFFA000000FFFF
#define GDT_USER_DATA_ENTRY   0x008FF2000000FFFF

struct __attribute__((packed)) gdt
{
  uint64_t null_entry;
  uint64_t kcode_entry;
  uint64_t kdata_entry;
  uint64_t ucode_entry;
  uint64_t udata_entry;
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

// Functions to reload/init the CPU tables
void
init_tables();
void
reload_tables();

// TSS loading helper
void
load_tss(uintptr_t address);

#endif // SYS_TABLES_H
