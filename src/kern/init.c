#include <ninex/acpi.h>
#include <ninex/smp.h>
#include <lib/cmdline.h>
#include <lib/kcon.h>
#include <lib/kcon.h>
#include <arch/ic.h>
#include <arch/tables.h>
#include <fs/vfs.h>
#include <vm/phys.h>
#include <vm/vm.h>

#include "config.h"

static uint16_t _kstack[8192];

// Adjusts the cpu_local of the BSP, since its invalid at first
#define PERCPU_FIXUP()                                                         \
  ({                                                                           \
    per_cpu(kernel_stack) = (uintptr_t)_kstack + VM_PAGE_SIZE*4;               \
    per_cpu(tss).rsp0 = per_cpu(kernel_stack);                                 \
  })

static struct stivale2_header_tag_smp smp_tag = {
  .tag = { .identifier = STIVALE2_HEADER_TAG_SMP_ID, .next = 0 },
  .flags = (1 << 0)
};

#ifdef LIMINE_EARLYCONSOLE
static struct stivale2_header_tag_terminal terminal_hdr_tag = {
    .tag = {
        .identifier = STIVALE2_HEADER_TAG_TERMINAL_ID,
        .next = (uintptr_t)&smp_tag
    },
    .flags = 0
};
#endif

static struct stivale2_header_tag_framebuffer fbuf_tag = {
  .tag = { .identifier = STIVALE2_HEADER_TAG_FRAMEBUFFER_ID,
  #ifdef LIMINE_EARLYCONSOLE
           .next = (uintptr_t)&terminal_hdr_tag },
  #else
           .next = (uintptr_t)&smp_tag },
  #endif
  .framebuffer_width = 0,
  .framebuffer_height = 0,
  .framebuffer_bpp = 0
};

static struct stivale2_tag five_lv_tag = {
  .identifier = STIVALE2_HEADER_TAG_5LV_PAGING_ID,
  .next = (uintptr_t)&fbuf_tag,
};

__attribute__((section(".stivale2hdr"),
               used)) static struct stivale2_header hdr = {
  .entry_point = 0,
  .stack = (uintptr_t)_kstack + sizeof(_kstack),
  .flags = (1 << 1) | (1 << 4),
  .tags = (uintptr_t)&five_lv_tag,
};

static struct stivale2_struct* bootags;

void*
stivale2_find_tag(uint64_t id)
{
  struct stivale2_tag* current_tag = (struct stivale2_tag*)bootags->tags;
  for (;;) {
    if (current_tag == NULL) {
      return NULL;
    }

    if (current_tag->identifier == id) {
      return current_tag;
    }

    // Get a pointer to the next tag in the linked list and repeat.
    current_tag = (struct stivale2_tag*)current_tag->next;
  }
}

static void
early_init()
{
  // Start the console and say hello!
  kcon_init();
  klog("9x (%s) (%s) - A project by cleanbaja", NINEX_VERSION, NINEX_ARCH);
  klog("Bootloader: %s [%s]",
       bootags->bootloader_brand,
       bootags->bootloader_version);

  // Get arch-specific structures up
  cpu_early_init();
  init_tables();

  // Finally, load the kernel command line
  struct stivale2_struct_tag_cmdline* cmdline_tag = (struct stivale2_struct_tag_cmdline*)stivale2_find_tag(STIVALE2_STRUCT_TAG_CMDLINE_ID);
  cmdline_load((char*)(cmdline_tag->cmdline));
}

void
kern_entry(struct stivale2_struct* bootinfo)
{
  // Save bootinfo and zero rbp (for stacktracing)
  bootags = bootinfo;
  __asm__ volatile("xor %rbp, %rbp");

  // Initialize the core parts of the kernel
  early_init();

  // Initialize the memory subsystem
  vm_init(stivale2_find_tag(STIVALE2_STRUCT_TAG_MEMMAP_ID));

  // Initialize ACPI
  acpi_init(stivale2_find_tag(STIVALE2_STRUCT_TAG_RSDP_ID));

  // Initialize other CPUs and fix the percpu structure
  smp_init(stivale2_find_tag(STIVALE2_STRUCT_TAG_SMP_ID));
  PERCPU_FIXUP();

  // Initialize the VFS
  vfs_init(stivale2_find_tag(STIVALE2_STRUCT_TAG_MODULES_ID));

  klog("init: Startup complete, halting all cores!");
  ic_send_ipi(IPI_HALT, 0, IPI_OTHERS);
  for (;;) {
    __asm__ volatile("sti; hlt");
  }
}

