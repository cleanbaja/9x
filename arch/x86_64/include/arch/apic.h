#ifndef SYS_APIC_H
#define SYS_APIC_H

#include <stdbool.h>
#include <stdint.h>

#define IA32_APIC 0x1B
#define IA32_x2APIC_BASE 0x800
#define IA32_TSC_DEADLINE 0x6E0

void
apic_enable();
void
calibrate_apic();
void
apic_eoi();

void
apic_redirect_gsi(uint8_t lapic_id,
                  uint8_t vec,
                  uint32_t gsi,
                  uint16_t flags,
                  bool status);
void
apic_redirect_irq(uint8_t lapic_id, uint8_t vec, uint8_t irq, bool masked);
void
apic_calibrate();

// Timer stuff
void apic_timer_stop();
void apic_timer_oneshot(uint8_t vec, uint64_t ms);

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

void
apic_send_ipi(uint8_t vec, uint32_t cpu, enum ipi_mode mode);

#endif // SYS_APIC_H