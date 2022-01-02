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

// IDT Definitions ================================================== 

struct __attribute__((packed)) idt_entry {
  uint16_t offset_low;
  uint16_t selector;
  uint8_t ist;
  uint8_t flags;
  uint16_t offset_mid;
  uint32_t offset_hi;
  uint32_t reserved;
};

typedef struct __attribute__((packed)) cpu_ctx {
	uint64_t r15, r14, r13, r12, r11, r10, r9;
	uint64_t r8, rbp, rdi, rsi, rdx, rcx, rbx;
	uint64_t rax, int_no, ec, rip, cs, rflags;
	uint64_t rsp, ss;
} ctx_t;

void init_idt();
void percpu_flush_idt();

void dump_regs(ctx_t* context);

#endif // SYS_TABLES_H

