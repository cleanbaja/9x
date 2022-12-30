#pragma once

#include <stdint.h>

struct cpu_regs {
  uint64_t r15, r14, r13, r12, r11, r10, r9;
  uint64_t r8, rbp, rsi, rdi, rdx, rcx, rbx;
  uint64_t rax, int_no, ec, rip, cs, rflags;
  uint64_t rsp, ss;
} __attribute__((packed)); 

void trap_init();
void trap_dump_frame(struct cpu_regs* regs);

