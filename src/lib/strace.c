#include <lib/log.h>

#define BACKTRACE_MAX 20

void strace_unwind(uintptr_t base) {
  if (base == 0)
    base = __builtin_frame_address(0);

  raw_log("Stacktrace:\n");
  if (base != 0) {
    // Backtrace the return address
    raw_log("  * 0x%lx\n", base);

    // Then backtrace the rest
    for (int i = 0; i < BACKTRACE_MAX; ++i) {
      if (*(uintptr_t *)base == 0) {
        break;
      }

      uintptr_t addr = ((uintptr_t *)base)[1];
      raw_log("  * 0x%lx\n", addr);
      base = *(uintptr_t *)base;
    }
  }
}
