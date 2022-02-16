#include <9x/acpi.h>
#include <9x/vm.h>
#include <lib/log.h>
#include <stdbool.h>

#include <lai/core.h>
#include <lai/drivers/ec.h>

static bool xsdt_found;
static struct acpi_rsdt_t* rsdt;
static struct acpi_xsdt_t* xsdt;

vec_lapic_t  madt_lapics;
vec_ioapic_t madt_ioapics;
vec_iso_t    madt_isos;
vec_nmi_t    madt_nmis;

void madt_init();
void setup_ec();

void
acpi_init(struct stivale2_struct_tag_rsdp* rk)
{
  acpi_xsdp_t* xsdp = (acpi_xsdp_t*)rk->rsdp;

  if (xsdp->revision >= 2 && xsdp->xsdt) {
    xsdt_found = true;
    xsdt = (acpi_xsdt_t*)((uintptr_t)xsdp->xsdt + VM_MEM_OFFSET);
  } else {
    xsdt_found = false;
    rsdt = (acpi_rsdt_t*)((uintptr_t)xsdp->rsdt + VM_MEM_OFFSET);
  }

  log("acpi: dumping tables... (revision: %u)", xsdp->revision);
  log("    %s %s %s  %s", "Signature", "Rev", "OEMID", "Address");
  int length = xsdt_found ? xsdt->header.length : rsdt->header.length;
  for (size_t i = 0; i < length - sizeof(acpi_header_t); i++) {
    uintptr_t entry;
    if (xsdt_found)
      entry = (uintptr_t)(((uint64_t*)xsdt->tables)[i]);
    else
      entry = (uintptr_t)(rsdt->tables[i]);

    if (!entry)
      continue;

    acpi_header_t* h = (acpi_header_t*)(entry + VM_MEM_OFFSET);

    if (!h->oem || !h->signature)
      continue;

    log("    %c%c%c%c      %d   %c%c%c%c%c%c 0x%lx",
        h->signature[0],
        h->signature[1],
        h->signature[2],
        h->signature[3],
        h->revision,
        h->oem[0],
        h->oem[1],
        h->oem[2],
        h->oem[3],
        h->oem[4],
        h->oem[5] ? h->oem[5] : ' ',
        (uint64_t)entry);
  }

  // Set ACPI Revision and parse the MADT
  // lai_set_acpi_revision(xsdp->revision);
  madt_init();

  // Setup Embedded Controllers (if they exist)
  // setup_ec();

  // Init the ACPI OSL
  lai_create_namespace();
  // lai_enable_acpi(1);
}

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
          // log("acpi: Found \"%s\" at 0x%lx", signature, (size_t)ptr);
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
          // log("acpi: Found \"%s\" at 0x%lx", signature, (size_t)ptr);
          return (void*)ptr;
        }
      }
    }
  }

  log("acpi: \"%s\" not found", signature);
  return (void*)0;
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

  log("acpi: Detected %u ISOs, %u I/O APICs, and %u NMIs", madt_isos.length, madt_ioapics.length, madt_nmis.length);
}


void setup_ec(void){
    LAI_CLEANUP_STATE lai_state_t state;
    lai_init_state(&state);

    LAI_CLEANUP_VAR lai_variable_t pnp_id = LAI_VAR_INITIALIZER;
    lai_eisaid(&pnp_id, ACPI_EC_PNP_ID);

    struct lai_ns_iterator it = LAI_NS_ITERATOR_INITIALIZER;
    lai_nsnode_t *node;
    while((node = lai_ns_iterate(&it))){
        if(lai_check_device_pnp_id(node, &pnp_id, &state)) // This is not an EC
            continue;

        // Found one
        log("acpi: Found Embedded Controller!");
	struct lai_ec_driver *driver = kmalloc(sizeof(struct lai_ec_driver));
        lai_init_ec(node, driver);   

        struct lai_ns_child_iterator child_it = LAI_NS_CHILD_ITERATOR_INITIALIZER(node);
        lai_nsnode_t *child_node;
        while((child_node = lai_ns_child_iterate(&child_it))){
            if(lai_ns_get_node_type(child_node) == LAI_NODETYPE_OPREGION)
                lai_ns_override_opregion(child_node, &lai_ec_opregion_override, driver);
        }
    }
}
