#ifndef ACPI_H
#define ACPI_H

#include <acpispec/tables.h>
#include <lib/stivale2.h>
#include <lib/vec.h>

typedef struct acpi_madt_t {
  acpi_header_t header;
  uint32_t apic_base_addr;  // Ignored, we use the APIC MSR
  uint32_t flags;
  char entries[];
} __attribute__((packed)) acpi_madt_t;

typedef struct madt_lapic_t {
  uint8_t type;
  uint8_t length;
  uint8_t processor_id;
  uint8_t apic_id;
  uint32_t flags;
} __attribute__((packed)) madt_lapic_t;

typedef struct madt_ioapic_t {
  uint8_t type;
  uint8_t length;
  uint8_t id;
  uint8_t reserved;
  uint32_t addr;
  uint32_t gsib;
} __attribute__((packed)) madt_ioapic_t;

typedef struct madt_iso_t {
  uint8_t type;
  uint8_t length;
  uint8_t bus_source;
  uint8_t irq_source;
  uint32_t gsi;
  uint16_t flags;
} __attribute__((packed)) madt_iso_t;

typedef struct madt_nmi_t {
  uint8_t type;
  uint8_t length;
  uint8_t processor;
  uint16_t flags;
  uint8_t lint;
} __attribute__((packed)) madt_nmi_t;

void acpi_enable();
void acpi_enter_ospm();
void *acpi_query(const char *signature, int index);

#endif  // ACPI_H
