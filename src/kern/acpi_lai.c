#include <9x/acpi.h>
#include <internal/asm.h>
#include <lai/core.h>
#include <lib/log.h>
#include <vm/phys.h>
#include <vm/virt.h>
#include <vm/vm.h>

void *laihost_malloc(size_t size) {
    return kmalloc(size);
}

void*
laihost_realloc(void* p, size_t size, size_t old_size)
{
  return krealloc(p, size);
}

void
laihost_free(void* p, size_t unused)
{
  return kfree(p);
}

void laihost_log(int level, const char *msg) {
  if (level == LAI_WARN_LOG) {
    log("WARNING: (lai) %s", msg);
  } else {
    log("lai: %s", msg);
  }
}

void *laihost_scan(const char *signature, size_t index) {
    if (!memcmp(signature, "DSDT", 4)) {
        if (index > 0) {
            log("acpi: Only valid index for DSDT is 0");
            return NULL;
        }

        // Scan for the FADT
        acpi_fadt_t *fadt = (acpi_fadt_t *)acpi_query("FACP", 0);
        void *dsdt = (char *)(size_t)fadt->dsdt + VM_MEM_OFFSET;

        log("acpi: Address of DSDT is 0x%lx", dsdt);
        return dsdt;
    } else {
      return acpi_query(signature, index);
    }
}

void *laihost_map(size_t base, size_t length) {
    (void) length;
    return (void *) (base + VM_MEM_OFFSET);
}

void laihost_unmap(void *base, size_t length) {
    (void) base;
    (void) length;
} 

void laihost_outb(uint16_t port, uint8_t data) {
    asm_outb(port, data);
}

void laihost_outw(uint16_t port, uint16_t data) {
    asm_outw(port, data);
}

void laihost_outd(uint16_t port, uint32_t data) {
    asm_outd(port, data);
}

uint8_t laihost_inb(uint16_t port) {
    return asm_inb(port);
}

uint16_t laihost_inw(uint16_t port) {
    return asm_inw(port);
}

uint32_t laihost_ind(uint16_t port) {
    return asm_ind(port);
}

void laihost_pci_writeb(uint16_t segment, uint8_t bus, uint8_t slot, uint8_t function, uint16_t offset, uint8_t data) {
    return;
}

uint8_t laihost_pci_readb(uint16_t segment, uint8_t bus, uint8_t slot, uint8_t function, uint16_t offset) {
    return 0;
}

void laihost_pci_writew(uint16_t segment, uint8_t bus, uint8_t slot, uint8_t function, uint16_t offset, uint16_t data) {
    return;
}

uint16_t laihost_pci_readw(uint16_t segment, uint8_t bus, uint8_t slot, uint8_t function, uint16_t offset) {
    return 0;
}

void laihost_pci_writed(uint16_t segment, uint8_t bus, uint8_t slot, uint8_t function, uint16_t offset, uint32_t data) {
    return;
}

uint32_t laihost_pci_readd(uint16_t segment, uint8_t bus, uint8_t slot, uint8_t function, uint16_t offset) {
    return 0;
}

// TODO: Actually sleep (with HPET?)
void laihost_sleep(uint64_t ms) {
    for (size_t i = 0; i < 1000 * ms; i++) {
        asm_inb(0x80);
    }
}

