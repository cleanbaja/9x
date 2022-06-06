#include <arch/arch.h>
#include <arch/irqchip.h>
#include <fs/vfs.h>
#include <lib/kcon.h>
#include <ninex/acpi.h>
#include <ninex/init.h>
#include <ninex/extension.h>
#include <vm/vm.h>

// Root kernel stage!
CREATE_STAGE_SMP(root_stage,
                 DUMMY_CALLBACK,
                 {arch_early_stage, vm_stage, acpi_stage, arch_late_stage,
                  vfs_stage, kext_stage});
static CREATE_SPINLOCK(smp_entry_lock);
static uint8_t _kstack[0x1000 * 16];

///////////////////////////
//  Stivale2 Interface
///////////////////////////
#ifdef LIMINE_EARLYCONSOLE
static struct stivale2_header_tag_terminal terminal_hdr_tag = {
    .tag = {.identifier = STIVALE2_HEADER_TAG_TERMINAL_ID, .next = 0},
    .flags = 0};
#endif

static struct stivale2_header_tag_framebuffer fbuf_tag = {
  .tag = { .identifier = STIVALE2_HEADER_TAG_FRAMEBUFFER_ID,
  #ifdef LIMINE_EARLYCONSOLE
           .next = (uintptr_t)&terminal_hdr_tag },
  #else
            .next = 0},
#endif
  .framebuffer_width = 0,
  .framebuffer_height = 0,
  .framebuffer_bpp = 0
};

static struct stivale2_tag five_lv_tag = {
  .identifier = STIVALE2_HEADER_TAG_5LV_PAGING_ID,
  .next = (uintptr_t)&fbuf_tag,
};

static struct stivale2_struct* bootags;
__attribute__((section(".stivale2hdr"),
               used)) static struct stivale2_header hdr = {
  .entry_point = 0,
  .stack = (uintptr_t)_kstack + sizeof(_kstack),
  .flags = (1 << 1) | (1 << 4),
  .tags = (uintptr_t)&five_lv_tag,
};

void* stivale2_find_tag(uint64_t id) {
  if (id == 1)
    return bootags;

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

///////////////////////////
//  Initgraph Functions
///////////////////////////
struct init_stage* stage_resolve(struct init_stage* entry) {
  struct init_stage *head, *tail, *stack;
  entry->next_resolve = NULL;
  head = tail = NULL;
  stack = entry;

  // Loop through the initgraph, fixing up dependencies and such
  while (stack != NULL) {
    struct init_stage* cur = stack;

    // Check if this target is completed
    if (cur->depth == cur->count) {
      stack = stack->next_resolve;
      cur->completed = true;
      cur->next = NULL;
      if (head == NULL) {
        head = cur;
        tail = cur;
      } else {
        tail->next = cur;
        tail = cur;
      }
    } else {
      struct init_stage* dependency = cur->deps[cur->depth++];
      if (dependency->completed) {
        continue;
      }
      if (dependency->depth != 0) {
        PANIC(NULL, "CIRCULAR DEPENDENCY!!!\n");
      }

      dependency->next_resolve = stack;
      stack = dependency;
    }
  }
  return head;
}

void stage_run(struct init_stage* entry, bool smp) {
  while (entry != NULL) {
    if ((entry->flags & INIT_COMPLETE) && !smp) {
      klog("init: (WARN) attempting to run already ran target %s", entry->name);
    } else {
      if (smp) {
        // Run only SMP compatible entries
        if (!(entry->flags & INIT_SMP_READY)) {
          entry = entry->next;
          continue;
        }

        // Finally, actually run the target
        entry->func();
      } else {
        entry->func();
        entry->flags |= INIT_COMPLETE;
      }
    }

    entry = entry->next;
  }
}

///////////////////////////
//   Kernel Entrypoints
///////////////////////////
struct init_stage* resolved_stages = NULL;
void kern_smp_entry() {
  spinlock_acquire(&smp_entry_lock);

  // Just run the initgraph (smp) and we're done
  stage_run(resolved_stages, true);
  spinlock_release(&smp_entry_lock);
}

void kern_entry(struct stivale2_struct* bootinfo) {
  // Save bootinfo and zero rbp (for stacktracing)
  bootags = bootinfo;
  __asm__ volatile("xor %rbp, %rbp");

  // Generate the initgraph and run it
  resolved_stages = stage_resolve(root_stage);
  stage_run(resolved_stages, false);
  klog("init: completed all targets, going to rest!");

  // Wait for ACPI power interrupts...
  for (;;) {
    asm volatile("sti; hlt");
  }
}
