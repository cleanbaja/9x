#include <arch/trap.h>

// TODO(cleanbaja): use a percpu stack instead
char __stack[0x10000] = {0};
extern uintptr_t vector_table[];

void trap_init() {
  // Set the stack pointer for exceptions
  asm volatile ("msr SPSel, 1; mov sp, %0; msr SPSel, 0;" :: "r"((uintptr_t)__stack + 0x10000));

  // Load VBAR_EL1 with our vector table
  asm volatile ("msr vbar_el1, %0" :: "r"(vector_table));
}

void handle_trap(struct cpu_regs* context) {
  // Halt for now
  for (;;) {
    asm volatile ("msr DAIFSet, #15; wfi");
  }
}

