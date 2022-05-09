#include <arch/arch.h>
#include <arch/cpu.h>
#include <arch/tables.h>
#include <lib/cmdline.h>
#include <lib/stivale2.h>
#include <lib/kcon.h>

CREATE_STAGE(arch_early_stage, arch_early_callback, 0, {kcon_stage, cpu_init_stage, tables_setup_stage})

static void arch_early_callback() {
  struct stivale2_struct_tag_cmdline* cmdline_tag = (struct stivale2_struct_tag_cmdline*)stivale2_find_tag(STIVALE2_STRUCT_TAG_CMDLINE_ID);
  cmdline_load((char*)(cmdline_tag->cmdline));
}
