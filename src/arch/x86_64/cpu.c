#include <arch/cpu.h>
#include <lvm/lvm_space.h>
#include <cpuid.h>

enum invl_mode {
  INVALIDATE_ADDR,
  INVALIDATE_ASID,
  INVALIDATE_FULL
};

static void (*invl_func)(enum invl_mode, int, uintptr_t);
uint64_t cpu_features = 0;

static void invpcid(enum invl_mode md, int asid, uintptr_t addr) {
  struct {
		uint64_t pcid;
		uintptr_t address;
	} desc = { (uint64_t)asid, addr };

  asm volatile ("invpcid %1, %0" : : "r"((uint64_t)md), "m"(desc) : "memory");
}

static void invlpgb(enum invl_mode md, int asid, uintptr_t addr) {
  uint64_t rdx_bits = ((uint64_t)asid << 16);
  uint64_t rax_bits = addr;

  switch (md) {
  case INVALIDATE_ADDR:
    rax_bits |= 0b11;
    break;
  case INVALIDATE_ASID:
    rax_bits |= 0b10;
    break;

  case INVALIDATE_FULL:
    // TODO(cleanbaja): find a way to get invlpgb to clean the TLB
    return;
  }

  asm volatile ("invlpgb" :: "a"(rax_bits), "d"(rdx_bits) : "memory");
}

static void inv_legacy(enum invl_mode md, int asid, uintptr_t addr) {
  switch (md) {
  case INVALIDATE_ADDR:
    asm volatile ("invlpg (%0)" :: "r"(addr) : "memory");
    break;

  case INVALIDATE_ASID: {
    if (!has_feat(CPU_FEAT_PCID))
      break;

    uint64_t old_cr3 = cpu_read_cr3();
    cpu_write_cr3(kspace.p.root | (uint64_t)asid);
    cpu_write_cr3(old_cr3);
    break;
  }

  case INVALIDATE_FULL:
    // is this even valid???
    cpu_write_cr3(cpu_read_cr3());
    break;
  }
}

void tlb_flush_page(int asid, uintptr_t addr) {
  (*invl_func)(INVALIDATE_ADDR, asid, addr);
}

void tlb_flush_asid(int asid) {
  (*invl_func)(INVALIDATE_ASID, asid, 0x0);
}

void tlb_flush_global() {
  (*invl_func)(INVALIDATE_FULL, 0x0, 0x0);
}

void cpu_init() {
  uint64_t a = 0, b = 0, c = 0, d = 0;
  __cpuid(1, a, b, c, d);

  if (c & (1 << 17)) {
    cpu_features |= CPU_FEAT_PCID;
    cpu_write_cr4(cpu_read_cr4() | 17);
  }

  // Enable SSE
  cpu_write_cr0(cpu_read_cr0() & ~0b100);
  cpu_write_cr0(cpu_read_cr0() | 0b10);
  cpu_write_cr4(cpu_read_cr4() | (0b11 << 9));

  // Enable global/no-execute pages (assumed to be supported)
  cpu_write_cr4(cpu_read_cr4() | (1 << 7));
  cpu_wrmsr(AMD64_EFER, cpu_rdmsr(AMD64_EFER) | (1 << 11));

  // Choose the appropriate TLB invalidation function
  invl_func = inv_legacy;

  __cpuid(0x80000008, a, b, c, d);
  if (b & (1 << 21))
    invl_func = invlpgb;

  __cpuid(8, a, b, c, d);
  if (b & (1 << 10))
    invl_func = invpcid;
}
