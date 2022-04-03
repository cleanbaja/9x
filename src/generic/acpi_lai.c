#include <generic/acpi.h>
#include <arch/asm.h>
#include <lai/core.h>
#include <lib/kcon.h>
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

void laihost_panic(const char* str) {
  PANIC(NULL, "lai: %s\n");

  for(;;); // To satisfy GCC
}

void laihost_log(int level, const char *msg) {
  if (level == LAI_WARN_LOG) {
    klog("WARNING: (lai) %s", msg);
  } else {
    klog("lai: %s", msg);
  }
}

void *laihost_scan(const char *signature, size_t index) {
    if (!memcmp(signature, "DSDT", 4)) {
        if (index > 0) {
            klog("acpi: Only valid index for DSDT is 0");
            return NULL;
        }

        // Scan for the FADT
        acpi_fadt_t *fadt = (acpi_fadt_t *)acpi_query("FACP", 0);
        void *dsdt = (char *)((size_t)fadt->dsdt + VM_MEM_OFFSET);

        klog("acpi: Address of DSDT is 0x%lx", dsdt);
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

uint8_t laihost_pci_readb(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset) {
    asm_outd(0xCF8, (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfffc) | 0x80000000);
    uint8_t v = asm_inb(0xCFC + (offset & 3));
    return v;
}

void laihost_pci_writeb(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset, uint8_t value) {
    asm_outd(0xCF8, (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfffc) | 0x80000000);
    asm_outb(0xCFC + (offset & 3), value);
}

uint16_t laihost_pci_readw(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset) {
    asm_outd(0xCF8, (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfffc) | 0x80000000);
    uint16_t v = asm_inw(0xCFC + (offset & 2));
    return v;
}

void laihost_pci_writew(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset, uint16_t value) {
    asm_outd(0xCF8, (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfffc) | 0x80000000);
    asm_outw(0xCFC + (offset & 2), value);
}

uint32_t laihost_pci_readd(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset) {
    asm_outd(0xCF8, (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfffc) | 0x80000000);
    uint32_t v = asm_ind(0xCFC);
    return v;
}

void laihost_pci_writed(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset, uint32_t value) {
    asm_outd(0xCF8, (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfffc) | 0x80000000);
    asm_outd(0xCFC, value);
}

// TODO: Actually sleep (with HPET?)
void laihost_sleep(uint64_t ms) {
    for (size_t i = 0; i < 1000 * ms; i++) {
        asm_inb(0x80);
    }
}

// The following are stubs functions I keep in here, so that the linker dosen't generate R_X86_64_GLOB_DAT relocs
#define STUB_CALLED() klog("lai: STUB CALLED!!!")
uint64_t laihost_timer() {
  STUB_CALLED();
  return 0;
}
void laihost_handle_amldebug(lai_variable_t *ptr) {
  (void)ptr;
  STUB_CALLED();
  return;
}
int laihost_sync_wait(struct lai_sync_state *ctx, unsigned int val, int64_t deadline) {
  (void)ctx; (void)val; (void)deadline;
  STUB_CALLED();
  return 0;
}
void laihost_sync_wake(struct lai_sync_state *ctx) {
  (void)ctx;
  STUB_CALLED();
  return;
}
