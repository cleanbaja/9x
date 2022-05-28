#ifndef ARCH_APIC_H
#define ARCH_APIC_H

#include <ninex/init.h>
#include <stdint.h>

#define IA32_APIC 0x1B
#define IA32_x2APIC_BASE 0x800
#define IA32_TSC_DEADLINE 0x6E0

// x86_64's IRQ range lasts from 32-254, since IRQs < 32 are CPU exceptions
#define ARCH_NUM_IRQS (254 - 32)
#define ARCH_LOWEST_IRQ 32

// CPU Trap Frame, as pushed to the stack by helpers.asm
// DO NOT MODIFY!!! (since its accessed by the raw assembly)
typedef struct __attribute__((packed)) cpu_context {
  uint64_t r15, r14, r13, r12, r11, r10, r9;
  uint64_t r8, rbp, rdi, rsi, rdx, rcx, rbx;
  uint64_t rax, int_no, ec, rip, cs, rflags;
  uint64_t rsp, ss;
} cpu_ctx_t;

// IRQ Stuff
void ic_mask_irq(uint16_t irq, bool status);
void ic_create_redirect(uint8_t lapic_id,
                        uint8_t vec,
                        uint16_t slot,
                        int flags,
                        bool legacy);
void ic_eoi();

// Timer stuff
void ic_timer_stop();
void ic_timer_oneshot(uint8_t vec, uint64_t ms);
void ic_timer_calibrate();

// Initialization Targets
EXPORT_STAGE(scan_madt_target);
EXPORT_STAGE(apic_ready);

enum ipi_mode { IPI_SELF = 0x10, IPI_OTHERS, IPI_SPECIFIC, IPI_EVERYONE };

#define IPI_HALT 254
#define IPI_INVL_TLB 253
#define IPI_SCHED_YIELD 252
void ic_send_ipi(uint8_t vec, uint32_t cpu, enum ipi_mode mode);

#endif  // ARCH_APIC_H
