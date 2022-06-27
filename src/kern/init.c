#include <arch/arch.h>
#include <arch/irqchip.h>
#include <fs/vfs.h>
#include <lib/kcon.h>
#include <ninex/acpi.h>
#include <ninex/proc.h>
#include <ninex/sched.h>
#include <ninex/extension.h>
#include <vm/vm.h>
#include <arch/smp.h>

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

static struct stivale2_struct* bootinfo;
extern char __kern_stack_top[];
__attribute__((section(".stivale2hdr"),
               used)) static struct stivale2_header hdr = {
  .entry_point = 0,
  .stack = (uintptr_t)__kern_stack_top,
  .flags = (1 << 1) | (1 << 4),
  .tags = (uintptr_t)&five_lv_tag,
};

struct stivale2_struct* stivale2_get_struct() {
  return bootinfo;
}

void* stivale2_find_tag(uint64_t id) {
  struct stivale2_tag* current_tag = (struct stivale2_tag*)bootinfo->tags;
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

////////////////////////////
//   Kernel Entrypoints
////////////////////////////
static void kern_stage2()  {
  klog("init: completed all targets, entering userspace!");

  // Setup the init thread's args...
  const char *argv[] = { "/bin/dash", NULL };
  const char *envp[] = {
    "HOME=/",
    "PATH=/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin",
    "TERM=linux",
    NULL
  };

  // Load in the init process
  struct exec_args args = { .argp = argv, .envp = envp  };
  proc_t* init_proc = create_process(NULL, vm_space_create(), "/dev/ttyS0");
  thread_t* init_thread = uthread_create(init_proc, argv[0], args);

  if (init_thread == NULL) {
    PANIC(NULL, "PID 1 '%s' is missing from initrd!\n", argv[0]);
  } else {
    sched_queue(init_thread);

    // Now that our job is done, leave the init thread
    sched_leave();
  }
}

void kern_entry(struct stivale2_struct* bootdata) {
  // Save the boot information from the bootloader and zero rbp (for stacktracing)
  bootinfo = bootdata;
  __asm__ volatile("xor %rbp, %rbp");

  // Run all primary init functions...
  {
    arch_early_init();
    vm_setup();
    acpi_enable();
    arch_init();
    vfs_setup();
    kern_load_extensions();
  }

  // Create the kernel init thread, to finish the remaining parts of
  // initialization, and to launch userspace!
  thread_t* init_thread = kthread_create((uintptr_t)kern_stage2, 0);
  sched_queue(init_thread);
  sched_setup();

  // DO NOT PUT ANYTHING HERE, USE THE INIT THREAD INSTEAD!
  for (;;) {
    asm_halt(true);
  }
}
