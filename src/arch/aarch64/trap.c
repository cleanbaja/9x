#include <arch/trap.h>
#include <lib/print.h>
#include <lib/panic.h>
#include <arch/pmap.h>

// TODO(cleanbaja): use a percpu stack instead
static char exc_stack[0x10000] = {0};
extern uintptr_t vector_table[];

void trap_init() {
  // Set the stack pointer for exceptions
  uintptr_t limine_stack = 0;
  asm volatile ("mov %0, sp" : "=r"(limine_stack));
  asm volatile ("mov sp, %0" :: "r"(limine_stack + LVM_HIGHER_HALF));
  asm volatile ("msr SPSel, 1; mov sp, %0; msr SPSel, 0;" :: "r"((uintptr_t)exc_stack + 0x10000));

  // Load VBAR_EL1 with our vector table
  asm volatile ("msr vbar_el1, %0" :: "r"(vector_table));
}

__attribute__((no_sanitize("undefined")))
void trap_dump_frame(struct cpu_regs* regs) {
  // TODO(cleanbaja): create names table for aarch64
  kprint("Exception #%d (IRQ/FIQ/SError/Synch):\n", regs->ec);

  for (int i = 0; i < 28; i += 4) {
    kprint("\tX%d: 0x%08lx, X%d: 0x%08lx, X%d: 0x%08lx, X%d: 0x%08lx\n",
        i, regs->x[i],
	i+1, regs->x[i+1],
	i+2, regs->x[i+2],
	i+3, regs->x[i+3]);
  }
  
  kprint("\tX29: 0x%08lx, LNK: 0x%08lx, IP: 0x%08lx, SP: 0x%08lx\n",
      regs->x[29],
      regs->x[30],
      regs->ip,
      regs->sp);

  kprint("\tPSTATE: 0x%08lx, ESR: 0x%08lx\n", regs->pstate, regs->esr);  
}


void handle_trap(struct cpu_regs* context) {
  panic(context, NULL);
}

