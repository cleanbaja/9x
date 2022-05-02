#ifndef ARCH_CPU_H
#define ARCH_CPU_H

// Define ARCH_INTERNAL if needed, since this code
// could be imported from generic code
#ifndef ARCH_INTERNAL
#define ARCH_INTERNAL
#endif // ARCH_INTERNAL

#include <arch/asm.h>

// List of CPU features recognized by 9x
#define CPU_FEAT_FSGSBASE  (1 << 0)
#define CPU_FEAT_INVARIANT (1 << 1)
#define CPU_FEAT_DEADLINE  (1 << 2)
#define CPU_FEAT_PCID      (1 << 3)
#define CPU_FEAT_INVPCID   (1 << 4)
#define CPU_FEAT_SMAP      (1 << 5)
#define CPU_FEAT_TCE       (1 << 6)
#define CPU_FEAT_XSAVE     (1 << 7)
#define CPU_CHECK(k) (cpu_features & k)
extern uint64_t cpu_features;

// CPU related functions/decls
void cpu_early_init();
void calibrate_tsc();

// FPU releated functions
extern uint64_t fpu_save_size;
void fpu_save(uint8_t* zone);    // Must be 16 or 64-byte aligned
void fpu_restore(uint8_t* zone);

// Tell if we're BSP
static inline bool is_bsp() {
  return (asm_rdmsr(0x1B) & (1 << 8));
}

#endif // ARCH_CPU_H

