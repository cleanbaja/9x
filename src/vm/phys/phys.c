#include <lib/builtin.h>
#include <lib/kcon.h>
#include <vm/phys.h>
#include <vm/vm.h>

static lock_t pmm_lock;

static struct vm_zone *find_zone(uintptr_t addr) {
  // Borrow the lock of the first zone
  spinlock(&pmm_lock);

  // Look for the zone that encompasses this address
  for (struct vm_zone *zn = head_zone; zn != NULL; zn = zn->next) {
    if (zn->base <= addr && addr <= zn->limit) {
      spinrelease(&pmm_lock);
      return zn;
    }
  }

  spinrelease(&pmm_lock);
  return NULL;
}

void *vm_phys_alloc(size_t pages, int flags) {
  size_t align = 0;

  // Process the flags
  if (flags & VM_ALLOC_HUGE) {
    align = 0x200;
    pages *= 0x200;
  } else {
    align = 1;
  }

  // Scan through every zone, looking for free pages
  for (struct vm_zone *zn = head_zone; zn != NULL; zn = zn->next) {
    void *ptr = vm_zone_alloc(zn, pages, align);
    if (ptr != NULL) {
      if (flags & VM_ALLOC_ZERO)
        memset64((void *)((uintptr_t)ptr + VM_MEM_OFFSET), 0,
                 pages * VM_PAGE_SIZE);

      return ptr;
    }
  }

  // We couldn't find anything, so return NULL
  klog("vm/phys: (WARN) Out of physical memory! (size: %u)", pages);
  return NULL;
}

void vm_phys_free(void *ptr, size_t count) {
  if (ptr == NULL) return;

  struct vm_zone *zn = find_zone((uintptr_t)ptr);
  if (zn == NULL) {
    klog("vm/phys: address 0x%lx dosen't fit in any zone!", ptr);
    return;
  } else {
    vm_zone_free(zn, ptr, count);
  }
}
