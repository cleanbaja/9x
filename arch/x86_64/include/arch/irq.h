#ifndef SYS_IRQ_H
#define SYS_IRQ_H

#include <stdbool.h>
#include <stdint.h>

// x86_64 CPU trap frame...
typedef struct __attribute__((packed)) cpu_context
{
  uint64_t r15, r14, r13, r12, r11, r10, r9;
  uint64_t r8, rbp, rdi, rsi, rdx, rcx, rbx;
  uint64_t rax, int_no, ec, rip, cs, rflags;
  uint64_t rsp, ss;
} cpu_ctx_t;

// Represents a IRQ handler, along with all the information needed to dispatch
typedef void (*HandlerFunc)(cpu_ctx_t*, void*);
struct irq_handler
{
  HandlerFunc hnd;
  void* extra_arg;
  bool is_irq;
  bool should_return;
};

void
dump_context(cpu_ctx_t* regs);
void
register_irq_handler(int vec, struct irq_handler h);
int
find_irq_slot(
  struct irq_handler h); // Just like above, but finds a empty slot for the IRQ

// Allocates a IDT vector, for reciving IRQs
int
alloc_irq_vec();

#endif // SYS_IRQ_H
