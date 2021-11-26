#include <internal/stivale2.h>
#include <sys/tables.h>
#include <lib/log.h>
#include <vm.h>

#include <stdint.h>

static uint16_t _kstack[8192];

static struct stivale2_header_tag_smp smp_tag = {
  .tag = {
    .identifier = STIVALE2_HEADER_TAG_SMP_ID,
    .next = 0
  },
  .flags = (1 << 0)
};

static struct stivale2_header_tag_framebuffer fbuf_tag = {
  .tag = {
    .identifier = STIVALE2_HEADER_TAG_FRAMEBUFFER_ID,
    .next = (uintptr_t)&smp_tag
  },
  .framebuffer_width = 0,
  .framebuffer_height = 0,
  .framebuffer_bpp = 0
};

__attribute__((section(".stivale2hdr"), used))
static struct stivale2_header hdr = {
  .entry_point = 0,
  .stack = (uintptr_t)_kstack + sizeof(_kstack),
  .flags = (1 << 1) | (1 << 4),
  .tags = (uintptr_t)&fbuf_tag,
};

static struct stivale2_struct* bootags;

static void* stivale2_find_tag(uint64_t id) {
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

void kern_entry(struct stivale2_struct* bootinfo) {
	bootags = bootinfo;

  // Say hello!
  log("9x (x86_64) (v0.1.0) - A project by Yusuf M (cleanbaja)");
  log("Bootloader: %s (%s)", bootinfo->bootloader_brand, bootinfo->bootloader_version);
  
  // Load GDT/IDT
  init_gdt();
  init_idt();

  // Initialize the memory subsystem
  vm_init(stivale2_find_tag(STIVALE2_STRUCT_TAG_MEMMAP_ID));

  PANIC(NULL, "End of kernel reached!\n\n");
}

