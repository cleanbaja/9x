#include <arch/irqchip.h>
#include <arch/timer.h>
#include <lib/cmdline.h>
#include <lib/kcon.h>
#include <ninex/acpi.h>
#include <ninex/irq.h>
#include <vm/phys.h>
#include <vm/virt.h>
#include <vm/vm.h>

#include <lai/core.h>
#include <lai/drivers/ec.h>
#include <lai/helpers/pm.h>
#include <lai/helpers/sci.h>

static bool xsdt_found;
static struct acpi_rsdt_t* rsdt;
static struct acpi_xsdt_t* xsdt;

void* acpi_query(const char* signature, int index) {
  acpi_header_t* ptr;
  int cnt = 0;

  if (xsdt_found) {
    for (size_t i = 0; i < (xsdt->header.length - sizeof(acpi_header_t)) / 8;
         i++) {
      ptr = (acpi_header_t*)((size_t)xsdt->tables[i] + VM_MEM_OFFSET);
      if (!memcmp(ptr->signature, signature, 4)) {
        if (cnt++ == index) {
          return (void*)ptr;
        }
      }
    }
  } else {
    for (size_t i = 0; i < (rsdt->header.length - sizeof(acpi_header_t)) / 4;
         i++) {
      ptr = (acpi_header_t*)((size_t)rsdt->tables[i] + VM_MEM_OFFSET);
      if (!memcmp(ptr->signature, signature, 4)) {
        if (cnt++ == index) {
          return (void*)ptr;
        }
      }
    }
  }

  return NULL;
}

static void setup_ec(void) {
  LAI_CLEANUP_STATE lai_state_t state;
  lai_init_state(&state);

  LAI_CLEANUP_VAR lai_variable_t pnp_id = LAI_VAR_INITIALIZER;
  lai_eisaid(&pnp_id, ACPI_EC_PNP_ID);

  struct lai_ns_iterator it = LAI_NS_ITERATOR_INITIALIZER;
  lai_nsnode_t* node;
  while ((node = lai_ns_iterate(&it))) {
    if (lai_check_device_pnp_id(node, &pnp_id, &state)) // This is not an EC
      continue;

    // Found one
    struct lai_ec_driver* driver = kmalloc(sizeof(struct lai_ec_driver));
    lai_init_ec(node, driver);

    struct lai_ns_child_iterator child_it =
      LAI_NS_CHILD_ITERATOR_INITIALIZER(node);
    lai_nsnode_t* child_node;
    while ((child_node = lai_ns_child_iterate(&child_it))) {
      if (lai_ns_get_node_type(child_node) == LAI_NODETYPE_OPREGION)
        lai_ns_override_opregion(child_node, &lai_ec_opregion_override, driver);
    }

    lai_nsnode_t* reg = lai_resolve_path(node, "_REG");
    if(reg) {
      LAI_CLEANUP_VAR lai_variable_t r0 = {};
      LAI_CLEANUP_VAR lai_variable_t r1 = {};

      r0.type = LAI_INTEGER; r0.integer = 3;
      r1.type = LAI_INTEGER; r1.integer = 1;

      lai_api_error_t e = lai_eval_largs(NULL, reg, &state, &r0, &r1, NULL);
      if(e != LAI_ERROR_NONE) {
        klog("acpi: Failed to evaluate EC _REG(EmbeddedControl, 1) -> %s\n", lai_api_error_to_string(e));
        continue;
      }
    }
  }
}

static void sci_handler(struct cpu_context* context) {
  (void)context;

  uint16_t ev = lai_get_sci_event();

  const char* ev_name = "?";
  if (ev & ACPI_POWER_BUTTON)
    ev_name = "power button";
  if (ev & ACPI_SLEEP_BUTTON)
    ev_name = "sleep button";
  if (ev & ACPI_WAKE)
    ev_name = "sleep wake up";

  klog("acpi: a SCI event has occured: 0x%x (%s)", ev, ev_name);
  if (ev & ACPI_POWER_BUTTON) {
    lai_enter_sleep(5); // Good Night!
  }
}

static void setup_sci(void) {
  // Find the required ACPI tables
  acpi_fadt_t* fadt = acpi_query("FACP", 0);
  acpi_madt_t* madt = acpi_query("APIC", 0);

  // Register the handler
  int slot;
  struct irq_resource* res = alloc_irq_handler(&slot);
  res->procfs_name = "acpi_sci";
  res->HandlerFunc = sci_handler;

  // Redirect the IRQ to the proper CPU
  ic_create_redirect(0, slot, fadt->sci_irq, 0, (madt->flags & 1));

  // Enable Interrupts
  __asm__ volatile("sti");
}

void acpi_enable() {
  struct stivale2_struct_tag_rsdp* rk =
      stivale2_find_tag(STIVALE2_STRUCT_TAG_RSDP_ID);
  acpi_xsdp_t* xsdp = (acpi_xsdp_t*)rk->rsdp;

  if (xsdp->revision >= 2 && xsdp->xsdt) {
    xsdt_found = true;
    xsdt = (acpi_xsdt_t*)((uintptr_t)xsdp->xsdt + VM_MEM_OFFSET);
  } else {
    xsdt_found = false;
    rsdt = (acpi_rsdt_t*)((uintptr_t)xsdp->rsdt + VM_MEM_OFFSET);
  }

  size_t header_len = (xsdt_found) ? xsdt->header.length : rsdt->header.length;
  size_t entry_count =
    ((header_len - sizeof(acpi_header_t))) / ((xsdp->revision > 0) ? 8 : 4);
  klog("acpi: v%d, with a total of %d tables!", xsdp->revision == 0 ? 1 : xsdp->revision, entry_count);

  if (!cmdline_get_bool("verbose", true))
    return;

  for (size_t i = 0; i < entry_count; i++) {
    uint64_t table_addr =
      (xsdp->revision > 0) ? xsdt->tables[i] : rsdt->tables[i];

    acpi_header_t* c = (acpi_header_t*)(table_addr + VM_MEM_OFFSET);
    klog(" *  %c%c%c%c  0x%lx %5u  (v%d %c%c%c%c%c%c%c", c->signature[0],
         c->signature[1], c->signature[2], c->signature[3], table_addr,
         c->length, c->revision, c->oem[0], c->oem[1], c->oem[2], c->oem[3],
         c->oem[4], (c->oem[5] == ' ') ? ')' : c->oem[5],
         (c->oem[5] == ' ') ? ' ' : ')', (uint64_t)table_addr);
  }
}

void acpi_enter_ospm() {
  struct stivale2_struct_tag_rsdp* rk =
      stivale2_find_tag(STIVALE2_STRUCT_TAG_RSDP_ID);
  acpi_xsdp_t* xsdp = (acpi_xsdp_t*)rk->rsdp;

  // Enable ACPI (make it a choice, since some ACPI impls are buggy/unsupported upstream)
  if (cmdline_get_bool("acpi", true)) {
    // Init the ACPI OSL
    lai_set_acpi_revision(xsdp->revision);
    lai_create_namespace();
    lai_enable_acpi(1);

    // Setup Embedded Controllers (if they exist) and SCI events
    setup_ec();
    setup_sci();
  }
}

////////////////////////////////////////////////
// LAI (Lightweight ACPI Interpreter) Bindings
////////////////////////////////////////////////
#include <arch/asm.h>

void* laihost_malloc(size_t size) {
  return kmalloc(size);
}

void* laihost_realloc(void* p, size_t size, size_t old_size) {
  return krealloc(p, size);
}

void laihost_free(void* p, size_t unused) {
  return kfree(p);
}

void laihost_panic(const char* str) {
  PANIC(NULL, "lai: %s\n", str);

  for (;;)
    ;  // To satisfy GCC
}

void laihost_log(int level, const char* msg) {
  if (level == LAI_WARN_LOG) {
    klog("WARNING: (lai) %s", msg);
  } else {
    klog("lai: %s", msg);
  }
}

void* laihost_scan(const char* signature, size_t index) {
  if (!memcmp(signature, "DSDT", 4)) {
    // Scan for the FADT
    acpi_fadt_t* fadt = (acpi_fadt_t*)acpi_query("FACP", 0);
    void* dsdt = (char*)((size_t)fadt->dsdt + VM_MEM_OFFSET);

    return dsdt;
  } else {
    return acpi_query(signature, index);
  }
}

void* laihost_map(size_t base, size_t length) {
  (void)length;
  return (void*)(base + VM_MEM_OFFSET);
}

void laihost_unmap(void* base, size_t length) {
  (void)base;
  (void)length;
}

#ifdef __x86_64__

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

uint8_t laihost_pci_readb(uint16_t seg,
                          uint8_t bus,
                          uint8_t slot,
                          uint8_t func,
                          uint16_t offset) {
  asm_outd(0xCF8, (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfffc) |
                      0x80000000);
  uint8_t v = asm_inb(0xCFC + (offset & 3));
  return v;
}

void laihost_pci_writeb(uint16_t seg,
                        uint8_t bus,
                        uint8_t slot,
                        uint8_t func,
                        uint16_t offset,
                        uint8_t value) {
  asm_outd(0xCF8, (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfffc) |
                      0x80000000);
  asm_outb(0xCFC + (offset & 3), value);
}

uint16_t laihost_pci_readw(uint16_t seg,
                           uint8_t bus,
                           uint8_t slot,
                           uint8_t func,
                           uint16_t offset) {
  asm_outd(0xCF8, (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfffc) |
                      0x80000000);
  uint16_t v = asm_inw(0xCFC + (offset & 2));
  return v;
}

void laihost_pci_writew(uint16_t seg,
                        uint8_t bus,
                        uint8_t slot,
                        uint8_t func,
                        uint16_t offset,
                        uint16_t value) {
  asm_outd(0xCF8, (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfffc) |
                      0x80000000);
  asm_outw(0xCFC + (offset & 2), value);
}

uint32_t laihost_pci_readd(uint16_t seg,
                           uint8_t bus,
                           uint8_t slot,
                           uint8_t func,
                           uint16_t offset) {
  asm_outd(0xCF8, (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfffc) |
                      0x80000000);
  uint32_t v = asm_ind(0xCFC);
  return v;
}

void laihost_pci_writed(uint16_t seg,
                        uint8_t bus,
                        uint8_t slot,
                        uint8_t func,
                        uint16_t offset,
                        uint32_t value) {
  asm_outd(0xCF8, (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfffc) |
                      0x80000000);
  asm_outd(0xCFC, value);
}

#endif  // __x86_64__

void laihost_sleep(uint64_t ms) {
  timer_msleep(ms);
}

// The following are stubs functions I keep in here, so that the linker dosen't
// generate R_X86_64_GLOB_DAT relocations
#define STUB_CALLED() klog("lai: function %s is a stub!!!", __func__)
uint64_t laihost_timer() {
  STUB_CALLED();
  return 0;
}
void laihost_handle_amldebug(lai_variable_t* ptr) {
  (void)ptr;
  STUB_CALLED();
  return;
}
int laihost_sync_wait(struct lai_sync_state* ctx,
                      unsigned int val,
                      int64_t deadline) {
  (void)ctx;
  (void)val;
  (void)deadline;

  STUB_CALLED();
  return 0;
}
void laihost_sync_wake(struct lai_sync_state* ctx) {
  (void)ctx;
  STUB_CALLED();
  return;
}

void laihost_handle_global_notify(lai_nsnode_t* ctx, int unused) {
  (void)ctx;
  (void)unused;

  STUB_CALLED();
  return;
}
