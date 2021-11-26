#include <sys/tables.h>
#include <lib/log.h>
#include <internal/asm.h>

static struct idt_entry entries[256] = {0};

static struct idt_entry idt_make_entry(void* handler, uint8_t ist) {
  uint64_t address = (uint64_t)handler;
  return (struct idt_entry) {
    .offset_low = (uint16_t)address,
    .selector = 0x08,
    .ist = ist,
    .flags = 0x8e,
    .offset_mid = (uint16_t)(address >> 16),
    .offset_hi = (uint32_t)(address >> 32),
    .reserved = 0
  };
}

void init_idt() {
	for (int i = 0; i < 256; i++) {
		entries[i] = idt_make_entry(asm_dispatch_table[i], 0);
	}

	percpu_flush_idt();
}

void percpu_flush_idt() {
  struct table_ptr table_pointer;
  table_pointer.base = (uint64_t)entries;
  table_pointer.limit = sizeof(entries) - 1;

  asm_load_idt(table_pointer);
}

/*
 * A list of execption messages stolen from the osdev wiki
 * NOTE: I removed some execption that were obsolete
*/

static char *exc_table[] = {
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
    [19] = "SIMD floating point Exception",
    [20] = "Virtualization Exception",
    [30] = "Security Exception"
};

void dump_regs(ctx_t* context) {
	raw_log("Exception #%d (%s)\n", context->int_no, exc_table[context->int_no]);
	raw_log("    Error Code: 0x%x, RIP: 0x%x, RSP: 0x%x\n", context->ec, context->rip, context->rsp);
  raw_log("    RAX: 0x%x, RBX: 0x%x, RCX: 0x%x, RDX: 0x%x\n", context->rax, context->rbx, context->rcx, context->rbx);
  raw_log("    RSI: 0x%x, RDI: 0x%x, RSP: 0x%x, RBP: 0x%x\n", context->rsi, context->rdi, context->rsp, context->rbp);
  raw_log("    R8: 0x%x, R9: 0x%x, R10: 0x%x, R11: 0x%x\n", context->r8, context->r9, context->r10, context->r11);
  raw_log("    R12: 0x%x, R12: 0x%x, R13: 0x%x, R14: 0x%x\n", context->r12, context->r13, context->r13, context->r14);
  raw_log("    R15: 0x%x, CS: 0x%x, SS: 0x%x\n\n", context->r15, context->cs, context->ss);
}

void sys_dispatch_isr(ctx_t* context) {
	if (context->int_no < 32) {
		PANIC(context, NULL);
	}
}

