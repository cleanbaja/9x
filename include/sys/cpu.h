#ifndef SYS_CPU_H
#define SYS_CPU_H

#include <internal/stivale2.h>
#include <internal/asm.h>
#include <lib/vec.h>
#include <9x/vm.h>

#define CPU_FEAT_MWAIT    (1 << 0)
#define CPU_FEAT_FSGSBASE (1 << 1)
#define CPU_CHECK(k) (cpu_features & k)
extern uint64_t cpu_features;

// Interrupt vector that halts the CPU
#define IPI_HALT 254

// Every CPU has this struct, and a pointer to it is located in the GS register
typedef struct {
  uint32_t cpu_num, lapic_id;
  uintptr_t kernel_stack, user_stack;
  vm_space_t* cur_space;
} percpu_t;
#define per_cpu(k) ((read_percpu())->k)

static inline percpu_t* read_percpu() {
  percpu_t* ptr;

  // Try to use FSGSBASE (If supported)
  if (CPU_CHECK(CPU_FEAT_FSGSBASE)) {
    __asm__ volatile("rdgsbase %0" : "=r" (ptr) :: "memory");
  } else {
    ptr = (percpu_t*)asm_rdmsr(0xC0000102);
  }

  return ptr;
}
static inline void write_percpu(percpu_t* p) {
  if (CPU_CHECK(CPU_FEAT_FSGSBASE)) {
    __asm__ volatile("wrgsbase %0" : "=r" (p) :: "memory");
  } else {
    asm_wrmsr(0xC0000102, (uint64_t)p);
  }
}

void cpu_init(struct stivale2_struct_tag_smp *smp_tag);

typedef vec_t(percpu_t*) vec_percpu_t;
extern vec_percpu_t cpu_locals;

#endif // SYS_CPU_H

