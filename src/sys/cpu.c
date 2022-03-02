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

// TODO: Rewrite the kernel init process,
// its really mangled at the moment

static void
ipi_halt(cpu_ctx_t* c, void* arg)
{
  (void)c;
  (void)arg;

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
  my_percpu->tss.rsp0 = my_percpu->kernel_stack;
  my_percpu->tss.ist1 =
    (uintptr_t)vm_phys_alloc(2) + VM_PAGE_SIZE + VM_MEM_OFFSET;
  my_percpu->tss.ist2 =
    (uintptr_t)vm_phys_alloc(2) + VM_PAGE_SIZE + VM_MEM_OFFSET;
  my_percpu->tss.ist3 =
    (uintptr_t)vm_phys_alloc(2) + VM_PAGE_SIZE + VM_MEM_OFFSET;
  my_percpu->tss.ist4 =
    (uintptr_t)vm_phys_alloc(2) + VM_PAGE_SIZE + VM_MEM_OFFSET;

  // Store it, along with the TSS
  WRITE_PERCPU(my_percpu);
  load_tss((uintptr_t)&my_percpu->tss);

  // Save it to the list, so we can get to it later...
  vec_push(&cpu_locals, my_percpu);
}

static void setup_cpu_features() {
  if (per_cpu(cpu_num) != 0)
    goto enable;

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
  }

  // Invariant TSC
  cpuid_subleaf(0x80000007, 0x0, &eax, &ebx, &ecx, &edx);
  if (edx & CPUID_EDX_INVARIANT) {
    cpu_features |= CPU_FEAT_INVARIANT;
  }

enable:
  if (CPU_CHECK(CPU_FEAT_FSGSBASE)) {
    asm_write_cr4(asm_read_cr4() | (1 << 16));
  }
}

static void ap_entry(struct stivale2_smp_info* info) {
  spinlock_acquire(&smp_lock);

  // Initialize basic CPU features
  reload_tables();
  percpu_init_vm();

  // Setup percpu stuff
  generate_percpu((void*)((uintptr_t)info + VM_MEM_OFFSET));

  // Enable the rest of the CPU features...
  setup_cpu_features();
  activate_apic();
  timer_init();

  // Let the BSP know we're done, and give other CPUs their turn
  spinlock_release(&smp_lock);
  ATOMIC_INC(&active_cpus);
  __asm__ volatile("xor %rbp, %rbp; sti");

  // Wait until scheduler is ready
  for (;;) {
    __asm__ volatile("sti; pause");
  }
}

void cpu_init(struct stivale2_struct_tag_smp *smp_tag) {
  // Set the handler for Halt IPIs
  struct irq_handler h = { .should_return = false,
                           .is_irq = true,
                           .hnd = ipi_halt };
  register_irq_handler(IPI_HALT, h);

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
    activate_apic();
    timer_init();
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

