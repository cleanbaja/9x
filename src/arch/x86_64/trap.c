#include <arch/trap.h>
#include <lib/print.h>
#include <lib/panic.h>
#include <lvm/lvm_space.h>

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

/*
 * A list of execption messages stolen from the osdev wiki
 * NOTE: I removed some execptions that were obsolete
 */
static char* names[] = { 
  [0] = "Division by Zero",
  [1] = "Debug",
  [2] = "Non Maskable Interrupt",
  [3] = "Breakpoint",
  [4] = "Overflow",
  [5] = "Out of Bounds",
  [6] = "Invalid Opcode",
  [8] = "Double Fault",
  [10] = "Invalid TSS",
  [11] = "Segment not present",
  [12] = "Stack Exception",
  [13] = "General Protection fault",
  [14] = "Page fault",
  [16] = "x87 Floating Point Exception",
  [17] = "Alignment check",
  [18] = "Machine check",
  [19] = "SIMD Floating Point Exception",
  [20] = "Virtualization Exception",
  [29] = "Reserved",
  [30] = "Security Exception"
};

void trap_dump_frame(struct cpu_regs* regs) {
  kprint("Exception #%d (%s)\n", regs->int_no, names[regs->int_no]);
  kprint("    Error Code: 0x%08lx, RIP: 0x%08lx, RSP: 0x%08lx\n",
      regs->ec,
      regs->rip,
      regs->rsp);
  kprint("    RAX: 0x%08lx, RBX: 0x%08lx, RCX: 0x%08lx, RDX: 0x%08lx\n",
      regs->rax,
      regs->rbx,
      regs->rcx,
      regs->rbx);
  kprint("    RSI: 0x%08lx, RDI: 0x%08lx, RSP: 0x%08lx, RBP: 0x%08lx\n",
      regs->rsi,
      regs->rdi,
      regs->rsp,
      regs->rbp);
  kprint("    R8:  0x%08lx, R9:  0x%08lx, R10: 0x%08lx, R11: 0x%08lx\n",
      regs->r8,
      regs->r9,
      regs->r10,
      regs->r11);
  kprint("    R12: 0x%08lx, R12: 0x%08lx, R13: 0x%08lx, R14: 0x%08lx\n",
      regs->r12,
      regs->r13,
      regs->r13,
      regs->r14);
  kprint("    CR2: 0x%08lx, R15: 0x%08lx, CS:  0x%08lx, SS:  0x%08lx\n\n",
      cpu_read_cr2(),
      regs->r15,
      regs->cs,
      regs->ss);
}

void handle_trap(struct cpu_regs* context) {
  // Handle page faults
  if (context->int_no == 14) {
    enum lvm_fault_flags vf = LVM_FAULT_NONE;
    if (context->ec & (1 << 0))
      vf |= LVM_FAULT_PROTECTION;
    if (context->ec & (1 << 4))
      vf |= LVM_FAULT_EXEC;
    if (context->ec & (1 << 1))
      vf |= LVM_FAULT_WRITE;

    uintptr_t address = cpu_read_cr2();
    if (lvm_fault(&kspace, address, vf))
      return;
  }

  panic(context, NULL);
}

