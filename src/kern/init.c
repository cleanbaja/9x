#include <9x/acpi.h>
#include <9x/vfs.h>
#include <9x/vm.h>
#include <lib/console.h>
#include <lib/log.h>
#include <lib/cmdline.h>
#include <sys/apic.h>
#include <sys/cpu.h>
#include <sys/tables.h>
#include <sys/timer.h>

#include "config.h"

static uint16_t _kstack[8192];

// Adjusts the cpu_local of the BSP, since its invalid at first
#define PERCPU_FIXUP()                                                         \
  ({                                                                           \
    per_cpu(kernel_stack) = (uintptr_t)_kstack + VM_PAGE_SIZE;                 \
    per_cpu(tss).rsp0 = per_cpu(kernel_stack);                                 \
  })

static struct stivale2_header_tag_smp smp_tag = {
  .tag = { .identifier = STIVALE2_HEADER_TAG_SMP_ID, .next = 0 },
  .flags = (1 << 0)
};

static struct stivale2_header_tag_framebuffer fbuf_tag = {
  .tag = { .identifier = STIVALE2_HEADER_TAG_FRAMEBUFFER_ID,
           .next = (uintptr_t)&smp_tag },
  .framebuffer_width = 0,
  .framebuffer_height = 0,
  .framebuffer_bpp = 0
};

__attribute__((section(".stivale2hdr"),
               used)) static struct stivale2_header hdr = {
  .entry_point = 0,
  .stack = (uintptr_t)_kstack + sizeof(_kstack),
  .flags = (1 << 1) | (1 << 4),
  .tags = (uintptr_t)&fbuf_tag,
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
  // Zero out the CPU-local storage (so PANIC dosen't get confused)
  WRITE_PERCPU(NULL, 0);

  // Get arch-specific structures up
  init_tables();

  // Start the console and say hello!
  console_init();
  log("9x [v%d.%d.%d] - A project by Yusuf M (cleanbaja)",
      VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
  log("Bootloader: %s (%s)",
      bootags->bootloader_brand,
      bootags->bootloader_version);

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
  cpu_init(stivale2_find_tag(STIVALE2_STRUCT_TAG_SMP_ID));
  PERCPU_FIXUP();

  // Initialize the vfs and filesystems
  vfs_init(stivale2_find_tag(STIVALE2_STRUCT_TAG_MODULES_ID));

  // Chill for now...
  log("init: Startup complete, halting all cores!");
  send_ipi(IPI_HALT, 0, IPI_OTHERS);

  for (;;) {
    __asm__ volatile("sti; hlt");
  }
}

