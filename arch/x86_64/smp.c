#include <arch/irqchip.h>
#include <arch/smp.h>
#include <arch/tables.h>
#include <lib/builtin.h>
#include <lib/kcon.h>
#include <ninex/acpi.h>
#include <vm/phys.h>
#include <vm/vm.h>

extern vec_t(madt_lapic_t*) madt_lapics;
static _Atomic(int) online_cores = 0;
struct madt_lapic_t* cur_lapic = NULL;
CREATE_STAGE(smp_stage, smp_startup, {scan_madt_target});

// Include the compiled smp trampoline
extern uint64_t smp_bootcode_begin[];
extern uint64_t smp_bootcode_end[];
asm(".global smp_bootcode_begin\n\t"
    "smp_bootcode_begin: \n\t"
    ".incbin \"" SMP_INCLUDE_DIR  "/smp_trampoline.bin\"\n\t"
    ".global smp_bootcode_end\n\t"
    "smp_bootcode_end:\n\t");

static void hello_ap(struct percpu_info* info) {
  // Save the information before clearing RBP
  asm_wrmsr(IA32_KERNEL_GS_BASE, (uint64_t)info);
  asm_wrmsr(IA32_TSC_AUX, info->proc_id);
  asm volatile("xor %rbp, %rbp");

  // Let the BSP know that we're online...
  online_cores++;
  kern_smp_entry();

  // The above function shouldn't return, but just in case...
  for (;;) {
    asm volatile("hlt");
  }
}

static void smp_startup() {
  // Map the first 2MB of memory, for the trampoline and stuff
  int flags = VM_PERM_READ | VM_PERM_WRITE | VM_PERM_EXEC | VM_PAGE_HUGE;
  vm_map_range(&kernel_space, 0, 0, (0x1000 * 512), flags);

  // Detect 5-level paging and the number of CPUs that are online-capable
  bool enable_la57 = (VM_MEM_OFFSET == 0xFF00000000000000) ? true : false;
  int expected_cpus = madt_lapics.length - 1;

  // Copy the trampoline into memory, and save the IDT
  memcpy((void*)0x80000, (void*)((uintptr_t)smp_bootcode_begin),
         (uintptr_t)smp_bootcode_end - (uintptr_t)smp_bootcode_begin);
  struct table_ptr saved_idtr;
  asm("sidtq %0" ::"m"(saved_idtr));

  // Loop through all the cores, booting them as needed
  for (int i = 0; i < madt_lapics.length; i++) {
    cur_lapic = madt_lapics.data[i];

    // Create the CPU local information (stored in GS)
    struct percpu_info* percpu = kmalloc(sizeof(struct percpu_info));
    percpu->lapic_id = cur_lapic->apic_id;
    percpu->proc_id = cur_lapic->processor_id;
    percpu->cur_spc = &kernel_space;
    percpu->kernel_stack = (uint64_t)vm_phys_alloc(16, VM_ALLOC_ZERO) +
                           VM_MEM_OFFSET + (VM_PAGE_SIZE * 16);

    if (!(cur_lapic->flags & 1)) {
      klog("smp: CPU core %d is disabled!", cur_lapic->processor_id);
      expected_cpus--;
      continue;
    } else if (cur_lapic->apic_id == get_lapic_id()) {
      asm_wrmsr(IA32_KERNEL_GS_BASE, (uint64_t)percpu);
      asm_wrmsr(IA32_TSC_AUX, cur_lapic->processor_id);
      continue;
    }

    // Fill in the bootinfo accordingly...
    uint64_t* bootinfo_ptr = (uint64_t*)0x82000;
    bootinfo_ptr[0] = percpu->kernel_stack;
    bootinfo_ptr[1] = kernel_space.root;
    bootinfo_ptr[2] = (uintptr_t)hello_ap;
    bootinfo_ptr[3] = (uint64_t)percpu;
    bootinfo_ptr[4] = (uintptr_t)&saved_idtr;
    if (enable_la57)
      bootinfo_ptr[5] = 1;
    else
      bootinfo_ptr[5] = 0;

    // Send the wakeup ipi
    ic_perform_startup(cur_lapic->apic_id);
  }

  // Wait for all cores to boot, then off we go!
  while (online_cores != expected_cpus)
    ;
  vm_unmap_range(&kernel_space, 0, (0x1000 * 512));
  klog("smp: a total of %d cores booted!", online_cores);
}
