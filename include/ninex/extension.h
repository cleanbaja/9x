#ifndef NINEX_EXTENSION_H
#define NINEX_EXTENSION_H

#include <ninex/init.h>
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

EXPORT_STAGE(kext_stage);

#endif // NINEX_EXTENSION_H

