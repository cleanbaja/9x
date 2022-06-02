#include <arch/hat.h>
#include <lib/builtin.h>
#include <lib/kcon.h>
#include <vm/phys.h>
#include <vm/virt.h>
#include <vm/vm.h>

//////////////////////////////////
//       Helper functions
//////////////////////////////////
static bool verify_prot(enum vm_fault real_prot, int prot) {
  if ((real_prot & VM_FAULT_WRITE) && !(prot & PROT_WRITE))
    return false;

  if ((real_prot & VM_FAULT_EXEC) && !(prot & PROT_EXEC))
    return false;

  if (prot & PROT_NONE)
    return false;

  return true;
}
static int calculate_prot(int unix_prot) {
  int result = VM_PERM_READ | VM_PERM_USER;

  if (unix_prot & PROT_NONE)
    return 0;

  if (unix_prot & PROT_WRITE)
    result |= VM_PERM_WRITE;

  if (unix_prot & PROT_EXEC)
    result |= VM_PERM_EXEC;

  return result;
}
static uintptr_t alloc_mmap_base(vm_space_t* spc, size_t len) {
  // TODO: Throw this in the HAT's config
  if (spc->mmap_base == 0) {
    struct vm_config* cfg = cur_config;
    if (cfg->levels == 5)
      spc->mmap_base = 0x00fff00000000000 - (0x1000 * 512 * 512);
    else
      spc->mmap_base = 0x0000700000000000 - (0x1000 * 512 * 512);
  }

  spc->mmap_base -= len;
  return spc->mmap_base;
}

//////////////////////////////////
//      Pagelist functions
//////////////////////////////////
static void create_pagelist(struct vm_seg* seg) {
  struct vm_config* cfg = cur_config;
  for (uintptr_t i = seg->base; i < (seg->base + seg->len);
       i += cfg->page_size) {
    struct tracked_page* page = kmalloc(sizeof(struct tracked_page));
    page->virt = i;
    page->refcount = 0;

    vec_push(&seg->pagelist, page);
  }
}
static struct tracked_page* find_page(struct vm_seg* seg,
                                      size_t bottom,
                                      size_t top,
                                      uintptr_t addr) {
  // Perform a binary search to find the page (best algorithm for this purpose)
  if (top >= bottom) {
    size_t middle = bottom + (top - bottom) / 2;

    // Check to see if the element is dead in the center
    if (seg->pagelist.data[middle]->virt == addr)
      return seg->pagelist.data[middle];

    // If the element is bigger than the target, then its on the left
    if (seg->pagelist.data[middle]->virt > addr) {
      return find_page(seg, bottom, middle - 1, addr);
    } else {
      // Otherwise, the element is on the right
      return find_page(seg, middle + 1, top, addr);
    }
  }

  return NULL;
}

//////////////////////////////////
//      Anonymous Segments
//////////////////////////////////
static bool handle_anon_pf(struct vm_seg* segment,
                           size_t offset,
                           enum vm_fault flags) {
  struct vm_config* cfg = cur_config;
  if (!verify_prot(flags, segment->prot))
    return false;

  // Align the offset to a page size
  if (offset % cfg->page_size != 0)
    offset = ALIGN_DOWN(offset, cfg->page_size);

  // Find the page metadata, and assure that the page isn't unmapped
  struct tracked_page* pg =
      find_page(segment, 0, segment->pagelist.length, segment->base + offset);
  if (pg->state == TRACKED_PAGE_UNMAPPED)
    return false;

  // Perform a COW (Copy-on-Write), doing the actual allocation where needed
  if ((flags & VM_FAULT_PROTECTION) && (flags & VM_FAULT_WRITE)) {
    // Don't do the actual copy unless the page has been touched by more than
    // one process
    if (pg->refcount >= 1 && pg->state == TRACKED_PAGE_PRESENT) {
      uintptr_t phys_buffer =
          (uintptr_t)vm_phys_alloc(cfg->page_size / 0x1000, VM_ALLOC_ZERO);
      vm_map_range(&kernel_space, phys_buffer, segment->base + offset,
                   cfg->page_size, calculate_prot(segment->prot));

      // Copy in the previous page, and bump the refcount
      memcpy((void*)(phys_buffer + VM_MEM_OFFSET),
             (void*)(pg->phys + VM_MEM_OFFSET), 0x1000);
      pg->refcount++;
      goto finished;
    }
  }

  // Otherwise, perform a standard allocation of 'page_size' bytes
  uintptr_t phys_window =
      (uintptr_t)vm_phys_alloc(cfg->page_size / 0x1000, VM_ALLOC_ZERO);
  vm_map_range(&kernel_space, phys_window, segment->base + offset,
               cfg->page_size, calculate_prot(segment->prot));

  // Fill in the page metadata...
  pg->state = TRACKED_PAGE_PRESENT;
  pg->phys = phys_window;
  pg->refcount++;

finished:
  // vm_invl(&kernel_space, segment->base + offset, cfg->page_size);
  return true;
}

struct vm_seg* create_anon_seg(uintptr_t hint,
                               uint64_t len,
                               int prot,
                               int mode) {
  // Make sure the flags are valid
  if (mode & MAP_SHARED) {
    klog("vm/seg: MAP_SHARED is not yet supported for anonymous mappings!");
    return NULL;
  } else if (!(mode & MAP_PRIVATE)) {
    klog(
        "vm/seg: either MAP_PRIVATE or MAP_SHARED is required for a anonymous "
        "mapping!");
    return NULL;
  }

  // Create the initial segment
  struct vm_seg* segment = kmalloc(sizeof(struct vm_seg));
  segment->len = len;
  segment->prot = prot;
  segment->mode = mode;
  segment->deal_with_pf = handle_anon_pf;

  // Find a suitable base for this segment
  if (!hint || !(mode & MAP_FIXED) || (hint % 0x1000 != 0)) {
    segment->base = alloc_mmap_base(&kernel_space, len);
  } else {
    if (vm_find_seg(hint, NULL) != NULL) {
      klog("vm/seg: (WARN) hint 0x%lx tried to overwrite existing mapping!",
           hint);
      segment->base = alloc_mmap_base(&kernel_space, len);
    } else {
      segment->base = hint;
    }
  }

  // Create the pagelist for this segment, before then adding this segment to
  // the global list!
  create_pagelist(segment);
  vec_push(&kernel_space.mappings, segment);
  return segment;
}

//////////////////////////////////
//      Segment functions
//////////////////////////////////
struct vm_seg* vm_find_seg(uintptr_t addr, size_t* offset) {
  vm_space_t* spc = &kernel_space;

  for (int i = 0; i < spc->mappings.length; i++) {
    struct vm_seg* sg = spc->mappings.data[i];
    if (sg->base <= addr && addr <= (sg->base + sg->len)) {
      if (offset)
        *offset = addr - sg->base;

      return sg;
    }
  }

  return NULL;
}

void vm_unmap_seg(struct vm_seg* seg, uintptr_t base, size_t len) {
  struct vm_config* cfg = cur_config;
  for (uintptr_t index = base; index < (base + len); index += cfg->page_size) {
    struct tracked_page* page = find_page(seg, 0, seg->pagelist.length, index);
    page->state = TRACKED_PAGE_UNMAPPED;
    page->refcount -= 1;

    if ((page->refcount == 0) && page->phys) {
      vm_phys_free((void*)page->phys, cfg->page_size / 0x1000);
      page->phys = 0;
    }
  }

  vm_unmap_range(&kernel_space, base, len);
}

void vm_destroy_seg(struct vm_seg* seg) {
  // Only unmap the segment if its shared
  if (seg->shared) {
    vm_unmap_seg(seg, seg->base, seg->len);
    return;
  }

  struct vm_config* cfg = cur_config;
  for (int i = 0; i < seg->pagelist.length; i++) {
    struct tracked_page* page = seg->pagelist.data[i];

    // Perform a cleanup, where needed...
    if (page->state == TRACKED_PAGE_PRESENT)
      vm_unmap_range(&kernel_space, page->virt, cfg->page_size);
    if (page->phys != 0)
      vm_phys_free((void*)page->phys, 1);
    kfree(page);
  }

  vec_deinit(&seg->pagelist);
  kfree(seg);
}

struct vm_seg* vm_create_seg(int mode, ...) {
  va_list va;
  va_start(va, mode);

  int prot = va_arg(va, int);
  if (mode & MAP_ANON) {
    uint64_t len = va_arg(va, uint64_t);

    struct vm_seg* sg = NULL;
    if (mode & MAP_FIXED) {
      sg = create_anon_seg(va_arg(va, uintptr_t), len, prot, mode);
    } else {
      sg = create_anon_seg(0, len, prot, mode);
    }

    va_end(va);
    return sg;
  } else {
    va_end(va);
    return NULL;
  }
}
