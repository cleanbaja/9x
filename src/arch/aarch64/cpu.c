#include <arch/cpu.h>

#define TLBI(instr) \
  asm volatile ("dsb st; tlbi " #instr "; dsb sy; isb" ::: "memory");

#define TLBI_X(instr, value) \
  asm volatile ("dsb st; tlbi " #instr ", %0; dsb sy; isb" :: "r"(value) : "memory");

void tlb_flush_page(int asid, uintptr_t addr) {
  TLBI_X(vae1, ((uint64_t)asid << 48) | addr >> 12);
}

void tlb_flush_asid(int asid) {
  TLBI_X(aside1, (uint64_t)asid << 48);
}

void tlb_flush_global() {
  TLBI(alle1);
}

void cpu_init() {
  // Only thing on aarch64 that isn't enabled 
  // by default is the FPU, so do it here...
  cpu_write_sysreg(cpacr_el1, 0b11ull << 20);
}
