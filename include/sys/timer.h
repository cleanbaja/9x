#ifndef SYS_TIMER_H
#define SYS_TIMER_H

#include <acpispec/tables.h>

typedef struct acpi_hpet_t {
    acpi_header_t header;
    
    uint8_t    hardware_rev_id;
    uint8_t    comparator_count : 5;
    uint8_t    counter_size : 1;
    uint8_t    reserved : 1;
    uint8_t    legacy_replace : 1;
    uint16_t   pci_vendor_id;

    acpi_gas_t    base;
    uint8_t       hpet_id;
    uint16_t      minimum_tick;
    uint8_t       page_protection;
} __attribute__((packed)) acpi_hpet_t;

#define HPET_CAP_REG           0x0
#define HPET_CONF_REG          0x10
#define HPET_MAIN_COUNTER_REG  0x0F0

void timer_init();
void timer_sleep(uint64_t ms);

#endif // SYS_TIMER_H

