#include <arch/arch.h>
#include <arch/cpu.h>
#include <arch/tables.h>
#include <arch/smp.h>
#include <arch/timer.h>
#include <lib/cmdline.h>
#include <lib/kcon.h>
#include <lib/stivale2.h>
#include <ninex/syscall.h>
#include <ninex/acpi.h>

// The bare miniumum to get a x86_64 build of ninex to the early console
void arch_early_init() {
  // Initialize the kcon and parts of the CPU
  kcon_init();
  tables_install();
  cpu_early_init();

  // Finally, load the kernel cmdline
  struct stivale2_struct_tag_cmdline* cmdline_tag =
      (struct stivale2_struct_tag_cmdline*)stivale2_find_tag(
          STIVALE2_STRUCT_TAG_CMDLINE_ID);
  cmdline_load((char*)(cmdline_tag->cmdline));
}

void arch_init() {
  // Do some ACPI/APIC related stuff
  ic_enable();
  scan_madt();
  acpi_enter_ospm();

  // Boot all other cores, and calibrate the TSC/APIC Timer
  smp_startup();
  timer_cali();
}

void syscall_archctl(cpu_ctx_t* context) {
  switch (context->rdi) {
    case ARCHCTL_WRITE_FS:
      asm_wrmsr(IA32_FS_BASE, context->rsi);
      break;
    case ARCHCTL_READ_MSR:
      context->rax = asm_rdmsr(context->rsi);
      break;
    case ARCHCTL_WRITE_MSR:
      asm_wrmsr(context->rsi, context->rdx);
      break;
  }
}

