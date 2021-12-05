#include <lib/log.h>

#define BACKTRACE_MAX 20

struct kernel_symbol {
  uint64_t base;
  char *name;
};

__attribute__((weak)) struct kernel_symbol ksym_table[] = {
    {.base = UINT64_MAX, .name = ""}};

static int strace_table_present() {
  if (ksym_table[0].base == UINT64_MAX) {
    return 0;
  } else {
    return 1;
  }
}

static char *addr_to_name(uintptr_t addr, uint64_t *offset) {
  if (!strace_table_present()) {
    return NULL;
  }

  for (int i = 0; ksym_table[i + 1].base < UINT64_MAX; i++) {
    if (ksym_table[i].base < addr && ksym_table[i + 1].base >= addr) {
      *offset = addr - ksym_table[i].base;
      return ksym_table[i].name;
    }
  }

  return NULL;
}

void strace_unwind(uintptr_t *base) {
  if (base == 0) {
    // base = (uintptr_t)__builtin_frame_address(0);
    __asm__ volatile("movq %%rbp, %0" : "=g"(base)::"memory");
  }

  raw_log("Stacktrace:\n");
  for (;;) {
    uintptr_t old_bp = base[0];
    uintptr_t ret_addr = base[1];

    if (!ret_addr)
      break;

    uintptr_t offset;
    char *name = addr_to_name(ret_addr, &offset);

    if (name)
      raw_log("  * 0x%lx <%s+0x%lx>\n", ret_addr, name, offset);
    else
      raw_log("  * 0x%lx\n", ret_addr);

    if (!old_bp)
      break;

    base = (void *)old_bp;
  }
}
