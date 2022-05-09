#include <ninex/acpi.h>
#include <lib/cmdline.h>
#include <lib/kcon.h>
#include <arch/ic.h>
#include <arch/irq.h>
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

vec_lapic_t  madt_lapics;
vec_ioapic_t madt_ioapics;
vec_iso_t    madt_isos;
vec_nmi_t    madt_nmis;

void*
acpi_query(const char* signature, int index)
{
  acpi_header_t* ptr;
  int cnt = 0;

  if (xsdt_found) {
    for (size_t i = 0; i < (xsdt->header.length - sizeof(acpi_header_t)) / 8;
         i++) {
      ptr = (acpi_header_t*)((size_t)xsdt->tables[i] + VM_MEM_OFFSET);
      if (!memcmp(ptr->signature, signature, 4)) {
        if (cnt++ == index) {
          // klog("acpi: Found \"%s\" at 0x%lx", signature, (size_t)ptr);
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
          // klog("acpi: Found \"%s\" at 0x%lx", signature, (size_t)ptr);
          return (void*)ptr;
        }
      }
    }
  }

  // klog("acpi: \"%s\" not found", signature);
  return NULL;
}


static void map_table(uintptr_t phys) {
  size_t    length = ((acpi_header_t*)(phys + VM_MEM_OFFSET))->length;
  uintptr_t paddr  = phys & ~(VM_PAGE_SIZE - 1);
  uint64_t  vsize  = (DIV_ROUNDUP(length, 0x1000) * 0x1000) + VM_PAGE_SIZE*2;

  for(size_t pg = 0; pg < vsize; pg += VM_PAGE_SIZE) {
    vm_virt_fragment(&kernel_space, paddr + pg + VM_MEM_OFFSET, VM_PERM_READ | VM_PERM_WRITE);
    vm_virt_map(&kernel_space, paddr + pg, paddr + pg + VM_MEM_OFFSET, VM_PERM_READ | VM_CACHE_UNCACHED);
  }
}

void madt_init() {
  acpi_madt_t* pmadt = (acpi_madt_t*)acpi_query("APIC", 0);
  if (pmadt == NULL)
    PANIC(NULL, "Unable to find ACPI MADT table");

  // Initialize the vectors
  vec_init(&madt_lapics);
  vec_init(&madt_ioapics);
  vec_init(&madt_isos);
  vec_init(&madt_nmis);

  // Parse the actual entries
  for (uint8_t *madt_ptr = (uint8_t *)pmadt->entries;
      (uintptr_t)madt_ptr < (uintptr_t)pmadt + pmadt->header.length;
      madt_ptr += *(madt_ptr + 1)) {
        switch (*(madt_ptr)) {
            case 0: // Processor Local APIC
                vec_push(&madt_lapics, (madt_lapic_t *)madt_ptr);
                break;
            case 1: // I/O APIC
                vec_push(&madt_ioapics, (madt_ioapic_t *)madt_ptr);
                break;
            case 2: // Interrupt Source Override
                vec_push(&madt_isos, (madt_iso_t *)madt_ptr);
                break;
            case 4: // Non-Maskable Interrupt
                vec_push(&madt_nmis, (madt_nmi_t *)madt_ptr);
                break;
        }
  }

  klog("acpi: Detected %u ISOs, %u I/O APICs, and %u NMIs", madt_isos.length, madt_ioapics.length, madt_nmis.length);
}

void
setup_ec(void)
{
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
  }
}

static void
sci_handler(struct cpu_context* context, void* arg)
{
  (void)context;
  (void)arg;

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

void
setup_sci(void)
{
  // Find the required ACPI tables
  acpi_fadt_t* fadt = acpi_query("FACP", 0);
  acpi_madt_t* madt = acpi_query("APIC", 0);

  // Register the handler
  int slot;
  struct irq_handler* hnd = request_irq("acpi_sci", &slot);
  hnd->should_return = true;
  hnd->is_irq = true;
  hnd->hnd    = sci_handler;

  // Redirect the IRQ to the proper CPU
  ic_create_redirect(0, slot, fadt->sci_irq, (madt->flags & 1));

  // Enable Interrupts
  __asm__ volatile("sti");
}


CREATE_STAGE(acpi_stage, acpi_init, 0, {})
static void acpi_init()
{
  struct stivale2_struct_tag_rsdp* rk = stivale2_find_tag(STIVALE2_STRUCT_TAG_RSDP_ID);
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
  klog("acpi: dumping %u entries... (revision: %u)", entry_count, xsdp->revision);
  klog("    %-8s %-s %-6s  %-11s", "Signature", "Rev", "OEMID", "Address");

  for (size_t i = 0; i < entry_count; i++) {
    uint64_t table_addr =
      (xsdp->revision > 0) ? xsdt->tables[i] : rsdt->tables[i];

    // TODO: Map ACPI tables (current func is broken)
    acpi_header_t* c = (acpi_header_t*)(table_addr + VM_MEM_OFFSET);
    klog("    %-c%c%c%c %6d %3c%c%c%c%c%c  %#0lx",
        c->signature[0],
        c->signature[1],
        c->signature[2],
        c->signature[3],
        c->revision,
        c->oem[0],
        c->oem[1],
        c->oem[2],
        c->oem[3],
        c->oem[4],
        c->oem[5],
        (uint64_t)table_addr + VM_MEM_OFFSET);
  }

  // Parse the MADT for IO-APIC information
  madt_init();

  // Enable ACPI (make it a choice, since some ACPI impls are buggy/unsupported upstream)
  if (!cmdline_get_bool("acpi", true)) {
    // Init the ACPI OSL
    lai_set_acpi_revision(xsdp->revision);
    lai_create_namespace();
    lai_enable_acpi(1);

    // Setup Embedded Controllers (if they exist) and SCI events
    setup_ec();
    setup_sci();
  }
}

