#include <sys/tables.h>
#include <internal/asm.h>

static struct gdt main_gdt = {0};

void init_gdt() {
  main_gdt.null_entry = 0;
  main_gdt.kcode_entry = GDT_KERNEL_CODE_ENTRY;
  main_gdt.kdata_entry = GDT_KERNEL_DATA_ENTRY;
  main_gdt.ucode_entry = GDT_USER_CODE_ENTRY;
  main_gdt.udata_entry = GDT_USER_DATA_ENTRY;

  percpu_flush_gdt();
}

void percpu_flush_gdt() {
  struct table_ptr gdt_ptr = {0};
  gdt_ptr.base = (uint64_t)&main_gdt;
  gdt_ptr.limit = sizeof(struct gdt) - 1;

  asm_load_gdt(&gdt_ptr);
}

