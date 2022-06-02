#include <arch/arch.h>
#include <arch/cpu.h>
#include <arch/tables.h>
#include <lib/cmdline.h>
#include <lib/kcon.h>
#include <lib/stivale2.h>
#include <ninex/acpi.h>

// The bare miniumum to get a x86_64 build of ninex to the early console
CREATE_STAGE(arch_early_stage,
             arch_early_callback,
             {kcon_stage, cpu_init_stage, tables_setup_stage})

// All remaining arch-specific things, that depended on the VM...
CREATE_STAGE(arch_late_stage,
             DUMMY_CALLBACK,
             {scan_madt_target, acpi_late_stage})

static void arch_early_callback() {
  struct stivale2_struct_tag_cmdline* cmdline_tag =
      (struct stivale2_struct_tag_cmdline*)stivale2_find_tag(
          STIVALE2_STRUCT_TAG_CMDLINE_ID);
  cmdline_load((char*)(cmdline_tag->cmdline));
}

