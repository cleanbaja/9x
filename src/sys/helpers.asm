[bits 64]

section .text

global asm_load_gdt
asm_load_gdt:
  lgdt [rdi]
  
  mov ax, 0x10

  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax
  mov ss, ax

  pop rdi
  mov rax, 0x08
 
  push rax
  push rdi
  retfq

  
