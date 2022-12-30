/* src/kern/init.c - Kernel Initialization Routines
 * SPDX-License-Identifier: Apache-2.0 */

#include <ninex/console.h>
#include <lib/print.h>
#include <lib/panic.h>
#include <lib/cmdline.h>
#include <lib/lock.h>
#include <misc/limine.h>
#include <lvm/lvm.h>
#include <arch/cpu.h>

volatile static struct limine_bootloader_info_request info_req = {
  .id = LIMINE_BOOTLOADER_INFO_REQUEST,
  .revision = 0
};

__attribute__((noreturn)) void kern_entry(void) {
  // Setup the basics
  print_init();
  trap_init();
  cmdline_init();

  cpu_init();
  lvm_init();
  print_register_sink(console_sink);

  kprint("welcome to ninex!\n");
  kprint("Bootloader: %s [%s]\n", info_req.response->name, info_req.response->version);

  // Call it quits for now...
  for (;;) {
#if defined (__x86_64__)
    asm volatile ("cli; hlt");
#elif defined (__aarch64__)
    asm volatile ("msr DAIFSet, #15; wfi");
#endif
  }
}
