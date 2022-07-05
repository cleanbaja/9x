#include <arch/smp.h>
#include <arch/timer.h>
#include <lib/lock.h>
#include <lib/builtin.h>
#include <lib/kcon.h>
#include <ninex/irq.h>
#include <vm/vm.h>

#include "config.h"

#ifdef LIMINE_EARLYCONSOLE
#include <lib/stivale2.h>
#include <arch/asm.h>
#endif

#define MAX_KCON_SINKS 10

// Define the stivale2 console sink, if wanted
#ifdef LIMINE_EARLYCONSOLE
static void (*stivale2_term_write)(const char *string, size_t length); 
static uint64_t limine_pagemap;

void stivale2_console_setup(const char* initial_buffer) {
  struct stivale2_struct_tag_terminal *term_str_tag;
  term_str_tag = stivale2_find_tag(STIVALE2_STRUCT_TAG_TERMINAL_ID);

  if (term_str_tag == NULL) {
    // What now, PANIC???
    for (;;) { asm_halt(false); }
  }

  void *term_write_ptr = (void *)term_str_tag->term_write;
  stivale2_term_write = term_write_ptr;

  // Save Limine's CR3 for later...
  limine_pagemap = asm_read_cr3();
}

void stivale2_console_write(const char* message) {
  uint64_t saved_cr3 = asm_read_cr3();
  asm_write_cr3(limine_pagemap);

  stivale2_term_write(message, strlen(message));
  asm_write_cr3(saved_cr3);
}

void stivale2_console_flush() {
  // Not needed for stiavle2 terminal
  return;
}

static struct kcon_sink stivale2_term_sink = {
  .setup = stivale2_console_setup,
  .write = stivale2_console_write,
  .flush = stivale2_console_flush
};

#endif // LIMINE_EARLYCONSOLE

// Define the BOCHS debugport output, if we're on x86_64
#ifdef __x86_64__
#include <arch/asm.h>

void bxdbg_console_write(const char* message) {
  while (*message != 0) {
    asm_outb(0xE9, *message);
    message++;
  }
}

void bxdbg_console_flush() {
  // Not needed for BOCHS debug console
  return;
}

void bxdbg_console_setup(const char* initial_buffer) {
  // Not needed for BOCHS debug console
  return;
}

static struct kcon_sink bxdbg_term_sink = {
  .setup = bxdbg_console_setup,
  .write = bxdbg_console_write,
  .flush = bxdbg_console_flush
};

#endif // __x86_64__

//////////////////////////////////////////////////
// Generic Kernel Console routines
//////////////////////////////////////////////////

static uint16_t num_sinks;
static struct kcon_sink* sinks[MAX_KCON_SINKS];
static lock_t kcon_lock;

void kcon_register_sink(struct kcon_sink* sink) {
  spinlock(&kcon_lock);
  if (num_sinks >= MAX_KCON_SINKS)
    return;

  sinks[num_sinks++] = sink;
  spinrelease(&kcon_lock);
}

static void halt_ipi(cpu_ctx_t* context) {
  (void)context;

  for (;;) {
    asm_halt(false);
  }
}

void kcon_init() {
  // Register the sinks
  #ifdef __x86_64__
  kcon_register_sink(&bxdbg_term_sink);
  #endif
  #ifdef LIMINE_EARLYCONSOLE
  kcon_register_sink(&stivale2_term_sink);
  #endif

  // Bootup all the sinks
  for (int i = 0; i < num_sinks; i++) {
    if (sinks[i] != 0) {
      sinks[i]->setup(NULL);
    }
  }

  // Print the 9x banner
  struct stivale2_struct* info = stivale2_get_struct();
  klog("9x (%s) (%s) - A project by cleanbaja", NINEX_VERSION, NINEX_ARCH);
  klog("Bootloader: %s [%s]", info->bootloader_brand, info->bootloader_version);

  // Set the HALT IPI handler...
  struct irq_resource* halt_irq = get_irq_handler(IPI_HALT);
  halt_irq->procfs_name  = "cpu_halt";
  halt_irq->HandlerFunc  = halt_ipi;
  halt_irq->eoi_strategy = EOI_MODE_EDGE;
}

void panic(void* frame, char* fmt, ...) {
  cpu_ctx_t* stack_frame = (cpu_ctx_t*)frame;
  static int in_panic = 0;

  // See if we're already in a panic
  if (in_panic == 1) {
    for (;;) {
      asm_halt(false);
    }
  } else {
    in_panic = 1;
  }

  // Shootdown all other CPUs
  ic_send_ipi(IPI_HALT, 0, IPI_OTHERS);

  // Log all the information possible
  klog_unlocked("\n--- KERNEL PANIC on CPU #%d ---\n", cpu_num);
  if (fmt) {
      va_list va;
      va_start(va, fmt);

      char realstr[512];
      vsnprintf(realstr, 512, fmt, va);
      va_end(va);

      klog_unlocked(realstr);
  }

  // Dump CPU contexts and stacktraces
  if (stack_frame) {
    dump_context(stack_frame);
    strace_unwind(stack_frame->rbp);
  } else {
    strace_unwind(0);
  }

  // Stop the scheduler on this CPU, and wait for ACPI interrupts
  timer_stop();
  for (;;) {
    asm_halt(false);
  }
}

//////////////////////////////////////////////////
// Logging/Formatting Code
//////////////////////////////////////////////////

static uint8_t initial_buffer[4096];
static bool console_locked = false;
uint64_t kcon_cursor = 0, kcon_size = 4096;
char* log_buf = (char*)initial_buffer;
static char format_buf[512], big_buf[512];

static void __buf_grow() {
  if ((uintptr_t)log_buf == (uintptr_t)initial_buffer) {
    void* new_buf = kmalloc(8192);
    memcpy(new_buf, log_buf, 4096);
    log_buf = new_buf;
    kcon_size = 8192;
  } else {
    log_buf = krealloc(log_buf, kcon_size * 2);
    kcon_size *= 2;
  }
}

static void __buf_write(char* str) {
  if ((kcon_cursor + strlen(str)) >= kcon_size) {
    __buf_grow();
  }

  memcpy((char*)(log_buf + kcon_cursor), str, strlen(str));
  kcon_cursor += strlen(str);
}

void klog(char* fmt, ...) {
  if (ATOMIC_READ(&console_locked))
    return;

  // Grab the spinlock and setup the va_args
  spinlock(&kcon_lock);
  va_list va;
  va_start(va, fmt);

  // Format the inner message, and the global format
  vsnprintf(format_buf, 512, fmt, va);
  snprintf(big_buf, 512, "[%*d.%06d] %s\n", 5, 0, 0, format_buf);
  va_end(va);

  // Write to all available sinks
  for (int i = 0; i < num_sinks; i++) {
    if (sinks[i] != 0) {
      sinks[i]->write(big_buf);
    }
  }

  // Then write to the internal buffer
  __buf_write(big_buf);

  // Return
  spinrelease(&kcon_lock);
}

void klog_unlocked(char* fmt, ...) {
  va_list va;
  va_start(va, fmt);

  // Format the outer buffer only
  vsnprintf(format_buf, 512, fmt, va);
  va_end(va);

  // Write to all available sinks
  for (int i = 0; i < num_sinks; i++) {
    if (sinks[i] != 0) {
      sinks[i]->write(format_buf);
    }
  }

  // Then write to the internal buffer
  __buf_write(format_buf);

  // Clear both buffers and return
  memset64(format_buf, 0, 512);
}

