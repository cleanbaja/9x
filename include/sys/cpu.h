#ifndef SYS_CPU_H
#define SYS_CPU_H

#include <internal/asm.h>
#include <internal/stivale2.h>
#include <lib/vec.h>
#include <sys/tables.h>
#include <vm/virt.h>

// List of CPU features recognized by 9x
#define CPU_FEAT_FSGSBASE  (1 << 0)
#define CPU_FEAT_INVARIANT (1 << 1)
#define CPU_FEAT_DEADLINE  (1 << 2)
#define CPU_FEAT_PCID      (1 << 3)
#define CPU_FEAT_INVPCID   (1 << 4)
#define CPU_FEAT_SMAP      (1 << 5)
#define CPU_FEAT_TCE       (1 << 6)
#define CPU_CHECK(k) (cpu_features & k)
extern uint64_t cpu_features;

// Every CPU has this struct, and a pointer to it is located in the GS register
typedef struct {
  uint32_t cpu_num, lapic_id;
  uintptr_t kernel_stack, user_stack;
  struct tss tss;
  vm_space_t* cur_space;
  uint64_t tsc_freq, lapic_freq;
} percpu_t;

// CPU related functions/decls
void cpu_init(struct stivale2_struct_tag_smp *smp_tag);
void cpu_early_init();
void calibrate_tsc();
extern percpu_t** cpu_locals;
extern int cpu_count;

// A quick way to get the current CPUs number
static inline uint32_t __get_cpunum() {
  uint32_t ret;
  asm volatile("mov %%gs:0, %0" : "=r"(ret));
  return ret;
}

// Functions for reading/writing kernel GS
#define READ_PERCPU(ptr) (cpu_locals[__get_cpunum()])
#define WRITE_PERCPU(ptr, id)                                                  \
  ({                                                                           \
    asm_wrmsr(IA32_KERNEL_GS_BASE, (uint64_t)ptr);                             \
    asm_wrmsr(IA32_TSC_AUX, id);                                               \
    asm_swapgs();                                                              \
  })
#define per_cpu(k) ((READ_PERCPU())->k)
#define cpunum() __get_cpunum()

#endif // SYS_CPU_H

