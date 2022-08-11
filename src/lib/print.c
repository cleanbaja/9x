#include <lib/print.h>
#include <lib/libc.h>
#include <lib/lock.h>

#define NANOPRINTF_IMPLEMENTATION
#include <misc/nanoprintf.h>

#define MAX_PRINT_SINKS 16
static struct print_sink sinks[MAX_PRINT_SINKS];

// BOCHS/QEMU debugcon output
#ifdef __x86_64__
static void debugcon_write(char* str, size_t len) {
  asm volatile ("rep outsb" : "+S"(str), "+c"(len) : "d"(0xE9) : "memory"); 
}

static bool debugcon_init() {
  return true;
}

static struct print_sink debugcon_sink = {
  .write = debugcon_write,
  .init  = debugcon_init
};
#endif

// UART print output
#ifdef __aarch64__
#include <misc/stivale2.h>

static struct stivale2_struct_tag_mmio32_uart* uart_tag = NULL;
static struct stivale2_struct_tag_mmio32_uart_status* uart_ext_tag = NULL;

static inline void uart_putc(char c) {
  volatile uint32_t* write_addr = (volatile uint32_t*)uart_tag->addr;
  
  if (uart_ext_tag) {
    // Wait for the previous transaction to complete
    while ((*((volatile uint32_t*)uart_ext_tag->status) & uart_ext_tag->mask) != uart_ext_tag->value)
      asm ("yield");
  }

  if (c == '\n')
    *write_addr = (uint32_t)'\r';
  
  *write_addr = ((uint32_t)c & 0xFF);
}

static void uart_write(char* str, size_t len) {
  for (size_t i = 0; i < len; i++)
    uart_putc(str[i]);
}

static bool uart_init() {
  uart_tag = stivale2_get_tag(STIVALE2_STRUCT_TAG_MMIO32_UART);
  uart_ext_tag = stivale2_get_tag(STIVALE2_STRUCT_TAG_MMIO32_UART_STATUS);

  if (!uart_tag && !uart_ext_tag)
    return false;

  return true;
}

static struct print_sink uart_sink = {
  .write = uart_write,
  .init  = uart_init
};
#endif

int print_register_sink(struct print_sink bck) {
  for (int i = 0; i < MAX_PRINT_SINKS; i++) {
    if (!sinks[i].active && !sinks[i].write) {
      if (!bck.init())
        return -1;
      
      bck.active = true;
      sinks[i] = bck;
      return i;
    }
  }

  bck.active = false;
  return -1;
}

void print_disable(int spot) {
  if (spot >= MAX_PRINT_SINKS)
    return;

  sinks[spot].active = false;
}

void print_init() {
#if defined (__x86_64__)
  print_register_sink(debugcon_sink);
#elif defined (__aarch64__)
  print_register_sink(uart_sink);
#endif
}

static char buf[512] = {0};
void kprint(const char* fmt, ...) {
  static lock_t log_lock = SPINLOCK_INIT;
  spinlock(&log_lock);

  va_list va;
  va_start(va, fmt);
  int len = npf_vsnprintf(buf, 512, fmt, va);
  va_end(va);

  for (int i = 0; i < MAX_PRINT_SINKS; i++) {
    if (sinks[i].active)
      sinks[i].write(buf, len);
  } 

  memset(buf, 0, 512);
  spinrelease(&log_lock);
}

