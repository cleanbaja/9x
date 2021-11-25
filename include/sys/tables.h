#ifndef SYS_TABLES_H
#define SYS_TABLES_H

#include <stdint.h>

struct __attribute__((packed)) table_ptr {
  uint16_t limit;
  uint64_t base;
};

// GDT Definitions ================================================== 

#define GDT_KERNEL_CODE_ENTRY 0x00AF9A000000FFFF
#define GDT_KERNEL_DATA_ENTRY 0x008F92000000FFFF
#define GDT_USER_CODE_ENTRY   0x00AFFA000000FFFF
#define GDT_USER_DATA_ENTRY   0x008FF2000000FFFF

struct __attribute__((packed)) gdt {
  uint64_t null_entry;
  uint64_t kcode_entry;
  uint64_t kdata_entry;
  uint64_t ucode_entry;
  uint64_t udata_entry;
};

void init_gdt();
void percpu_flush_gdt();

extern void asm_load_gdt(struct table_ptr* g);

// IDT Definitions ================================================== 

#endif // SYS_TABLES_H

