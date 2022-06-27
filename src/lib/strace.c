#include <lib/kcon.h>
#include <lib/stivale2.h>
#include <lib/builtin.h>
#include <stdbool.h>

#define BACKTRACE_MAX 20

struct kernel_symbol
{
  uint64_t base;
  char* name;
};

extern __attribute__((weak)) struct kernel_symbol ksym_table[]; 
extern __attribute__((weak)) int ksym_elem_count;
static uintptr_t kernel_slide = UINT64_MAX;

static bool
strace_table_present()
{
  if (!ksym_table) {
    return false;
  } else {
    return true;
  }
}

static void find_kernel_slide() {
  struct stivale2_struct_tag_kernel_slide* kslide = stivale2_find_tag(STIVALE2_STRUCT_TAG_KERNEL_SLIDE_ID);
  if (kslide == NULL) {
    kernel_slide = 0;
  } else {
    kernel_slide = kslide->kernel_slide;
  }
} 

static char*
addr_to_name(uintptr_t addr, uint64_t* offset) {
  if (!strace_table_present()) {
    return NULL;
  }
  if (kernel_slide == UINT64_MAX)
    find_kernel_slide();

  uintptr_t prev_addr = 0;
  char *prev_name  = NULL;
  addr -= kernel_slide;

  for (size_t i = 0;;) {
    if (ksym_table[i].base > addr) {
      *offset = addr - prev_addr;
      return prev_name;
    }
        
    prev_addr = ksym_table[i].base;
    prev_name = ksym_table[i].name;

    i += 1;
    if (ksym_table[i].base == UINT64_MAX)
      break;
  }

  return NULL;
}

uintptr_t
strace_get_symbol(char* name) {
  if (name == NULL || !strace_table_present())
    return 0x0;

  if (kernel_slide == UINT64_MAX)
    find_kernel_slide();

  for (int i = 0; ksym_table[i + 1].base < UINT64_MAX; i++) {
    if (strcmp(name, ksym_table[i].name) == 0) {
      return ksym_table[i].base + kernel_slide;
    }
  }

  return 0x0;
}

void
strace_unwind(uintptr_t* base)
{
  if (base == 0) {
    __asm__ volatile("movq %%rbp, %0" : "=g"(base) :: "memory");
  }

  klog_unlocked("Stacktrace:\n");
  for (;;) {
    uintptr_t old_bp = base[0];
    uintptr_t ret_addr = base[1];

    if (!ret_addr)
      break;

    uintptr_t offset;
    char* name = addr_to_name(ret_addr, &offset);

    if (name)
      klog_unlocked("  * 0x%lx <%s+0x%lx>\n", ret_addr, name, offset);
    else
      klog_unlocked("  * 0x%lx\n", ret_addr);

    if (!old_bp)
      break;

    base = (void*)old_bp;
  }
}
