#include <internal/asm.h>
#include <lib/log.h>
#include <sys/apic.h>
#include <sys/tables.h>

static struct idt_entry entries[256] = { 0 };
static struct handler handlers[256] = { 0 };

void
idt_set_handler(struct handler h, int vector)
{
  handlers[vector] = h;
}

static struct idt_entry
idt_make_entry(void* handler, uint8_t ist)
{
  uint64_t address = (uint64_t)handler;
  return (struct idt_entry){ .offset_low = (uint16_t)address,
                             .selector = 0x08,
                             .ist = ist,
                             .flags = 0x8e,
                             .offset_mid = (uint16_t)(address >> 16),
                             .offset_hi = (uint32_t)(address >> 32),
                             .reserved = 0 };
}

void
init_idt()
{
  for (int i = 0; i < 256; i++) {
    entries[i] = idt_make_entry(asm_dispatch_table[i], 0);
  }

  percpu_flush_idt();
}

void
percpu_flush_idt()
{
  struct table_ptr table_pointer;
  table_pointer.base = (uint64_t)entries;
  table_pointer.limit = sizeof(entries) - 1;

  asm_load_idt(table_pointer);
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
dump_regs(ctx_t* context)
{
  raw_log("Exception #%d (%s)\n", context->int_no, exc_table[context->int_no]);
  raw_log("    Error Code: 0x%08lx, RIP: 0x%08lx, RSP: 0x%08lx\n",
          context->ec,
          context->rip,
          context->rsp);
  raw_log("    RAX: 0x%08lx, RBX: 0x%08lx, RCX: 0x%08lx, RDX: 0x%08lx\n",
          context->rax,
          context->rbx,
          context->rcx,
          context->rbx);
  raw_log("    RSI: 0x%08lx, RDI: 0x%08lx, RSP: 0x%08lx, RBP: 0x%08lx\n",
          context->rsi,
          context->rdi,
          context->rsp,
          context->rbp);
  raw_log("    R8:  0x%08lx, R9:  0x%08lx, R10: 0x%08lx, R11: 0x%08lx\n",
          context->r8,
          context->r9,
          context->r10,
          context->r11);
  raw_log("    R12: 0x%08lx, R12: 0x%08lx, R13: 0x%08lx, R14: 0x%08lx\n",
          context->r12,
          context->r13,
          context->r13,
          context->r14);
  raw_log("    R15: 0x%08lx, CS:  0x%08lx, SS:  0x%08lx\n\n",
          context->r15,
          context->cs,
          context->ss);

  if (context->int_no == 14) {
    // Print extra information in the case of a page fault
    uint64_t cr2_val = asm_read_cr2();
    raw_log("Linear Address: 0x%lx\nConditions:\n", cr2_val);

    // See Intel x86 SDM Volume 3a Chapter 4.7
    uint64_t error_code = context->ec;
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

void
sys_dispatch_isr(ctx_t* context)
{
  if (context->int_no < 32) {
    PANIC(context, NULL);
  }

  if (handlers[context->int_no].func) {
    handlers[context->int_no].func(context);
  }

  if (handlers[context->int_no].is_irq) {
    apic_eoi();
  }
}
