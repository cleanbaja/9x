#ifndef SYS_APIC_H
#define SYS_APIC_H

#include <stdint.h>

#define IA32_APIC 0x1B
#define IA32_x2APIC_BASE 0x800

void
activate_apic();
void
apic_eoi();

enum ipi_mode
{
  IPI_SELF = 0x10,
  IPI_OTHERS,
  IPI_SPECIFIC,
  IPI_EVERYONE
};

void
send_ipi(uint8_t vec, uint32_t cpu, enum ipi_mode mode);

#endif // SYS_APIC_H
