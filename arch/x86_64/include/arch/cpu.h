#ifndef ARCH_CPU_H
#define ARCH_CPU_H

#include <ninex/proc.h>
#include <arch/asm.h>

// List of CPU features recognized by 9x
#define CPU_FEAT_RDPID     (1 << 0)
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

// Proc related functions
void cpu_create_kctx(thread_t* thrd, uintptr_t entry, uint64_t arg1);
void cpu_create_uctx(thread_t* thrd, struct exec_args args);
void cpu_save_thread(cpu_ctx_t* context);
void cpu_restore_thread(cpu_ctx_t* context);

// Tell if we're BSP
static inline bool is_bsp() {
  return (asm_rdmsr(0x1B) & (1 << 8));
}

#endif // ARCH_CPU_H

