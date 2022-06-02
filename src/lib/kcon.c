#include <arch/irqchip.h>
#include <lib/builtin.h>
#include <lib/kcon.h>
#include <lib/lock.h>
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
    for (;;) { __asm__ volatile ("cli; hlt"); }
  }

  void *term_write_ptr = (void *)term_str_tag->term_write;
  stivale2_term_write = term_write_ptr;

  // Load Limine's CR3 and print the initial buffer if needed
  limine_pagemap = asm_read_cr3();
  if (initial_buffer != NULL) {
    stivale2_term_write(initial_buffer, strlen(initial_buffer));
  }
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
CREATE_STAGE_NODEP(kcon_stage, kcon_init);
static CREATE_SPINLOCK(kcon_lock);

void kcon_register_sink(struct kcon_sink* sink) {
  spinlock_acquire(&kcon_lock);
  if (num_sinks >= MAX_KCON_SINKS)
    return;

  sinks[num_sinks++] = sink;
  spinlock_release(&kcon_lock);
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
  struct stivale2_struct* info = stivale2_find_tag(1);
  klog("9x (%s) (%s) - A project by cleanbaja", NINEX_VERSION, NINEX_ARCH);
  klog("Bootloader: %s [%s]", info->bootloader_brand, info->bootloader_version);
}

void panic(void* frame, char* fmt, ...) {
  cpu_ctx_t* stack_frame = (cpu_ctx_t*)frame;
  static int in_panic = 0;

  // See if we're already in a panic
  if (in_panic == 1) {                                                       
    for (;;) {
      asm_halt();
    } 
  } else {
    in_panic = 1; 
  }

  // Shootdown all other CPUs
  // ic_send_ipi(IPI_HALT, 0, IPI_OTHERS);

  // Log all the information possible
  // klog_unlocked("\nKERNEL PANIC on CPU #%d\n", cpunum());
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

  // Halt for now...
  for (;;) {
    asm_halt();
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
  if (ATOMIC_READ(&console_locked) == true)
    return;

  // Grab the spinlock and setup the va_args
  spinlock_acquire(&kcon_lock);
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

  // Clear both buffers and return
  memset64(format_buf, 0, 512);
  memset64(big_buf, 0, 512);
  spinlock_release(&kcon_lock);
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

#include <arch/irqchip.h>
void syscall_debuglog(cpu_ctx_t* context) {
  char* msg = (char*)context->rdi;
  klog("%s", msg);
}

