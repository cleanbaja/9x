/* src/kern/init.c - Kernel Initialization Routines
 * SPDX-License-Identifier: Apache-2.0 */

#include <misc/limine.h>
#include <lib/print.h>
#include <lib/panic.h>
#include <lib/cmdline.h>
#include <lib/lock.h>
#include <dev/console.h>
#include <lvm/lvm.h>
#include <arch/cpu.h>

volatile static struct limine_bootloader_info_request info_req = {
  .id = LIMINE_BOOTLOADER_INFO_REQUEST,
  .revision = 0
};

volatile static struct limine_kernel_file_request kfile_req = {
  .id = LIMINE_KERNEL_FILE_REQUEST,
  .revision = 0
};

__attribute__((noreturn)) void kern_entry(void) {
  // Initialize outputs and traps
  print_init();
  trap_init();
  console_init();

  kprint("welcome to ninex!\n");
  kprint("Bootloader: %s [%s]\n", info_req.response->name, info_req.response->version);
  
  // Load the kernel command line
  char* cmdline_raw = kfile_req.response->kernel_file->cmdline;
  if (cmdline_raw) cmdline_load(cmdline_raw);

  cpu_init();
  lvm_init();

  // Call it quits for now...
  for (;;) {
#if defined (__x86_64__)
    asm volatile ("cli; hlt");
#elif defined (__aarch64__)
    asm volatile ("msr DAIFSet, #15; wfi");
#endif
  }
}

