#include <internal/asm.h>
#include <lib/lock.h>
#include <sys/tables.h>

static struct gdt main_gdt = { 0 };
static struct idt_entry entries[256] = { 0 };
static CREATE_SPINLOCK(table_lock);

static struct idt_entry
make_idt_entry(void* handler, uint8_t ist)
{
  uint64_t address = (uint64_t)handler;
  return (struct idt_entry){ .offset_low = (uint16_t)address,
                             .selector = 0x08,
                             .ist = ist,
                             .flags = 0x8e,
                             .offset_mid = (uint16_t)(address >> 16),
                             .offset_hi = (uint32_t)(address >> 32),
                             .reserved = 0 };
}

void
init_tables()
{
  // Setup the GDT
  main_gdt.null_entry = 0;
  main_gdt.kcode_entry = GDT_KERNEL_CODE_ENTRY;
  main_gdt.kdata_entry = GDT_KERNEL_DATA_ENTRY;
  main_gdt.ucode_entry = GDT_USER_CODE_ENTRY;
  main_gdt.udata_entry = GDT_USER_DATA_ENTRY;

  // Setup the TSS entry
  main_gdt.tss.length = 104;
  main_gdt.tss.base_low16 = 0;
  main_gdt.tss.base_mid8 = 0;
  main_gdt.tss.flags1 = 0b10001001;
  main_gdt.tss.flags2 = 0;
  main_gdt.tss.base_high8 = 0;
  main_gdt.tss.base_upper32 = 0;
  main_gdt.tss.reserved = 0;

  // Setup the IDT
  for (int i = 0; i < 256; i++) {
    entries[i] = make_idt_entry(asm_dispatch_table[i], 0);
  }

  // Set proper IST entries
  entries[2] = make_idt_entry(asm_dispatch_table[2], 1);
  entries[8] = make_idt_entry(asm_dispatch_table[8], 2);
  entries[14] = make_idt_entry(asm_dispatch_table[14], 3);
  entries[18] = make_idt_entry(asm_dispatch_table[18], 4);

  // Load both the IDT and GDT
  reload_tables();
}

void
reload_tables()
{
  struct table_ptr table_pointer;
  spinlock_acquire(&table_lock);

  // Reload the GDT
  table_pointer.base = (uint64_t)&main_gdt;
  table_pointer.limit = sizeof(struct gdt) - 1;
  asm_load_gdt(&table_pointer);

  // Zero memory in between, to prevent corruption
  memset(&table_pointer, 0, sizeof(struct table_ptr));

  // Reload the IDT
  table_pointer.base = (uint64_t)entries;
  table_pointer.limit = sizeof(entries) - 1;
  asm_load_idt(table_pointer);

  spinlock_release(&table_lock);
}

void
load_tss(uintptr_t address)
{
  spinlock_acquire(
    &table_lock); // We use a lock, becuase this modifies the global GDT

  main_gdt.tss.base_low16 = (uint16_t)address;
  main_gdt.tss.base_mid8 = (uint8_t)(address >> 16);
  main_gdt.tss.flags1 = 0b10001001;
  main_gdt.tss.flags2 = 0;
  main_gdt.tss.base_high8 = (uint8_t)(address >> 24);
  main_gdt.tss.base_upper32 = (uint32_t)(address >> 32);
  main_gdt.tss.reserved = 0;

  __asm__ volatile("ltr %0" ::"rm"((uint16_t)0x28) : "memory");
  spinlock_release(&table_lock);
}
