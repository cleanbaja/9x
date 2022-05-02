#include <lib/lock.h>
#include <arch/asm.h>
#include <arch/tables.h>

static struct gdt main_gdt = { 0 };
static struct idt_entry entries[256] = { 0 };
extern void* asm_dispatch_table[256];
static CREATE_SPINLOCK(table_lock);
extern void  asm_load_gdt(void* g, uint16_t codeseg, uint16_t dataseg);
#define asm_load_idt(ptr) __asm__ volatile("lidt %0" ::"m"(ptr))

static struct idt_entry
make_idt_entry(void* handler, uint8_t ist)
{
  uint64_t address = (uint64_t)handler;
  return (struct idt_entry){ .offset_low = (uint16_t)address,
                             .selector = GDT_KERNEL_CODE,
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

  // Add the Legacy Entries, if we're using the limine terminal
#ifdef LIMINE_EARLYCONSOLE
  main_gdt.ocode_entry = GDT_16BIT_CODE_ENTRY;
  main_gdt.odata_entry = GDT_16BIT_DATA_ENTRY;
  main_gdt.lcode_entry = GDT_LEGACY_CODE_ENTRY;
  main_gdt.ldata_entry = GDT_LEGACY_DATA_ENTRY; 
#endif // LIMINE_EARLYCONSOLE

  // Add the standard 64-bit user/kernel entries
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
  asm_load_gdt(&table_pointer, GDT_KERNEL_CODE, GDT_KERNEL_DATA);

  // Reload the IDT
  table_pointer.base = (uint64_t)entries;
  table_pointer.limit = sizeof(entries) - 1;
  asm_load_idt(table_pointer);

  spinlock_release(&table_lock);
}

void
load_tss(uintptr_t address)
{
  spinlock_acquire(&table_lock);

  // Activate the TSS, after configuring it...
  main_gdt.tss.base_low16 = (uint16_t)address;
  main_gdt.tss.base_mid8 = (uint8_t)(address >> 16);
  main_gdt.tss.flags1 = 0b10001001;
  main_gdt.tss.flags2 = 0;
  main_gdt.tss.base_high8 = (uint8_t)(address >> 24);
  main_gdt.tss.base_upper32 = (uint32_t)(address >> 32);
  main_gdt.tss.reserved = 0;
  __asm__ volatile("ltr %0" ::"rm"((uint16_t)GDT_TSS_SELECTOR) : "memory");

  // Update the IDT to become TSS aware
  entries[2] = make_idt_entry(asm_dispatch_table[2], 1);
  entries[8] = make_idt_entry(asm_dispatch_table[8], 3);
  entries[14] = make_idt_entry(asm_dispatch_table[14], 2);
  entries[18] = make_idt_entry(asm_dispatch_table[18], 4);
  
  // Then reload it
  struct table_ptr idt_pointer;
  idt_pointer.base = (uint64_t)entries;
  idt_pointer.limit = sizeof(entries) - 1;
  asm_load_idt(idt_pointer);

  spinlock_release(&table_lock);
}

/*
 * A list of execption messages stolen from the osdev wiki
 * NOTE: I removed some execptions that were obsolete
 */
static char* exc_table[] = { [0] = "Division by Zero",
                             [1] = "Debug",
                             [2] = "Non Maskable Interrupt",
                             [3] = "Breakpoint",
                             [4] = "Overflow",
                             [5] = "Out of Bounds",
                             [6] = "Invalid Opcode",
                             [8] = "Double Fault",
                             [10] = "Invalid TSS",
                             [11] = "Segment not present",
                             [12] = "Stack Exception",
                             [13] = "General Protection fault",
                             [14] = "Page fault",
                             [16] = "x87 Floating Point Exception",
                             [17] = "Alignment check",
                             [18] = "Machine check",
                             [19] = "SIMD floating point Exception",
                             [20] = "Virtualization Exception",
                             [30] = "Security Exception" };

void
dump_context(cpu_ctx_t* regs)
{
  klog_unlocked("Exception #%d (%s)\n", regs->int_no, exc_table[regs->int_no]);
  klog_unlocked("    Error Code: 0x%08lx, RIP: 0x%08lx, RSP: 0x%08lx\n",
          regs->ec,
          regs->rip,
          regs->rsp);
  klog_unlocked("    RAX: 0x%08lx, RBX: 0x%08lx, RCX: 0x%08lx, RDX: 0x%08lx\n",
          regs->rax,
          regs->rbx,
          regs->rcx,
          regs->rbx);
  klog_unlocked("    RSI: 0x%08lx, RDI: 0x%08lx, RSP: 0x%08lx, RBP: 0x%08lx\n",
          regs->rsi,
          regs->rdi,
          regs->rsp,
          regs->rbp);
  klog_unlocked("    R8:  0x%08lx, R9:  0x%08lx, R10: 0x%08lx, R11: 0x%08lx\n",
          regs->r8,
          regs->r9,
          regs->r10,
          regs->r11);
  klog_unlocked("    R12: 0x%08lx, R12: 0x%08lx, R13: 0x%08lx, R14: 0x%08lx\n",
          regs->r12,
          regs->r13,
          regs->r13,
          regs->r14);
  klog_unlocked("    R15: 0x%08lx, CS:  0x%08lx, SS:  0x%08lx\n\n",
          regs->r15,
          regs->cs,
          regs->ss);

  if (regs->int_no == 14) {
    // Print extra information in the case of a page fault
    uint64_t cr2_val = asm_read_cr2();
    klog_unlocked("Linear Address: 0x%lx\nConditions:\n", cr2_val);

    // See Intel x86 SDM Volume 3a Chapter 4.7
    uint64_t error_code = regs->ec;
    if (((error_code) & (1 << (0))))
      klog_unlocked("    - Page level protection violation\n");
    else
      klog_unlocked("    - Non-present page\n");

    if (((error_code) & (1 << (1))))
      klog_unlocked("    - Write\n");
    else
      klog_unlocked("    - Read\n");

    if (((error_code) & (1 << (2))))
      klog_unlocked("    - User access\n");
    else
      klog_unlocked("    - Supervisor access\n");

    if (((error_code) & (1 << (3))))
      klog_unlocked("    - Reserved bit set\n");

    if (((error_code) & (1 << (4))))
      klog_unlocked("    - Instruction fetch\n");

    if (((error_code) & (1 << (5))))
      klog_unlocked("    - Protection key violation\n");

    if (((error_code) & (1 << (15))))
      klog_unlocked("    - SGX violation\n");

    klog_unlocked("\n");
  }
}

