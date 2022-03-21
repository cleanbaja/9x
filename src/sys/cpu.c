#include <internal/cpuid.h>
#include <lib/lock.h>
#include <lib/log.h>
#include <sys/apic.h>
#include <sys/cpu.h>
#include <sys/tables.h>
#include <sys/timer.h>
#include <vm/phys.h>
#include <vm/vm.h>

uint64_t cpu_features = 0; 
percpu_t** cpu_locals = NULL;
int cpu_count = 0;
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

static void detect_cpu_features() {
  uint32_t eax, ebx, ecx, edx;

  cpuid_subleaf(1, 0x0, &eax, &ebx, &ecx, &edx);
  if (ecx & CPUID_ECX_DEADLINE) {
    cpu_features |= CPU_FEAT_DEADLINE;
  }
  if (ecx & CPUID_ECX_PCID) {
    cpu_features |= CPU_FEAT_PCID;
  }
  
  cpuid_subleaf(0x7, 0x0, &eax, &ebx, &ecx, &edx);
  if (ebx & CPUID_EBX_FSGSBASE) {
    cpu_features |= CPU_FEAT_FSGSBASE;
  }
  if (ebx & CPUID_EBX_SMAP) {
    cpu_features |= CPU_FEAT_SMAP;
  }
  if (ebx & CPUID_EBX_INVPCID) {
    cpu_features |= CPU_FEAT_INVPCID;
  }
  if (ecx & (1 << 16)) {
    kernel_vma = 0xFF00000000000000;
    extern enum { VM_5LV_PAGING, VM_4LV_PAGING } paging_mode;
    paging_mode  = VM_5LV_PAGING;

    log("cpu: la57 detected!");
  }
  
  cpuid_subleaf(0x80000007, 0x0, &eax, &ebx, &ecx, &edx);
  if (edx & CPUID_EDX_INVARIANT) {
    cpu_features |= CPU_FEAT_INVARIANT;
  }

  cpuid_subleaf(0x80000001, 0x0, &eax, &ebx, &ecx, &edx);
  if (ecx & CPUID_ECX_TCE) {
    cpu_features |= CPU_FEAT_TCE;
  }
  
  // Set the last bit so that we don't run this function more than once
  cpu_features |= (1ull << 63ull);
}

void cpu_early_init() {
  // Find features if we haven't done that already
  if (cpu_features == 0) detect_cpu_features();

  // Assert that certain CPU features are present...
  uint32_t eax, ebx, ecx, edx;
  cpuid_subleaf(CPUID_EXTEND_FUNCTION_1, 0x0, &eax, &ebx, &ecx, &edx);

  if (!(edx & CPUID_EDX_XD_BIT_AVIL))
    PANIC(NULL, "NX not supported!\n");

  cpuid_subleaf(0x1, 0, &eax, &ebx, &ecx, &edx);
  if (!(edx & CPUID_EDX_PGE))
    PANIC(NULL, "Global Pages not supported!\n");

  cpuid_subleaf(0x7, 0, &eax, &ebx, &ecx, &edx);
  if (!(ebx & CPUID_EBX_SMEP))
    PANIC(NULL, "SMEP not supported!\n");
  
  // Then enable all possible features
  uint64_t cr4 = asm_read_cr4();
  cr4 |= (1 << 2)  | // Stop userspace from reading the TSC
	 (1 << 7)  | // Enables Global Pages
         (1 << 9)  | // Allows for fxsave/fxrstor, along with SSE
	 (1 << 10) | // Allows for unmasked SSE exceptions
	 (1 << 20);  // Enables Supervisor Mode Execution Prevention
  asm_write_cr4(cr4);

  // Enable No-Execute pages
  uint64_t efer = asm_rdmsr(IA32_EFER);
  efer |= (1 << 11);
  asm_wrmsr(IA32_EFER, efer);

  /* Enable optional features, if supported! */
  cr4 = asm_read_cr4();
  
  // Enable Supervisor Mode Access Prevention
  cr4 |= (CPU_CHECK(CPU_FEAT_SMAP) << 21);
  
  // Enable PCID
  cr4 |= (CPU_CHECK(CPU_FEAT_PCID) << 17);
  
  // Enable FSGSBASE instructions
  cr4 |= (CPU_CHECK(CPU_FEAT_FSGSBASE) << 16);
  asm_write_cr4(cr4);

  // Enable Translation Cache Extension, a AMD only feature.
  efer = asm_rdmsr(IA32_EFER);
  efer |= (CPU_CHECK(CPU_FEAT_TCE) << 15);
  asm_wrmsr(IA32_EFER, efer);
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
    (uintptr_t)vm_phys_alloc(2, VM_ALLOC_ZERO) + VM_PAGE_SIZE + VM_MEM_OFFSET;
  my_percpu->tss.ist2 =
    (uintptr_t)vm_phys_alloc(2, VM_ALLOC_ZERO) + VM_PAGE_SIZE + VM_MEM_OFFSET;
  my_percpu->tss.ist3 =
    (uintptr_t)vm_phys_alloc(2, VM_ALLOC_ZERO) + VM_PAGE_SIZE + VM_MEM_OFFSET;
  my_percpu->tss.ist4 =
    (uintptr_t)vm_phys_alloc(2, VM_ALLOC_ZERO) + VM_PAGE_SIZE + VM_MEM_OFFSET;

  return my_percpu;
}

static void ap_entry(struct stivale2_smp_info* info) {
  info = (void*)((uintptr_t)info + VM_MEM_OFFSET);
  spinlock_acquire(&smp_lock);

  // Initialize basic CPU features
  cpu_early_init();
  reload_tables();
  percpu_init_vm();

  // Save and activate percpu stuff
  percpu_t* data = generate_percpu(info);
  cpu_locals[info->processor_id] = data;
  WRITE_PERCPU(data, info->processor_id);
  load_tss((uintptr_t)&per_cpu(tss));
  
  // Activate the remaining devices
  activate_apic();

  // Let the BSP know we're done, and give other CPUs their turn
  spinlock_release(&smp_lock);
  ATOMIC_INC(&active_cpus);
  __asm__ volatile("xor %rbp, %rbp; cli");

  // Take a nap while the kernel does its thing...
  for(;;) { __asm__ volatile ("hlt"); }
}

void cpu_init(struct stivale2_struct_tag_smp *smp_tag) {
  // Set the handler for Halt IPIs
  struct irq_handler h = { .should_return = false,
                           .is_irq = true,
                           .hnd = ipi_halt };
  register_irq_handler(IPI_HALT, h);

  cpu_locals = (percpu_t**)kmalloc(sizeof(void*) * smp_tag->cpu_count);
  cpu_count = smp_tag->cpu_count;

  // Init the BSP
  for (int i = 0; i < smp_tag->cpu_count; i++) {
    struct stivale2_smp_info* sp = &smp_tag->smp_info[i];
    if (sp->lapic_id != smp_tag->bsp_lapic_id) {
      continue;
    }
   
    // Load the newly created CPU local data
    percpu_t* data = generate_percpu(sp);
    cpu_locals[0] = data;
    WRITE_PERCPU(data, 0);
    load_tss((uintptr_t)&per_cpu(tss));

    activate_apic();
  }

  // Detect and init all other CPUs
  for (int i = 0; i < smp_tag->cpu_count; i++) {
    struct stivale2_smp_info* sp = &smp_tag->smp_info[i];
    if (sp->lapic_id == smp_tag->bsp_lapic_id) {
      continue;
    }

    sp->target_stack = ((uint64_t)vm_phys_alloc(2, VM_ALLOC_ZERO) + VM_MEM_OFFSET);
    ATOMIC_WRITE((uint64_t*)&sp->goto_address, ap_entry);
  }

  // Wait for all CPUs to be ready
  while (ATOMIC_READ(&active_cpus) != smp_tag->cpu_count);
  log("smp: %d CPUs initialized!", active_cpus);
}

