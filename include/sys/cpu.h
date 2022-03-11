#ifndef SYS_CPU_H
#define SYS_CPU_H

#include <9x/vm.h>
#include <internal/asm.h>
#include <internal/stivale2.h>
#include <lib/vec.h>
#include <sys/tables.h>

// List of CPU features recognized by 9x
#define CPU_FEAT_MWAIT     (1 << 0)
#define CPU_FEAT_FSGSBASE  (1 << 1)
#define CPU_FEAT_INVARIANT (1 << 2)
#define CPU_FEAT_DEADLINE  (1 << 3)
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
void calibrate_tsc();
typedef vec_t(percpu_t*) vec_percpu_t;
extern vec_percpu_t cpu_locals;

// A quick way to get the current CPUs number
static inline uint32_t __get_cpunum() {
  uint32_t ret;
  asm volatile ("rdtscp" : "=c" (ret) :: "rax", "rdx");
  return ret;
}

// Functions for reading/writing kernel GS
#define READ_PERCPU(ptr) (percpu_t*)asm_rdmsr(0xc0000102)
// #define READ_PERCPU(ptr) (cpu_locals.data[__get_cpunum()])
#define WRITE_PERCPU(ptr, id) ({               \
	asm_wrmsr(0xC0000102, (uint64_t)ptr);  \
	asm_wrmsr(0xC0000103, id);             \
})
#define per_cpu(k) ((READ_PERCPU())->k)
#define cpunum() __get_cpunum()

#endif // SYS_CPU_H

