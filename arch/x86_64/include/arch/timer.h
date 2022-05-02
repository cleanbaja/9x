#ifndef ARCH_TIMER_H
#define ARCH_TIMER_H

#include <arch/ic.h>
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

#define HPET_REG_CAP      0x0
#define HPET_REG_CONF     0x10
#define HPET_REG_COUNTER  0x0F0

void timer_usleep(uint64_t us);
void timer_msleep(uint64_t ms);

/* Amount of times that the TSC calibration code 
 * is repeated, more higher value means longer 
 * boot time and better accuracy, and vice versa
 *
 * I personally find 5 to be the best balance.
 */
#define TSC_CALI_CYCLES 5

void timer_calibrate_tsc();

#endif // ARCH_TIMER_H

