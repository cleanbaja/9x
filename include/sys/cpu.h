#ifndef SYS_CPU_H
#define SYS_CPU_H

#include <internal/stivale2.h>
#include <internal/asm.h>
#include <lib/vec.h>
#include <9x/vm.h>

// List of CPU features recognized by 9x
#define CPU_FEAT_MWAIT     (1 << 0)
#define CPU_FEAT_FSGSBASE  (1 << 1)
#define CPU_FEAT_INVARIANT (1 << 2)
#define CPU_FEAT_DEADLINE  (1 << 3)
#define CPU_CHECK(k) (cpu_features & k)
extern uint64_t cpu_features;

// Interrupt vector that halts the CPU
#define IPI_HALT 254

// Every CPU has this struct, and a pointer to it is located in the GS register
typedef struct {
  uint32_t cpu_num, lapic_id;
  uintptr_t kernel_stack, user_stack;
  vm_space_t* cur_space;
  uint64_t tsc_freq;
} percpu_t;
#define per_cpu(k) ((read_percpu())->k)

// CPU related function
void cpu_init(struct stivale2_struct_tag_smp *smp_tag);
void calibrate_tsc();
typedef vec_t(percpu_t*) vec_percpu_t;
extern vec_percpu_t cpu_locals;

// Functions for reading/writing kernel GS
static inline percpu_t* read_percpu() {
  percpu_t* ptr;
  ptr = (percpu_t*)asm_rdmsr(0xC0000102);
  return ptr;
}
static inline void write_percpu(percpu_t* p) {
  asm_wrmsr(0xC0000102, (uint64_t)p);
}

#endif // SYS_CPU_H

