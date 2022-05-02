#ifndef ARCH_APIC_H
#define ARCH_APIC_H

#include <stdbool.h>
#include <stdint.h>

#define IA32_APIC 0x1B
#define IA32_x2APIC_BASE 0x800
#define IA32_TSC_DEADLINE 0x6E0

// Basic operations
void ic_enable();
void ic_eoi();

// IRQ Stuff
void ic_mask_irq(uint16_t irq, bool status);
void ic_create_redirect(uint8_t lapic_id, uint8_t vec, uint16_t slot, bool legacy);

// Timer stuff
void ic_timer_stop();
void ic_timer_oneshot(uint8_t vec, uint64_t ms);
void ic_timer_calibrate();

enum ipi_mode
{
  IPI_SELF = 0x10,
  IPI_OTHERS,
  IPI_SPECIFIC,
  IPI_EVERYONE
};

#define IPI_HALT        254
#define IPI_INVL_TLB    253
#define IPI_SCHED_YIELD 252
void ic_send_ipi(uint8_t vec, uint32_t cpu, enum ipi_mode mode);

#endif // ARCH_APIC_H

