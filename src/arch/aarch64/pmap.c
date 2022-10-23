#include <arch/pmap.h>
#include <arch/cpu.h>
#include <lvm/lvm_space.h>
#include <lvm/lvm_page.h>
#include <misc/limine.h>
#include <lib/panic.h>

extern void lvm_setup_kspace();
volatile struct limine_hhdm_request hhdm_req = {
  .id = LIMINE_HHDM_REQUEST,
  .revision = 0
};

static inline uint64_t encode_mair_type(int flags) {
  int type = flags >> 16;
  uint64_t bits = 0;

  switch (type) {
    case LVM_CACHE_WC:
      bits = (1 << 2) | PTE_OSH;
      break;

    case LVM_CACHE_NONE:
      bits = (2 << 2) | PTE_OSH;
      break;

    case LVM_CACHE_DEVICE:
      bits = (3 << 2) | PTE_OSH;
      break;

    default:
      bits |= PTE_ISH; // Use the default (Write-Back Inner Shareable)
      break;
  }

  return bits;
}

static uintptr_t* iterate_level(uintptr_t* table, uint64_t index, bool create) {
  if (!(table[index] & PTE_P)) {
    if (!create)
      return NULL;

    table[index] = LVM_ALLOC_PAGE(true, LVM_PAGE_SYSTEM);
    table[index] |= PTE_P | PTE_TBL;
  }

  return (uintptr_t*)((table[index] & ~(0x1ff)) + LVM_HIGHER_HALF);
}

void pmap_insert(struct pmap* p, uintptr_t virt, uintptr_t phys, int flags) {
  uintptr_t pte = PTE_P | PTE_AF | PTE_TBL;
  bool top_half = virt & (1ull << 63);
  uintptr_t *root = (uintptr_t*)((uintptr_t)p->ttbr[top_half] + LVM_HIGHER_HALF);

  uint64_t indices[] = {
#define INDEX(shift) ((virt & ((uint64_t)0x1ff << shift)) >> shift)
    INDEX(39), INDEX(30), INDEX(21), INDEX(12)
#undef INDEX
  };

  if (!(flags & LVM_PERM_WRITE))
    pte |= PTE_RO;
  if (!(flags & LVM_PERM_EXEC))
    pte |= PTE_NX;
  if (!(flags & LVM_TYPE_GLOBAL))
    pte |= PTE_NG;
  if (flags & LVM_TYPE_USER)
    pte |= PTE_U;

  // since aarch64 doesn't have a hardware feature
  // like x86_64's SMEP, emulate it using the NX bits
  if (top_half)
    pte |= PTE_UXN;
  else
    pte |= PTE_PXN;

  root = iterate_level(root, indices[0], true);
  root = iterate_level(root, indices[1], true);
  if (flags & LVM_TYPE_HUGE) {
    root[indices[2]] = (pte | phys) & ~PTE_TBL;
    return;
  }

  root = iterate_level(root, indices[2], true);

  root[indices[3]] = pte | phys | PTE_ISH;
}

void pmap_remove(struct pmap* p, uintptr_t virt) {
  bool is_kern_addr = virt & (1ull << 63);
  uintptr_t *root = (uintptr_t*)((uintptr_t)p->ttbr[is_kern_addr] + LVM_HIGHER_HALF);

  uint64_t indices[] = {
#define INDEX(shift) ((virt & ((uint64_t)0x1ff << shift)) >> shift)
    INDEX(39), INDEX(30), INDEX(21), INDEX(12)
#undef INDEX
  };

#define CHECK_PTE(ptr, idx)           \
  if (ptr == NULL) {                  \
    return; /* Non-Existent Page */   \
  } else if (!(ptr[idx] & PTE_TBL)) { \
    ptr[idx] &= ~PTE_P;               \
    return; /* Huge page */           \
  }

  root = iterate_level(root, indices[0], false);
  CHECK_PTE(root, indices[1]);

  root = iterate_level(root, indices[1], false);
  CHECK_PTE(root, indices[2]);

  root = iterate_level(root, indices[2], false);
  CHECK_PTE(root, indices[3]);

  root[indices[3]] &= ~PTE_P;
}

void pmap_load(struct pmap* p) {
  // ttbr1 should never be reloaded, so don't even bother...
  // instead, reload only ttbr0
  asm volatile (
    "dsb st;\n\t"
    "msr ttbr0_el1, %0;\n\t"
    "dsb sy; isb\n\t" 
    :: "r"(p->ttbr[0] | p->asid)
  );
}

void pmap_init() {
  // Run some sanity checks, to make sure platform is good...
  uint64_t id_mmfr0 = cpu_read_sysreg(id_aa64mmfr0_el1);
  uint64_t fb_attr = ((cpu_read_sysreg(mair_el1) >> 8) & 0xFF);;
  uint64_t pa_bits = id_mmfr0 & 0xF;
  assert(hhdm_req.response->offset == 0xFFFF800000000000);

  // Certain systems (such as QEMU) map the framebuffer as 0xFF, even 
  // though it's device memory, so override it here...
  fb_attr = (fb_attr == 0xFF) ? 0b00001100 : 0xFF;

  if (((id_mmfr0 >> 28) & 0b1111) == 0b1111)
    panic(NULL, "pmap: CPU doesn't support 4KB translation granules\n");
  else if ((id_mmfr0 & 0xF) < 1)
    panic(NULL, "pmap: CPU doesn't support 48-bit physical addresses (max is %u)\n", (id_mmfr0 & 0xF));
  else if (!(id_mmfr0 & (1 << 5)))
    panic(NULL, "pmap: CPU doesn't support 16-bit ASIDs (only support 8-bit ASIDs)\n");

  // Setup the kernel space, and map initial memory
  kspace.p.ttbr[1] = LVM_ALLOC_PAGE(true, LVM_PAGE_SYSTEM);
  kspace.p.ttbr[0] = LVM_ALLOC_PAGE(true, LVM_PAGE_SYSTEM);
  lvm_setup_kspace();
  cpu_write_sysreg(ttbr1_el1, kspace.p.ttbr[1]);
  lvm_space_load(&kspace);

  // Setup MAIR (caching stuff)
  uint64_t mair = 
    (0b11111111 << 0) | // Normal, Write-back RW-Allocate non-transient
    (fb_attr    << 8) | // Framebuffer Memory, (not always write-combining)
    (0b00000000 << 16)| // Device, nGnRnE (aka uncachable)
    (0b00000100 << 24); // Normal, Uncachable (aka device memory)

  // Setup TCR (paging control)
  uint64_t tcr =
    (16 << 0)    | // T0SZ=16
    (16 << 16)   | // T1SZ=16
    (1 << 8)     | // TTBR0 Inner WB RW-Allocate
    (1 << 10)    | // TTBR0 Outer WB RW-Allocate
    (2 << 12)    | // TTBR0 Inner shareable
    (1 << 24)    | // TTBR1 Inner WB RW-Allocate
    (1 << 26)    | // TTBR1 Outer WB RW-Allocate
    (2 << 28)    | // TTBR1 Inner shareable
    (2ull << 30) | // TTBR1 4K granule
    (1ull << 36) | // 16-bit ASIDs
    ((uint64_t)pa_bits << 32); // 48-bit intermediate address

  cpu_write_sysreg(mair_el1, mair);
  cpu_write_sysreg(tcr_el1, tcr);
}
