#ifndef NINEX_EXTENSION_H
#define NINEX_EXTENSION_H

#include <stdbool.h>

struct kernel_extension {
  char *name, *version;

  bool (*init)();
  void (*deinit)(); 
};

#define DEFINE_EXTENSION(nam, ver, ini, fini)          \
  static struct kernel_extension                       \
    __attribute__((section(".kext"), used)) nam = {    \
    .name = #nam,                                      \
    .version = ver,                                    \
    .init = ini,                                       \
    .deinit = fini                                     \
  };

void kern_load_extensions();

#endif // NINEX_EXTENSION_H

