#include <arch/hat.h>
#include <arch/irqchip.h>
#include <lib/cmdline.h>
#include <lib/kcon.h>
#include <vm/phys.h>
#include <vm/virt.h>
#include <vm/vm.h>

static char*
mem_to_str(int mem_type)
{
  switch (mem_type) {
    case STIVALE2_MMAP_USABLE:
      return "usable";
      break;
    case STIVALE2_MMAP_RESERVED:
      return "reserved";
      break;
    case STIVALE2_MMAP_ACPI_RECLAIMABLE:
      return "acpi_reclaimable";
      break;
    case STIVALE2_MMAP_ACPI_NVS:
      return "acpi_nvs";
      break;
    case STIVALE2_MMAP_BAD_MEMORY:
      return "badmem";
      break;
    case STIVALE2_MMAP_BOOTLOADER_RECLAIMABLE:
      return "boot_reclaimable";
      break;
    case STIVALE2_MMAP_KERNEL_AND_MODULES:
      return "kernel+mods";
      break;
    case STIVALE2_MMAP_FRAMEBUFFER:
      return "framebuffer";
      break;
    default:
      return "???";
      break;
  }
}

void vm_setup() {
  struct stivale2_struct_tag_memmap* mm_tag =
      stivale2_find_tag(STIVALE2_STRUCT_TAG_MEMMAP_ID);

  // Setup the HAT first
  hat_init();

  // Dump all memmap entries
  if (!(mm_tag->entries > 25) || cmdline_get_bool("verbose", false)) {
    klog("vm/phys: Dumping memory map (entries: %d):", mm_tag->entries);
    for (int i = 0; i < mm_tag->entries; i++) {
      struct stivale2_mmap_entry entry = mm_tag->memmap[i];
      klog("    (0x%016lx-0x%016lx) %s",
          entry.base,
          entry.base + entry.length,
          mem_to_str(entry.type));
    }
  }

  // Create the physical memory zones...
  for (int i = 0; i < mm_tag->entries; i++) {
    struct stivale2_mmap_entry entry = mm_tag->memmap[i];
    if (entry.type == STIVALE2_MMAP_USABLE &&
        vm_zone_possible(entry.base, entry.length))
      vm_create_zone(entry.base, entry.length);
  }

  // Check to make sure we have at least one possible zone we can use
  if (head_zone == NULL) {
    PANIC(NULL, "No suitable memory zones!\n");
  }

  // Setup virtual memory...
  vm_virt_init();
}
