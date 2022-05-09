#ifndef PROC_SMP_H
#define PROC_SMP_H

#include <ninex/proc.h>
#include <ninex/init.h>
#include <arch/tables.h>
#include <lib/vec.h>
#include <vm/virt.h>

#ifndef ARCH_INTERNAL
#define ARCH_INTERNAL
#endif // ARCH_INTERNAL
#include <arch/asm.h>

// We mark this as packed, since its accessed in assembly, which can't put up with that.
typedef struct __attribute__((packed)) {
  // Generic per-cpu elements, standard across all archs.
  uintptr_t self_ptr;
  uint32_t cpu_num, lapic_id, errno;
  thread_t* cur_thread;
  struct thread_queue* local_q;
  vm_space_t* cur_space;

  // x86_64 specific stuff, like syscall stacks and TSC frequencies.
  // NOTE: on x86_64, the per-cpu structure has a pointer to itself,
  // so we can access it via GS relative reads
  uintptr_t kernel_stack, user_stack;
  uint64_t tsc_freq, lapic_freq;
  struct tss tss;
} percpu_t;

// List of CPU related structures and values.
typedef vec_t(percpu_t*) percpu_vec_t;
extern percpu_vec_t cpu_locals;
extern uint64_t total_cpus;

#ifdef __x86_64__

// Functions for reading/writing the percpu structure
#define READ_PERCPU(ptr) ((percpu_t*)__get_selfptr())
#define WRITE_PERCPU(ptr, id)                                                  \
  ({                                                                           \
    asm_wrmsr(IA32_KERNEL_GS_BASE, (uint64_t)ptr);                             \
    asm_wrmsr(IA32_USER_GS_BASE, (uint64_t)ptr);                               \
    asm_wrmsr(IA32_TSC_AUX, id);                                               \
  })
#define per_cpu(k) ((READ_PERCPU())->k)
#define cpunum() __get_cpunum()

#endif

EXPORT_STAGE(smp_stage);

#endif // PROC_SMP_H

