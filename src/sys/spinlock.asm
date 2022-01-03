[bits 64]

section .text

global asm_spinlock_acquire
asm_spinlock_acquire:
      cli
      mov     rax, 1
lock  xchg    [rdi], rax
      cmp     rax, 0

      je      .escape
      pause
      jmp     asm_spinlock_acquire

.escape:
      sti
      ret

global asm_sleeplock_acquire
asm_sleeplock_acquire:
      cli
      mov     r8, 1
lock  xchg    [rdi], r8
      cmp     r8, 0
      je      .escape
      
      xor rcx, rcx
      xor rdx, rdx
      mov rax, rdi
      monitor
      mwait

      nop
      jmp     asm_sleeplock_acquire

.escape:
      sti
      ret

