.section .text

.macro pushall
  push %rax
  push %rbx
  push %rcx
  push %rdx
  push %rdi
  push %rsi
  push %rbp
  push %r8
  push %r9
  push %r10
  push %r11
  push %r12
  push %r13
  push %r14
  push %r15
.endm

.macro popall
  pop %rax
  pop %rbx
  pop %rcx
  pop %rdx
  pop %rdi
  pop %rsi
  pop %rbp
  pop %r8
  pop %r9
  pop %r10
  pop %r11
  pop %r12
  pop %r13
  pop %r14
  pop %r15
.endm

.macro mkirq num
__intr_\num:
.if !(\num == 8 || (\num >= 10 && \num <= 14) || \num == 17 || \num == 21 || \num == 29 || \num == 30)
  pushq $0
.endif
  pushq $\num
  jmp common_handler
.endm

common_handler:
  cld

  pushall
  mov %rsp, %rdi

  .extern handle_trap
  call handle_trap

  popall
  addq $16, %rsp
  iretq

.altmacro
.macro mklabel num
    .section .text
    mkirq \num

    .section .data
    .quad __intr_\num
.endm

.section .data
.global vector_table

vector_table:
.set i,0
.rept 256
    mklabel %i
    .set i, i+1
.endr

