[bits 64]
section .text

; Defined in src/kern/syscall.c...
extern syscall_table
extern nr_syscalls

; The syscall instruction starts off here
global asm_syscall_entry
asm_syscall_entry:
  ; First off, make sure the syscall number isn't out of range
  cmp rax, [rel nr_syscalls]
  jae .unknown_syscall

  ; Next, swap stacks and GSBASE, before pushing a stack frame
  swapgs
  mov [gs:30], rsp       ; gs.user_stack = rsp
  mov rsp, [gs:22]       ; rsp = gs.kernel_stack
  sti

  ; Create a dummy interrupt frame
  push qword 0x38         ; user data segment
  push qword [gs:30]      ; saved stack
  push r11                ; saved rflags
  push qword 0x40         ; user code segment
  push rcx                ; instruction pointer
  push 0                  ; error code
  push 0                  ; int no

  ; Push everything else
  push rax
  push rbx
  push rcx
  push rdx
  push rsi
  push rdi
  push rbp
  push r8
  push r9
  push r10
  push r11
  push r12
  push r13
  push r14
  push r15

  ; Index into the jump table, and call the proper syscall handler
  mov rdi, rsp
  lea rbx, [rel syscall_table]
  call [rbx + rax * 8]

  ; Pop the registers, and clean the remaining mess
  pop r15
  pop r14
  pop r13
  pop r12
  pop r11
  pop r10
  pop r9
  pop r8
  pop rbp
  pop rdi
  pop rsi
  pop rdx
  pop rcx
  pop rbx
  pop rax
  add rsp, 56

  ; Disable interrupts, before restoring the remaining context
  cli
  mov rax, qword [gs:54]  ; rax = gs.errno
  mov rsp, qword [gs:30]  ; rsp = gs.user_stack
  swapgs
  o64 sysret

.unknown_syscall:
  mov rax, 1051 ; ENOSYS
  o64 sysret

