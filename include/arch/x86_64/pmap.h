#pragma once

#include <misc/limine.h>

#define LVM_PAGE_SIZE   0x1000
#define LVM_HUGE_FACTOR 512
#define LVM_HIGHER_HALF hhdm_req.response->offset

#define PTE_P  (1ull << 0)
#define PTE_W  (1ull << 1)
#define PTE_U  (1ull << 2)
#define PTE_A  (1ull << 5)
#define PTE_D  (1ull << 6)
#define PTE_PS (1ull << 7)
#define PTE_G  (1ull << 8)
#define PTE_NX (1ull << 63)

#define PTE_PKEY(key) ((key & 0b1111) << 59)
#define PTE_FRAME(phys) ((phys & 0xFFF))

struct pmap {
  uintptr_t root;
  uint16_t asid;
};

void pmap_insert(struct pmap* p, uintptr_t virt, uintptr_t phys, int flags);
void pmap_remove(struct pmap* p, uintptr_t virt);
void pmap_load(struct pmap* p);
void pmap_init();

extern volatile struct limine_hhdm_request hhdm_req;
