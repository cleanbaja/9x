#ifndef NINEX_EXTENSION_H
#define NINEX_EXTENSION_H

#include <stdbool.h>
#include <stdint.h>

struct kernel_extension {
  char *name, *version;

  bool (*init)();
  void (*deinit)();

  // NOTE: drivers should not set these below
  // variables, they are reserved for the kernel
  // extension loader
  uintptr_t load_base, load_end;
};

#define DEFINE_EXTENSION(nam, ver, ini, fini)                            \
  static struct kernel_extension __attribute__((section(".kext"), used)) \
  nam = {.name = #nam, .version = ver, .init = ini, .deinit = fini};

void kern_load_extensions();

#endif  // NINEX_EXTENSION_H
