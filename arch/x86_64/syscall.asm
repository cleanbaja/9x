[bits 64]
section .data
nr_syscalls equ ((jump_table.end - jump_table) / 8)

align 16
jump_table:
  extern syscall_debuglog
  dq syscall_debuglog
  extern syscall_archctl
  dq syscall_archctl
.end:

section .text

; The syscall instruction starts off here
global asm_syscall_entry
asm_syscall_entry:
  ; Make sure the syscall isn't too big or unknown
  cmp rax, nr_syscalls
  jae .unknown_syscall

  ; First off, swap stacks before we do anything else
  swapgs
  mov [gs:56], rsp       ; gs.user_stack = rsp
  mov rsp, [gs:48]       ; rsp = gs.kernel_stack 
  sti

  ; Create a dummy interrupt frame
  push qword 0x1b         ; user data segment
  push qword [gs:56]      ; saved stack
  push r11                ; saved rflags
  push qword 0x23         ; user code segment 
  push rcx                ; instruction pointer
  push 0                  ; error code
  push 0                  ; int no

  ; Push everything else
  push rax
  push rbx
  push rcx
  push rdx
  push rbp
  push rdi
  push rsi
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
  lea rbx, [rel jump_table]
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
  pop rsi
  pop rdi
  pop rbp
  pop rdx
  pop rcx
  pop rbx
  pop rax
  add rsp, 56

  ; Disable interrupts, before restoring the remaining context
  cli
  mov rdx, qword [gs:40] ; rdx = gs.errno
  mov rsp, qword [gs:16]  ; rsp = gs.user_stack
  swapgs
  o64 sysret

.unknown_syscall:
  mov rdx, 134 ; ENOSYS
  o64 sysret

