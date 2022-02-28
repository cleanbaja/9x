#include <9x/vm.h>
#include <internal/cpuid.h>
#include <lib/lock.h>
#include <lib/log.h>
#include <sys/apic.h>
#include <sys/cpu.h>
#include <sys/timer.h>
#include <sys/tables.h>

uint64_t cpu_features = 0; 
vec_percpu_t cpu_locals;
static volatile int active_cpus = 1;
static CREATE_SPINLOCK(smp_lock);

static void
ipi_halt(ctx_t* c)
{
  (void)c;

  for (;;) {
    asm_halt();
  }
}

static void generate_percpu(struct stivale2_smp_info* rinfo) {
  // Create cpu_local data structure
  percpu_t* my_percpu     = (percpu_t*)kmalloc(sizeof(percpu_t));
  my_percpu->cpu_num      = rinfo->processor_id;
  my_percpu->lapic_id     = rinfo->lapic_id;
  my_percpu->kernel_stack = rinfo->target_stack;
  my_percpu->cur_space    = &kernel_space;

  // Store it
  write_percpu(my_percpu);
  spinlock_acquire(&smp_lock);
  vec_push(&cpu_locals, my_percpu);
  spinlock_release(&smp_lock);
}

static void setup_cpu_features() {
  uint32_t eax, ebx, ecx, edx;
  cpuid_subleaf(1, 0x0, &eax, &ebx, &ecx, &edx);

  // MONITOR/MWAIT and TSC Deadline
  if (ecx & CPUID_ECX_MONITOR) {
    cpu_features |= CPU_FEAT_MWAIT;
  }
  if (ecx & CPUID_ECX_DEADLINE) {
    cpu_features |= CPU_FEAT_DEADLINE;
  }

  // {RD,WR}FSGSBASE
  cpuid_subleaf(0x7, 0x0, &eax, &ebx, &ecx, &edx);
  if (ebx & CPUID_EBX_FSGSBASE) {
    cpu_features |= CPU_FEAT_FSGSBASE;
    asm_write_cr4(asm_read_cr4() | (1 << 16)); 
  }

  // Invariant TSC
  cpuid_subleaf(CPUID_EXTEND_INVA_TSC, 0, &eax, &ebx, &ecx, &edx);
  if (edx & CPUID_EDX_INVARIANT) {
    cpu_features |= CPU_FEAT_INVARIANT;
  }
}

static void ap_entry(struct stivale2_smp_info* info) {
  // Get the CPU up to speed
  spinlock_acquire(&smp_lock);
    percpu_flush_gdt();
    percpu_flush_idt();
    percpu_init_vm();

    activate_apic();
  spinlock_release(&smp_lock);

  // Setup percpu stuff
  generate_percpu((void*)((uintptr_t)info + VM_MEM_OFFSET));

  // Finish the remaining parts of CPU preperation, and tell the BSP we're done
  setup_cpu_features();
  timer_init();
  __asm__ volatile("xor %rbp, %rbp; sti");
  ATOMIC_INC(&active_cpus);

  // Wait until scheduler is setup and ready
  for (;;) { __asm__ volatile("hlt"); }
}

void cpu_init(struct stivale2_struct_tag_smp *smp_tag) {
  // Set the handler for Halt IPIs
  struct handler hnd = { .is_irq = true, .func = ipi_halt };
  idt_set_handler(hnd, IPI_HALT);

  // Init the BSP
  for (int i = 0; i < smp_tag->cpu_count; i++) {
    struct stivale2_smp_info* sp = &smp_tag->smp_info[i];
    if (sp->lapic_id != smp_tag->bsp_lapic_id) {
      continue;
    }

    // For the BSP, all we have to do is setup the cpu_local 
    // data, along with the Timer/APIC and features
    generate_percpu(sp);
    setup_cpu_features();
    timer_init();
    activate_apic();
  }

  // Detect and init all other CPUs
  for (int i = 0; i < smp_tag->cpu_count; i++) {
    struct stivale2_smp_info* sp = &smp_tag->smp_info[i];
    if (sp->lapic_id == smp_tag->bsp_lapic_id) {
      continue;
    }

    sp->target_stack = ((uint64_t)vm_phys_alloc(2) + VM_MEM_OFFSET);
    ATOMIC_WRITE((uint64_t*)&sp->goto_address, ap_entry);
  }

  // Wait for all CPUs to be ready
  while (ATOMIC_READ(&active_cpus) != smp_tag->cpu_count);
  log("smp: %d CPUs initialized!", active_cpus);
}

