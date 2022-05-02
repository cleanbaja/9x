#ifndef ARCH_IRQ_H
#define ARCH_IRQ_H

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
  char* name;
  uint32_t gsi;
  bool is_irq, should_return;
};

struct irq_handler* request_irq(char* desc, int* slot);
struct irq_handler* get_handler(int slot);

// Allocates a raw IDT vector, for reciving IRQs
int alloc_irq_vec();

#endif // ARCH_IRQ_H
