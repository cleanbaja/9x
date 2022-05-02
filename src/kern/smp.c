#include <lib/lock.h>
#include <lib/kcon.h>
#include <lib/builtin.h>
#include <ninex/smp.h>
#include <vm/phys.h>
#include <vm/vm.h>

#define ARCH_INTERNAL
#include <arch/cpuid.h>
#include <arch/ic.h>
#include <arch/cpu.h>
#include <arch/tables.h>
#include <arch/timer.h>

percpu_vec_t cpu_locals;
uint64_t total_cpus = 0;
static uint64_t active_cpus = 1;
static CREATE_SPINLOCK(smp_lock);

static void
ipi_halt(cpu_ctx_t* c, void* arg) {
  (void)c;
  (void)arg;

  for (;;) {
    asm_halt();
  }
}

static percpu_t* setup_cpulocal(struct stivale2_smp_info* info) {
  // Create the percpu_t, and fill in the generic values...
  percpu_t* local = kmalloc(sizeof(percpu_t));
  memset(local, 0, sizeof(percpu_t));  // Zero it out, since it isn't empty for some reason
  local->cpu_num   = info->processor_id;
  local->cur_space = &kernel_space;
  
  // Setup stack related things, that are specific to x86_64...
  local->kernel_stack = info->target_stack;
  local->tss.rsp0 = local->tss.ist2 = local->kernel_stack;
  local->tss.ist1 =
    (uintptr_t)vm_phys_alloc(2, VM_ALLOC_ZERO) + (VM_PAGE_SIZE*2) + VM_MEM_OFFSET;
  local->tss.ist3 =
    (uintptr_t)vm_phys_alloc(2, VM_ALLOC_ZERO) + (VM_PAGE_SIZE*2) + VM_MEM_OFFSET;
  local->tss.ist4 =
    (uintptr_t)vm_phys_alloc(2, VM_ALLOC_ZERO) + (VM_PAGE_SIZE*2) + VM_MEM_OFFSET;

  // Finally, setup the remaining things, and save the percpu_t.
  local->lapic_id = info->lapic_id;
  local->self_ptr = (uintptr_t)local;
  vec_push(&cpu_locals, local);
  return local;
}

void ap_entry(struct stivale2_smp_info* sm) {
  spinlock_acquire(&smp_lock);

  // Get the CPU into a working state...
  // TODO: Refractor this into architecture specific stuff
  {
    reload_tables();
    cpu_early_init();
    percpu_init_vm();
    ic_enable();

    WRITE_PERCPU(sm->extra_argument, sm->processor_id);
    load_tss((uintptr_t)&per_cpu(tss));
    timer_calibrate_tsc();
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
                           .hnd = ipi_halt,
 			   .name = "Halt IPI" };
  *get_handler(IPI_HALT) = h;
  total_cpus = smp_tag->cpu_count + 1;

  // Wakeup all the waiting CPUs
  for (int i = 0; i < smp_tag->cpu_count; i++) {
    struct stivale2_smp_info* sp = &smp_tag->smp_info[i];
    if (sp->lapic_id == smp_tag->bsp_lapic_id) {
      percpu_t* p = setup_cpulocal(sp);
      WRITE_PERCPU(p, sp->processor_id);
      load_tss((uintptr_t)&per_cpu(tss));
      ic_enable();
      timer_calibrate_tsc();

      continue;
    }

    uint64_t stack = ((uint64_t)vm_phys_alloc(2, VM_ALLOC_ZERO) + VM_MEM_OFFSET);
    ATOMIC_WRITE((uint64_t*)&sp->target_stack, stack + (VM_PAGE_SIZE * 2));
    ATOMIC_WRITE((uint64_t*)&sp->extra_argument, (uint64_t)setup_cpulocal(sp));
    ATOMIC_WRITE((uint64_t*)&sp->goto_address, (uint64_t)ap_entry);
  } 

  // Wait for all CPUs to be ready
  while (ATOMIC_READ(&active_cpus) != smp_tag->cpu_count);
  klog("smp: %d CPUs initialized!", active_cpus);
}


