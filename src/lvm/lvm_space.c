#include <lvm/lvm_space.h>
#include <lvm/lvm_page.h>
#include <lvm/lvm.h>
#include <lib/libc.h>

struct lvm_space kspace = {0};

void lvm_space_load(struct lvm_space *s) {
  pmap_load(&s->p);
  s->active = true;
}

void lvm_map_page(struct lvm_space* s, uintptr_t virt, uintptr_t phys, size_t size, int flags) {
  int inc_size = LVM_PAGE_SIZE;
  bool can_do_huge = ((virt % (LVM_PAGE_SIZE*LVM_HUGE_FACTOR)) == 0)
                     && ((phys % (LVM_PAGE_SIZE*LVM_HUGE_FACTOR)) == 0);

  if (can_do_huge) {
    flags |= LVM_TYPE_HUGE;
    inc_size *= LVM_HUGE_FACTOR;
  }

  for (size_t i = 0; i < size; i += inc_size)
    pmap_insert(&s->p, virt+i, phys+i, flags);

  // TODO(cleanbaja): TLB invalidation for SMP systems
  if (s->active) {
    for (size_t i = 0; i < size; i += inc_size) {
      tlb_flush_page(s->p.asid, virt+i);
    }
  }
}

void lvm_unmap_page(struct lvm_space* s, uintptr_t virt, size_t size) {
  int inc_size = LVM_PAGE_SIZE;

  if ((virt % (LVM_PAGE_SIZE*LVM_HUGE_FACTOR)) == 0)
    inc_size *= LVM_HUGE_FACTOR;

  for (size_t i = 0; i < size; i += inc_size)
    pmap_remove(&s->p, virt+i);

  // TODO(cleanbaja): TLB invalidation for SMP systems
  if (s->active) {
    for (size_t i = 0; i < size; i += inc_size) {
      tlb_flush_page(s->p.asid, virt+i);
    }
  }
}

bool lvm_fault(struct lvm_space *s, uintptr_t addr, enum lvm_fault_flags flags) {
    uintptr_t base = ALIGN_DOWN(addr, LVM_PAGE_SIZE);

    if (addr >= LVM_HEAP_START && addr < LVM_HEAP_END) {
        // Demand page the kernel heap
        int perms = LVM_PERM_READ | LVM_PERM_WRITE | LVM_TYPE_GLOBAL;
        uintptr_t mem = LVM_ALLOC_PAGE(false, LVM_PAGE_ALLOC);
        memset((void*)(mem + LVM_HIGHER_HALF), 0x0, LVM_PAGE_SIZE);

        lvm_map_page(s, base, mem, LVM_PAGE_SIZE, perms);
        return true;
    }

    return false;
}
