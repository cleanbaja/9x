.section .text

.macro pushall
  stp x0, x1, [sp, #0]
  stp x2, x3, [sp, #16]
  stp x4, x5, [sp, #32]
  stp x6, x7, [sp, #48]
  stp x8, x9, [sp, #64]
  stp x10, x11, [sp, #80]
  stp x12, x13, [sp, #96]
  stp x14, x15, [sp, #112]
  stp x16, x17, [sp, #128]
  stp x18, x19, [sp, #144]
  stp x20, x21, [sp, #160]
  stp x22, x23, [sp, #176]
  stp x24, x25, [sp, #192]
  stp x26, x27, [sp, #208]
  stp x28, x29, [sp, #224]
  str x30, [sp, #240]
.endm

.macro popall
  ldp x0, x1, [sp, #0]
  ldp x2, x3, [sp, #16]
  ldp x4, x5, [sp, #32]
  ldp x6, x7, [sp, #48]
  ldp x8, x9, [sp, #64]
  ldp x10, x11, [sp, #80]
  ldp x12, x13, [sp, #96]
  ldp x14, x15, [sp, #112]
  ldp x16, x17, [sp, #128]
  ldp x18, x19, [sp, #144]
  ldp x20, x21, [sp, #160]
  ldp x22, x23, [sp, #176]
  ldp x24, x25, [sp, #192]
  ldp x26, x27, [sp, #208]
  ldp x28, x29, [sp, #224]
  ldr x30, [sp, #240]
.endm

.macro make_exc_handler type
common_handler_\type:
  // Create enough space on the stack for the frame
  sub sp, sp, #288

  // Save all registers 
  pushall

  // Save the stack pointer
  mrs x1, sp_el0
  str x1, [sp, #248] 

  // Save PSTATE, Type, ELR (faulting IP) and ESR
  mrs x1, spsr_el1
  str x1, [sp, #256]
  mrs x1, elr_el1
  str x1, [sp, #264]
  mov x1, \type
  str x1, [sp, #272]
  mrs x1, esr_el1
  str x1, [sp, #280]

  mov x1, xzr
  mov x0, sp

  .extern handle_trap
  bl handle_trap

  // Restore PSTATE and ELR
  mrs x1, spsr_el1
  str x1, [sp, #256]
  mrs x1, elr_el1
  str x1, [sp, #264]

  // Restore the stack pointer
  mrs x1, sp_el0
  str x1, [sp, #248] 

  // Restore all registers, clean up the stack and away we go
  popall
  add sp, sp, #288
  eret
.endm

.macro make_exc_ent type
  .balign 0x80
  b common_handler_\type
.endm

make_exc_handler 0
make_exc_handler 1
make_exc_handler 2
make_exc_handler 3
make_exc_handler 4
make_exc_handler 5
make_exc_handler 6
make_exc_handler 7

.balign 0x800
.global vector_table
vector_table:
  // Current EL, SP is set to SP_EL0 (unsupported)
  make_exc_ent 4
  make_exc_ent 5
  make_exc_ent 6
  make_exc_ent 7

  // Current EL, SP is set to SP_EL{1, 2, 3}
  make_exc_ent 0
  make_exc_ent 1
  make_exc_ent 2
  make_exc_ent 3

  // Lower EL, AArch64 state
  make_exc_ent 4
  make_exc_ent 5
  make_exc_ent 6
  make_exc_ent 7

  // Lower EL, AArch32 state (unsupported)
  .balign 0x80; eret
  .balign 0x80; eret
  .balign 0x80; eret
  .balign 0x80; eret

