#include <arch/asm.h>
#include <arch/hat.h>
#include <arch/smp.h>
#include <arch/tables.h>
#include <lib/kcon.h>
#include <lib/lock.h>
#include <ninex/irq.h>
#include <ninex/sched.h>

static struct gdt main_gdt = { 0 };
static struct idt_entry entries[256] = { 0 };
static struct table_ptr table_pointer;
static lock_t reload_lock;

// GDT entry definitons
#define GDT_16BIT_CODE_ENTRY  0x008F9A000000FFFF
#define GDT_16BIT_DATA_ENTRY  0x008F92000000FFFF
#define GDT_LEGACY_CODE_ENTRY 0x00CF9A000000FFFF
#define GDT_LEGACY_DATA_ENTRY 0x00CF92000000FFFF
#define GDT_KERNEL_CODE_ENTRY 0x00AF9A000000FFFF
#define GDT_KERNEL_DATA_ENTRY 0x008F92000000FFFF
#define GDT_USER_CODE_ENTRY   0x00AFFA000000FFFF
#define GDT_USER_DATA_ENTRY   0x008FF2000000FFFF

// External assembly definitions we need here
extern void* asm_dispatch_table[256];
extern void asm_load_gdt(void* gdt_ptr, uint16_t codeseg, uint16_t dataseg);

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

static void reload_tables() {
  // Reload the GDT
  table_pointer.base = (uint64_t)&main_gdt;
  table_pointer.limit = sizeof(struct gdt) - 1;
  asm_load_gdt(&table_pointer, GDT_KERNEL_CODE, GDT_KERNEL_DATA);

  // Reload the IDT
  table_pointer.base = (uint64_t)entries;
  table_pointer.limit = sizeof(entries) - 1;
  __asm__ volatile("lidt %0" ::"m"(table_pointer));
}

static void init_tables() {
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

void tables_install() {
  if (is_bsp())
    init_tables();
  else
    reload_tables();
}

void
load_tss(uintptr_t address)
{
  spinlock(&reload_lock);

  // Activate the TSS, after configuring it...
  main_gdt.tss.base_low16 = (uint16_t)address;
  main_gdt.tss.base_mid8 = (uint8_t)(address >> 16);
  main_gdt.tss.flags1 = 0b10001001;
  main_gdt.tss.flags2 = 0;
  main_gdt.tss.base_high8 = (uint8_t)(address >> 24);
  main_gdt.tss.base_upper32 = (uint32_t)(address >> 32);
  main_gdt.tss.reserved = 0;
  __asm__ volatile("ltr %0" ::"rm"((uint16_t)GDT_TSS_SELECTOR) : "memory");

  // Next, update the pf handler, and give it its own IST
  entries[14] = make_idt_entry(asm_dispatch_table[14], 1);
  table_pointer.base = (uint64_t)entries;
  table_pointer.limit = sizeof(entries) - 1;
  __asm__ volatile("lidt %0" ::"m"(table_pointer));

  spinrelease(&reload_lock);
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
                             [19] = "SIMD Floating Point Exception",
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
}

extern int resched_slot;
cpu_ctx_t* sys_dispatch_isr(cpu_ctx_t* context) {
  uint32_t vec = context->int_no;

  if (vec == 14) {
    handle_pf(context);
  } else if (vec < 32) {
    PANIC(context, NULL);
  } else if (vec == resched_slot) {
    ic_eoi();
    reschedule(context);
  } else {
    respond_irq(context, vec);
  }

  return context;
}
