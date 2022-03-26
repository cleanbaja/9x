#include <internal/cpuid.h>
#include <lib/lock.h>
#include <lib/log.h>
#include <sys/apic.h>
#include <sys/cpu.h>
#include <sys/timer.h>
#include <sys/tables.h>
#include <vm/phys.h>
#include <vm/vm.h>

percpu_t** cpu_locals = NULL;
uint64_t active_cpus = 1;
static CREATE_SPINLOCK(smp_lock);

static void
ipi_halt(cpu_ctx_t* c, void* arg) {
  (void)c;
  (void)arg;

  for (;;) {
    asm_halt();
  }
}

static percpu_t* generate_percpu(struct stivale2_smp_info* rinfo) {
  // Create cpu_local data structure and fill in the basic parts
  percpu_t* my_percpu     = (percpu_t*)kmalloc(sizeof(percpu_t));
  my_percpu->cpu_num      = rinfo->processor_id;
  my_percpu->lapic_id     = rinfo->lapic_id;
  my_percpu->kernel_stack = rinfo->target_stack;
  my_percpu->cur_space    = &kernel_space;

  // Fill in the TSS
  my_percpu->tss.rsp0 = my_percpu->kernel_stack;
  my_percpu->tss.ist1 =
    (uintptr_t)vm_phys_alloc(2, VM_ALLOC_ZERO) + (VM_PAGE_SIZE*2) + VM_MEM_OFFSET;
  my_percpu->tss.ist2 =
    (uintptr_t)vm_phys_alloc(2, VM_ALLOC_ZERO) + (VM_PAGE_SIZE*2) + VM_MEM_OFFSET;
  my_percpu->tss.ist3 =
    (uintptr_t)vm_phys_alloc(2, VM_ALLOC_ZERO) + (VM_PAGE_SIZE*2) + VM_MEM_OFFSET;
  my_percpu->tss.ist4 =
    (uintptr_t)vm_phys_alloc(2, VM_ALLOC_ZERO) + (VM_PAGE_SIZE*2) + VM_MEM_OFFSET;

  // Store the percpu structure and get on with our boot
  cpu_locals[rinfo->processor_id] = my_percpu;
  return my_percpu;
}

void ap_entry(struct stivale2_smp_info* sm) {
  // Pad the SMP info, and get us ready...
  sm = (void*)((uintptr_t)sm + VM_MEM_OFFSET);
  spinlock_acquire(&smp_lock);

  // Get the CPU into a working state...
  {
    reload_tables();
    cpu_early_init();
    percpu_init_vm();
    apic_enable();

    WRITE_PERCPU(sm->extra_argument, sm->processor_id);
    load_tss((uintptr_t)&per_cpu(tss));
    tsc_calibrate();
  }

  // Release the CPU lock, and tell the BSP we're done!
  spinlock_release(&smp_lock);
  ATOMIC_INC(&active_cpus);

  // Go to sleep for now...
  __asm__ volatile ("xor %rbp, %rbp; sti");

  for (;;) { __asm__ volatile ("pause"); }
}

void smp_init(struct stivale2_struct_tag_smp *smp_tag) {
  // Set the handler for Halt IPIs
  struct irq_handler h = { .should_return = false,
                           .is_irq = true,
                           .hnd = ipi_halt };
  register_irq_handler(IPI_HALT, h);

  // Create the percpu array
  cpu_locals = (percpu_t**)kmalloc(sizeof(void*) * (smp_tag->cpu_count + 1));

  // Wakeup all the waiting CPUs
  for (int i = 0; i < smp_tag->cpu_count; i++) {
    struct stivale2_smp_info* sp = &smp_tag->smp_info[i];
    if (sp->lapic_id == smp_tag->bsp_lapic_id) {
      percpu_t* p = generate_percpu(sp);
      WRITE_PERCPU(p, sp->processor_id);
      load_tss((uintptr_t)&per_cpu(tss));
      apic_enable();
      tsc_calibrate();

      continue;
    }

    uint64_t stack = ((uint64_t)vm_phys_alloc(2, VM_ALLOC_ZERO) + VM_MEM_OFFSET);
    ATOMIC_WRITE((uint64_t*)&sp->target_stack, stack + (VM_PAGE_SIZE * 2));
    ATOMIC_WRITE((uint64_t*)&sp->extra_argument, (uint64_t)generate_percpu(sp));
    ATOMIC_WRITE((uint64_t*)&sp->goto_address, (uint64_t)ap_entry);
  } 

  // Wait for all CPUs to be ready
  while (ATOMIC_READ(&active_cpus) != smp_tag->cpu_count);
  log("smp: %d CPUs initialized!", active_cpus);
}


