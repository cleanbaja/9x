#include <arch/pmap.h>
#include <arch/cpu.h>
#include <lvm/lvm_space.h>
#include <lvm/lvm_page.h>
#include <lib/panic.h>

#define KERNEL_VMA_5LV 0xFF00000000000000
#define KERNEL_VMA_4LV 0xFFFF800000000000

extern void lvm_setup_kspace();
volatile struct limine_hhdm_request hhdm_req = {
  .id = LIMINE_HHDM_REQUEST,
  .revision = 0
};

static inline uint64_t encode_pat_type(int flags) {
  int type = flags >> 16;
  uint64_t bits = 0;

  switch (type) {
    case LVM_CACHE_WC:
      bits = (bits & ~(1ull << 4)) | (1ull << 3); // PAT1
      break;

    // Use UC (Uncachable) instead of UC- (Strong Uncacheable) since UC lets
    // the BIOS override our mappings (via MTRRs), which is better since
    // BIOS devs often know what's better for the system than 9x
    case LVM_CACHE_NONE:
    case LVM_CACHE_DEVICE:
      bits = (1ull << 4) | (1ull << 3); // PAT3
      break;

    default:
      break; // Just use the default Write-Back mode (PAT0)
  }

  return bits;
}

static uintptr_t* iterate_level(uintptr_t* table, uint64_t index, bool create) {
  if (!(table[index] & PTE_P)) {
    if (!create)
      return NULL;

    table[index] = LVM_ALLOC_PAGE(true, LVM_PAGE_SYSTEM);
    table[index] |= PTE_P | PTE_W | PTE_U;
  }

  return (uintptr_t*)((table[index] & ~(0x1ff)) + LVM_HIGHER_HALF);
}

void pmap_insert(struct pmap* p, uintptr_t virt, uintptr_t phys, int flags) {
  uintptr_t pte = PTE_P | encode_pat_type(flags);
  uintptr_t *root = (uintptr_t*)((uintptr_t)p->root + LVM_HIGHER_HALF);

  uint64_t indices[] = {
#define INDEX(shift) ((virt & ((uint64_t)0x1ff << shift)) >> shift)
    INDEX(48), INDEX(39), INDEX(30), INDEX(21), INDEX(12)
#undef INDEX
  };

  if (flags & LVM_PERM_WRITE)
    pte |= PTE_W;
  if (!(flags & LVM_PERM_EXEC))
    pte |= PTE_NX;
  if (flags & LVM_TYPE_USER)
    pte |= PTE_U;
  if (flags & LVM_TYPE_GLOBAL)
    pte |= PTE_G;

  if (LVM_HIGHER_HALF == KERNEL_VMA_5LV)
    root = iterate_level(root, indices[0], true);

  root = iterate_level(root, indices[1], true);
  root = iterate_level(root, indices[2], true);
  if (flags & LVM_TYPE_HUGE) {
    root[indices[3]] = pte | phys | PTE_PS;
    return;
  }

  root = iterate_level(root, indices[3], true);

  root[indices[4]] = pte | phys;
}

void pmap_remove(struct pmap* p, uintptr_t virt) {
  uintptr_t *root = (uintptr_t*)((uintptr_t)p->root + LVM_HIGHER_HALF);

  uint64_t indices[] = {
#define INDEX(shift) ((virt & ((uint64_t)0x1ff << shift)) >> shift)
    INDEX(48), INDEX(39), INDEX(30), INDEX(21), INDEX(12)
#undef INDEX
  };

#define CHECK_PTE(ptr, idx)          \
  if (ptr == NULL) {                 \
    return; /* Non-Existent Page */  \
  } else if (ptr[idx] & PTE_PS) {    \
    ptr[idx] &= ~PTE_P;              \
    return; /* Huge page */          \
  }

  if (LVM_HIGHER_HALF == KERNEL_VMA_5LV) {
    root = iterate_level(root, indices[0], false);
    if (root == NULL)
      return;
  }

  root = iterate_level(root, indices[1], false);
  if (root == NULL)
    return;

  root = iterate_level(root, indices[2], false);
  CHECK_PTE(root, indices[3]);
  
  root = iterate_level(root, indices[3], false);
  CHECK_PTE(root, indices[4]);

  root[indices[4]] &= ~PTE_P;
}

void pmap_load(struct pmap* p) {
  uint64_t raw = p->root;
  if (has_feat(CPU_FEAT_PCID)) {
    raw |= (p->asid & 0xFFF);

    // don't modify TLB on CR3 load
    raw |= (1ull << 63);
  }
  
  cpu_write_cr3(raw);
}

void pmap_init() {
  // Make sure the higher half isn't shifted
  assert(LVM_HIGHER_HALF == KERNEL_VMA_4LV
      || LVM_HIGHER_HALF == KERNEL_VMA_5LV);

  // Setup the kernel space, map initial memory and reload
  kspace.p.root = LVM_ALLOC_PAGE(true, LVM_PAGE_SYSTEM);
  lvm_setup_kspace();
  lvm_space_load(&kspace);

  // Set PAT1 to Write-Combining (for the framebuffer)
  uint64_t pat = cpu_rdmsr(AMD64_PAT);
  pat |= (1ull << 8);
  cpu_wrmsr(AMD64_PAT, pat);
}
