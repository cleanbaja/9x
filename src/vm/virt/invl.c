#include <vm/vm.h>
#include <vm/virt.h>
#include <sys/apic.h>
#include <sys/hat.h>
#include <proc/smp.h>
#include <lib/lock.h>

static uintptr_t invl_base, invl_length;
static int invl_mode;
static CREATE_SPINLOCK(invl_lock);

void ipi_invl(cpu_ctx_t* c, void* arg) {
  (void)c;
  (void)arg;

  switch (invl_mode) {
  case INVL_SINGLE_ADDR:
    for (uintptr_t i = invl_base; i < (invl_base + invl_length); i += VM_PAGE_SIZE) {
      hat_invl(per_cpu(cur_space)->root, i, per_cpu(cur_space)->asid, invl_mode);
    }
    break;
  case INVL_SINGLE_ASID:
    hat_invl(per_cpu(cur_space)->root, 0, per_cpu(cur_space)->asid, invl_mode);
    break;
  case INVL_ALL_ASIDS:
  case INVL_ENTIRE_TLB:
    hat_invl(per_cpu(cur_space)->root, 0, 0, invl_mode);
    break;
  }
}

void vm_invl_addr(vm_space_t* spc, uintptr_t address) {
  spinlock_acquire(&invl_lock);

  // Setup the IPI information
  invl_base = address;
  invl_length = 0x1000;
  invl_mode = INVL_SINGLE_ADDR;

  // Try to invalidate the other TLBs first
  if (cpu_locals != NULL) {
    for (int i = 0; i < active_cpus; i++) {
      if (cpu_locals[i]->cur_space == spc && cpu_locals[i] != READ_PERCPU()) {
        // Send a invalidation IPI
        apic_send_ipi(IPI_INVL_TLB, cpu_locals[i]->cpu_num, IPI_SPECIFIC);
      }
    }

    // Finally, invalidate our own TLB
    ipi_invl(NULL, NULL);
  } else {
    // If its this early in the boot process, then we're proably using the kernel space    
    for (uintptr_t i = invl_base; i < (invl_base + invl_length); i += VM_PAGE_SIZE) {
      hat_invl(kernel_space.root, i, kernel_space.asid, invl_mode);
    }
  }

  spinlock_release(&invl_lock);   
}

void vm_invl_range(vm_space_t* spc, uintptr_t address, uintptr_t len) {
  spinlock_acquire(&invl_lock);

  // Setup the IPI information
  invl_base = address;
  invl_length = len;
  invl_mode = INVL_SINGLE_ADDR;

  // Try to invalidate the other TLBs first
  if (cpu_locals != NULL) {
    for (int i = 0; i < active_cpus; i++) {
      if (cpu_locals[i]->cur_space == spc && cpu_locals[i] != READ_PERCPU()) {
        // Send a invalidation IPI
        apic_send_ipi(IPI_INVL_TLB, cpu_locals[i]->cpu_num, IPI_SPECIFIC);
      }
    }

    // Finally, invalidate our own TLB
    ipi_invl(NULL, NULL);
  } else {
    // If its this early in the boot process, then we're proably using the kernel space    
    for (uintptr_t i = invl_base; i < (invl_base + invl_length); i += VM_PAGE_SIZE) {
      hat_invl(kernel_space.root, i, kernel_space.asid, invl_mode);
    }
  }

  spinlock_release(&invl_lock); 
}

void vm_invl_asid(vm_space_t* spc, uint32_t asid) {
  spinlock_acquire(&invl_lock);

  // Setup the IPI information
  invl_base = invl_length = 0;
  invl_mode = INVL_SINGLE_ASID;

  // Try to invalidate the other TLBs first
  if (cpu_locals != NULL) {
    for (int i = 0; i < active_cpus; i++) {
      if (cpu_locals[i]->cur_space == spc && cpu_locals[i] != READ_PERCPU()) {
        // Send a invalidation IPI
        apic_send_ipi(IPI_INVL_TLB, cpu_locals[i]->cpu_num, IPI_SPECIFIC);
      }
    }
    // Finally, invalidate our own TLB
    ipi_invl(NULL, NULL);
  } else {
    // If its this early in the boot process, then we're proably using the kernel space    
    hat_invl(kernel_space.root, 0, kernel_space.asid, invl_mode);
  }

  spinlock_release(&invl_lock); 
}

