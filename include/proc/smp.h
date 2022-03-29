#ifndef PROC_SMP_H
#define PROC_SMP_H

#include <internal/asm.h>
#include <internal/stivale2.h>
#include <proc/proc.h>
#include <sys/tables.h>
#include <vm/virt.h>

// Every CPU has this struct, and a pointer to it is located in the GS register
typedef struct {
  uint32_t cpu_num, lapic_id;
  uintptr_t kernel_stack, user_stack;
  uint64_t errno;
  struct tss tss;
  vm_space_t* cur_space;
  uint64_t tsc_freq, lapic_freq;
  thread_t* cur_thread;
} percpu_t;

// List of active CPUs
extern percpu_t** cpu_locals;
extern uint64_t total_cpus, active_cpus;

// Functions for reading/writing the percpu structure
#define READ_PERCPU(ptr) (cpu_locals[__get_cpunum()])
#define WRITE_PERCPU(ptr, id)                                                  \
  ({                                                                           \
    asm_wrmsr(IA32_KERNEL_GS_BASE, (uint64_t)ptr);                             \
    asm_wrmsr(IA32_USER_GS_BASE, (uint64_t)ptr);                               \
    asm_wrmsr(IA32_TSC_AUX, id);                                               \
  })
#define per_cpu(k) ((READ_PERCPU())->k)
#define cpunum() __get_cpunum()

void smp_init(struct stivale2_struct_tag_smp *smp_tag);

#endif // PROC_SMP_H

