#include <arch/arch.h>
#include <arch/cpu.h>
#include <arch/tables.h>
#include <arch/smp.h>
#include <arch/timer.h>
#include <lib/cmdline.h>
#include <lib/kcon.h>
#include <lib/builtin.h>
#include <lib/stivale2.h>
#include <ninex/syscall.h>
#include <ninex/acpi.h>
#include <vm/vm.h>

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

  // Boot all other cores, and calibrate the TSC/APIC Timer
  smp_startup();
  timer_cali();
  acpi_enter_ospm();
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

void mg_enable() {
  if (CPU_CHECK(CPU_FEAT_SMAP))
    asm volatile ("clac" ::: "cc");
}

void mg_disable() {
  if (CPU_CHECK(CPU_FEAT_SMAP))
    asm volatile ("stac" ::: "cc");
}

bool mg_validate(void* ptr, size_t len) {
  if (len <= VM_PAGE_SIZE) {
    // TODO: Check if range is mapped
    if ((uintptr_t)ptr > VM_MEM_OFFSET)
      return false;
    else if (ptr == NULL)
      return false;
  } else {
    for (size_t i = 0; i < len; i += VM_PAGE_SIZE) {
      if (!mg_validate(i, VM_PAGE_SIZE))
        return false;
    }
  }

  return true;
}

bool mg_copy_to_user(void* usrptr, void* srcptr, size_t len) {
  mg_disable();

  // Check the entire address range we access, in uint32_t chunks
  if (!mg_validate(usrptr, len))
    return false;

  memcpy(usrptr, srcptr, len);
  mg_enable();
  return true;
}

bool mg_copy_from_user(void* kernptr, void* srcptr, size_t len) {
  mg_disable();

  // Check the entire address range we access, in uint32_t chunks
  if (!mg_validate(srcptr, len))
    return false;

  memcpy(kernptr, srcptr, len);
  mg_enable();
  return true;
}


