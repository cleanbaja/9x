[bits 64]
section .text

%macro asm_push_regs 0
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
%endmacro

%macro asm_pop_regs 0
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
%endmacro

%macro INTR_NAME 1
dq __intr_%1
%endmacro

%macro INTR_CREATE 1
__intr_%1:
    push qword 0  ; Push a dummy code, since cpu pushes none
    push qword %1
    jmp asm_handle_trap
%endmacro

%macro INTR_CREATE_ERR 1
__intr_%1:
    push qword %1
    jmp asm_handle_trap
%endmacro

global asm_load_gdt
asm_load_gdt:
  lgdt [rdi]
  lea rax, [rel .update_segments]
  push rsi
  push rax
  retfq

.update_segments:
  mov rax, rdx
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax
  mov ss, ax
  ret

extern sys_dispatch_isr
asm_handle_trap:
    cld
    asm_push_regs

    mov rdi, rsp
    call sys_dispatch_isr
    mov rsp, rax

    asm_pop_regs
    add rsp, 16  ; Pop the CPU pushed error code and int-no

    iretq

INTR_CREATE       0
INTR_CREATE       1
INTR_CREATE       2
INTR_CREATE       3
INTR_CREATE       4
INTR_CREATE       5
INTR_CREATE       6
INTR_CREATE       7
INTR_CREATE_ERR   8
INTR_CREATE       9
INTR_CREATE_ERR   10
INTR_CREATE_ERR   11
INTR_CREATE_ERR   12
INTR_CREATE_ERR   13
INTR_CREATE_ERR   14
INTR_CREATE       15
INTR_CREATE       16
INTR_CREATE_ERR   17
INTR_CREATE       18
INTR_CREATE       19
INTR_CREATE       20
INTR_CREATE       21
INTR_CREATE       22
INTR_CREATE       23
INTR_CREATE       24
INTR_CREATE       25
INTR_CREATE       26
INTR_CREATE       27
INTR_CREATE       28
INTR_CREATE       29
INTR_CREATE_ERR   30
INTR_CREATE       31

%assign i 32
%rep 223

INTR_CREATE i
%assign i i+1

%endrep

__intr_255:
  ret


global sched_spinup
sched_spinup:
  mov rsp, rdi

  asm_pop_regs
  add rsp, 16  ; Pop the CPU pushed error code and int-no

  iretq

section .data

global asm_dispatch_table
asm_dispatch_table:
%assign i 0
%rep 256

INTR_NAME i
%assign i i+1

%endrep
 
