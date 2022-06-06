#include <ninex/extension.h>
#include <lib/kcon.h>

#include "serial_priv.h"

bool serial_entry() {
  #ifdef __x86_64__
  rs232_init();
  #endif
  return true; // success!
}

void serial_unload() {
  return; // We don't have to do anything for now... 
}

DEFINE_EXTENSION(
    serial, "1.0.3", 
    serial_entry,
    serial_unload
);

