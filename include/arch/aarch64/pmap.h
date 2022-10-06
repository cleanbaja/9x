#pragma once

#include <stdint.h>

#define LVM_PAGE_SIZE   0x1000
#define LVM_HUGE_FACTOR 512
#define LVM_HIGHER_HALF 0xFFFF800000000000

#define PTE_P   (1ull << 0)
#define PTE_TBL (1ull << 1)
#define PTE_U   (1ull << 6)
#define PTE_RO  (1ull << 7)
#define PTE_OSH (2ull << 8)
#define PTE_ISH (3ull << 8)
#define PTE_AF  (1ull << 10)
#define PTE_NG  (1ull << 11)
#define PTE_PXN (1ull << 53)
#define PTE_UXN (1ull << 54)
#define PTE_NX  (PTE_PXN | PTE_UXN)

#define PTE_FRAME(phys) ((phys & 0xFFF))

struct pmap {
  uintptr_t ttbr[2];
  uint16_t asid;
};

void pmap_insert(struct pmap* p, uintptr_t virt, uintptr_t phys, int flags);
void pmap_remove(struct pmap* p, uintptr_t virt);
void pmap_load(struct pmap* p);
void pmap_init();

extern uintptr_t kernel_vma;
