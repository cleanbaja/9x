#include <9x/vm.h>
#include <internal/cpuid.h>
#include <lib/lock.h>
#include <lib/log.h>
#include <sys/apic.h>
#include <sys/cpu.h>
#include <sys/tables.h>

uint64_t cpu_features = 0; 
static volatile int active_cpus = 1;
static CREATE_LOCK(smp_lock);

static void
ipi_halt()
{
  for (;;) {
    asm_halt();
  }
}

static void ap_entry(struct stivale2_smp_info* info) {
  // FIXME: Add locks to the various functions below, so that we don't have to use a lock here
  SLEEPLOCK_ACQUIRE(smp_lock);
    percpu_flush_gdt();
    percpu_flush_idt();
    percpu_init_vm();

    activate_apic();
  LOCK_RELEASE(smp_lock);

  if (CPU_CHECK(CPU_FEAT_FSGSBASE)) {
      asm_write_cr4(asm_read_cr4() | (1 << 16)); 
  }

  __asm__ volatile("cli");
  ATOMIC_INC(&active_cpus);

  for (;;) { __asm__ volatile("hlt"); }
}

void cpu_init(struct stivale2_struct_tag_smp *smp_tag) {
  uint32_t eax, ebx, ecx, edx;
  cpuid_subleaf(1, 0x0, &eax, &ebx, &ecx, &edx);

  if (ecx & CPUID_ECX_MONITOR) {
    cpu_features |= CPU_FEAT_MWAIT;
  }

  cpuid_subleaf(0x7, 0x0, &eax, &ebx, &ecx, &edx);
  if (ebx & CPUID_EBX_FSGSBASE) {
    cpu_features |= CPU_FEAT_FSGSBASE;
    asm_write_cr4(asm_read_cr4() | (1 << 16)); // Enable it
  }

  // Set the handler for Halt IPIs
  struct handler hnd = { .is_irq = true, .func = ipi_halt };
  idt_set_handler(hnd, IPI_HALT);

  activate_apic();
  
  // Detect and init all other CPUs
  for (int i = 0; i < smp_tag->cpu_count; i++) {
    struct stivale2_smp_info* sp = &smp_tag->smp_info[i];
    if (sp->lapic_id == smp_tag->bsp_lapic_id) {
      continue;
    }

    sp->target_stack = ((uint64_t)vm_phys_alloc(2) + VM_MEM_OFFSET);
    ATOMIC_WRITE((uint64_t*)&sp->goto_address, ap_entry);
  }

  while (ATOMIC_READ(&active_cpus) != smp_tag->cpu_count);
  log("smp: %d CPUs initialized!", active_cpus);

  
}

