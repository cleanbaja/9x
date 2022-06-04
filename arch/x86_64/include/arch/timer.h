#ifndef ARCH_TIMER_H
#define ARCH_TIMER_H

#include <acpispec/tables.h>
#include <arch/irqchip.h>
#include <ninex/init.h>

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

/* Amount of times that the TSC calibration code 
 * is repeated, more higher value means longer 
 * boot time and better accuracy, and vice versa
 *
 * I personally find 5 to be the best balance.
 */
#define TSC_CALI_CYCLES 5

EXPORT_STAGE(timer_cali);
void timer_usleep(uint64_t us);
void timer_msleep(uint64_t ms);

// On x86, timer related stuff is done in the APIC, so define it as such
#define timer_oneshot(ms, slot) ic_timer_oneshot(ms, slot)
#define timer_stop() ic_timer_stop()

#endif // ARCH_TIMER_H

