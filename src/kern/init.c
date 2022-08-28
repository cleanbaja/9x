/* src/kern/init.c - Kernel Initialization Routines
 * SPDX-License-Identifier: Apache-2.0 */

#include <misc/stivale2.h>
#include <lib/print.h>
#include <lib/panic.h>
#include <lib/cmdline.h>
#include <lib/lock.h>
#include <dev/console.h>

static struct stivale2_struct* bootinfo = NULL;
extern char __kern_stack_top[];

static struct stivale2_header_tag_framebuffer fbuf_tag = {
    .tag = {
        .identifier = STIVALE2_HEADER_TAG_FRAMEBUFFER_ID,
        .next = 0
    },

    .framebuffer_width = 0,
    .framebuffer_height = 0,
    .framebuffer_bpp = 0
};

__attribute__((section(".stivale2hdr"),
               used)) static struct stivale2_header hdr = {
    .entry_point = 0,
    .stack = (uintptr_t)__kern_stack_top,
    .tags = (uintptr_t)&fbuf_tag,

    // Use higher-half pointers, don't panic when lowmem isn't available
    .flags = (1 << 1) | (1 << 4)
};

void *stivale2_get_tag(uint64_t id) {
    struct stivale2_tag *current_tag = (struct stivale2_tag *)bootinfo->tags;
    for (;;) {
        if (current_tag == NULL) {
            return NULL;
        }

        if (current_tag->identifier == id) {
            return current_tag;
        }

        // Get a pointer to the next tag in the linked list and repeat.
        current_tag = (struct stivale2_tag *)current_tag->next;
    }
}

__attribute__((noreturn)) void kern_entry(struct stivale2_struct* info) {
  bootinfo = info;

  // Initialize outputs and traps
  print_init();
  trap_init();
  console_init();

  kprint("welcome to ninex!\n");
  kprint("Bootloader: %s [%s]\n", info->bootloader_brand, info->bootloader_version);
  
  // Load the kernel command line
  struct stivale2_struct_tag_cmdline* ctag = stivale2_get_tag(STIVALE2_STRUCT_TAG_CMDLINE_ID);
  if (ctag) cmdline_load((const char*)ctag->cmdline);

  // Call it quits for now...
  for (;;) {
#if defined (__x86_64__)
    asm volatile ("cli; hlt");
#elif defined (__aarch64__)
    asm volatile ("msr DAIFSet, #15; wfi");
#endif
  }
}

