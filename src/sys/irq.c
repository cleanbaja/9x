#include <internal/asm.h>
#include <lib/log.h>
#include <sys/apic.h>
#include <sys/irq.h>

static struct irq_handler handlers[256] = { 0 };
static int last_vector = 64; // Start over the I/O APIC window, 
                             // so we don't overlap.

void
register_irq_handler(int vec, struct irq_handler h)
{
  if (vec >= 255) {
    return; // Out of bounds access
  } else {
    handlers[vec] = h;
  }
}

int
alloc_irq_vec()
{
  // Make sure we don't give out reserved vectors, 
  // which are from 250-255 and 0-63
  if (last_vector > 249 || last_vector < 64) {
    log("sys/irq: (WARN) Unable to find any free vectors!");
    return -1;
  } else {
    return last_vector++;
  }
}

int
find_irq_slot(struct irq_handler h)
{
  int slot = alloc_irq_vec();
  if (slot == -1) {
    return -1;
  } else {
    register_irq_handler(slot, h);
    return slot;
  }
}

cpu_ctx_t*
sys_dispatch_isr(cpu_ctx_t* context)
{
  uint32_t vec = context->int_no;

  // If there is a CPU exception with no handler, then panic.
  if (vec < 32 && !handlers[vec].hnd) {
    PANIC(context, NULL);
  }

  // Call the associated handler (if present)
  if (handlers[vec].hnd) {
    handlers[vec].hnd(context, handlers[context->int_no].extra_arg);
  }

  // Take the appropriate action based on the handler
  if (handlers[vec].is_irq) {
    apic_eoi();
  }
  if (handlers[vec].should_return) {
    return context;
  } else {
    __asm__ volatile("cli; hlt");
    for (;;) {
      __asm__ volatile("hlt");
    }
  }
}

/*
 * A list of execption messages stolen from the osdev wiki
 * NOTE: I removed some execption that were obsolete
 */
static char* exc_table[] = { [0] = "Division by Zero",
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
                             [19] = "SIMD floating point Exception",
                             [20] = "Virtualization Exception",
                             [30] = "Security Exception" };

void
dump_context(cpu_ctx_t* regs)
{
  raw_log("Exception #%d (%s)\n", regs->int_no, exc_table[regs->int_no]);
  raw_log("    Error Code: 0x%08lx, RIP: 0x%08lx, RSP: 0x%08lx\n",
          regs->ec,
          regs->rip,
          regs->rsp);
  raw_log("    RAX: 0x%08lx, RBX: 0x%08lx, RCX: 0x%08lx, RDX: 0x%08lx\n",
          regs->rax,
          regs->rbx,
          regs->rcx,
          regs->rbx);
  raw_log("    RSI: 0x%08lx, RDI: 0x%08lx, RSP: 0x%08lx, RBP: 0x%08lx\n",
          regs->rsi,
          regs->rdi,
          regs->rsp,
          regs->rbp);
  raw_log("    R8:  0x%08lx, R9:  0x%08lx, R10: 0x%08lx, R11: 0x%08lx\n",
          regs->r8,
          regs->r9,
          regs->r10,
          regs->r11);
  raw_log("    R12: 0x%08lx, R12: 0x%08lx, R13: 0x%08lx, R14: 0x%08lx\n",
          regs->r12,
          regs->r13,
          regs->r13,
          regs->r14);
  raw_log("    R15: 0x%08lx, CS:  0x%08lx, SS:  0x%08lx\n\n",
          regs->r15,
          regs->cs,
          regs->ss);

  if (regs->int_no == 14) {
    // Print extra information in the case of a page fault
    uint64_t cr2_val = asm_read_cr2();
    raw_log("Linear Address: 0x%lx\nConditions:\n", cr2_val);

    // See Intel x86 SDM Volume 3a Chapter 4.7
    uint64_t error_code = regs->ec;
    if (((error_code) & (1 << (0))))
      raw_log("    - Page level protection violation\n");
    else
      raw_log("    - Non-present page\n");

    if (((error_code) & (1 << (1))))
      raw_log("    - Write\n");
    else
      raw_log("    - Read\n");

    if (((error_code) & (1 << (2))))
      raw_log("    - User access\n");
    else
      raw_log("    - Supervisor access\n");

    if (((error_code) & (1 << (3))))
      raw_log("    - Reserved bit set\n");

    if (((error_code) & (1 << (4))))
      raw_log("    - Instruction fetch\n");

    if (((error_code) & (1 << (5))))
      raw_log("    - Protection key violation\n");

    if (((error_code) & (1 << (15))))
      raw_log("    - SGX violation\n");

    raw_log("\n");
  }
}
