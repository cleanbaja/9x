#include <arch/trap.h>

#define GDT_KERN_CODE 0x00af9b000000ffff
#define GDT_KERN_DATA 0x00af93000000ffff
#define GDT_USER_CODE 0x00affb000000ffff
#define GDT_USER_DATA 0x00aff3000000ffff

struct {
  uint64_t ents[5];
} gdt_table = {0};

struct __attribute__((packed)) idt_entry {
   uint16_t offset_lo;
   uint16_t selector;
   uint8_t  ist;
   uint8_t  attrib;
   uint16_t offset_mid;
   uint32_t offset_hi;
   uint32_t rsvd;
};

struct idt_entry idt_table[256] = {0};
extern uintptr_t vector_table[];

static void gdt_init() {
  // Fill in the GDT
  gdt_table.ents[1] = GDT_KERN_CODE;
  gdt_table.ents[2] = GDT_KERN_DATA;
  gdt_table.ents[3] = GDT_USER_DATA;
  gdt_table.ents[4] = GDT_USER_CODE;

  // Create a GDT descriptor
  struct __attribute__((packed)) {
    uint16_t limit;
    uint64_t base;
  } gdt_table_pointer = { 
    .limit = sizeof(gdt_table) - 1, 
    .base = (uintptr_t)&gdt_table
  };

  // Then load it in
  asm volatile (
    "lgdt %0\n\t"
    "leaq 1f(%%rip), %%rbx\n\t"
    "push $0x08\n\t"
    "push %%rbx\n\t"
    "lretq\n"
    "1:\n\t"
    "movq $0x10, %%rbx\n\t"
    "mov %%ebx, %%ds\n\t"
    "mov %%ebx, %%es\n\t"
    "mov %%ebx, %%fs\n\t"
    "mov %%ebx, %%gs\n\t"
    "mov %%ebx, %%ss\n\t"

    :: "m"(gdt_table_pointer) : "memory", "rbx");
}

void trap_init() {
  // Setup the GDT...
  gdt_init();

  // Fill in the IDT entries
  for (int i = 0; i < 256; i++) {
    idt_table[i].offset_lo  = (uint16_t)vector_table[i];
    idt_table[i].offset_mid = (uint16_t)(vector_table[i] >> 16);
    idt_table[i].offset_hi  = (uint32_t)(vector_table[i] >> 32);

    idt_table[i].selector = 0x08;
    idt_table[i].ist = 0;
    idt_table[i].attrib = 0x8E;
  }

  // Finally, load the IDT
  struct __attribute__((packed)) {
    uint16_t limit;
    uint64_t base;
  } idt_table_pointer = { 
    .limit = sizeof(idt_table) - 1, 
    .base = (uintptr_t)&idt_table
  };

  asm volatile ("lidt %0" :: "m"(idt_table_pointer));
}

void handle_trap(struct cpu_regs* context) {
  // Halt for now
  for (;;) {
    asm volatile ("cli; hlt");
  }
}

