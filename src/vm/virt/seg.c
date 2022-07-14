#include <arch/hat.h>
#include <arch/smp.h>
#include <lib/builtin.h>
#include <lib/errno.h>
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
  if (spc->mmap_base == 0)
    spc->mmap_base = hat_get_base(HAT_BASE_USEG);

  spc->mmap_base -= len;
  return spc->mmap_base;
}

//////////////////////////////////
//      Anonymous Segments
//////////////////////////////////
static bool anon_fault(struct vm_seg* segment, size_t offset, enum vm_fault flags) {
  if (!verify_prot(flags, segment->prot))
    return false;

  // Align the offset to a page size
  if (offset % cur_config->page_size != 0)
    offset = ALIGN_DOWN(offset, cur_config->page_size);

  struct vm_page* pg = htab_find(&segment->pagelist, &offset, sizeof(size_t));
  if (pg == NULL) {
    // Create the page metadata now, before we map it in...
    pg = kmalloc(sizeof(struct vm_page));
  } else if (pg->unmapped) {
    return false;
  } else {
    klog("seg: fault on pre-mapped page???");
    return false;
  }

  // Perform a COW (Copy-on-Write), doing the actual allocation where needed
  if (flags & VM_FAULT_PROTECTION) {
    // Find the parent page, so that we can determine the refcount
    struct vm_seg* parent = segment->context;
    if (!parent || !(flags & VM_FAULT_WRITE))
      return false;

    // Don't do the actual copy unless the page has been touched
    struct vm_page* ppg = htab_find(&parent->pagelist, &offset, sizeof(size_t));
    if (ppg && ppg->refcount >= 1 && ppg->present) {
      uintptr_t phys_buffer = (uintptr_t)vm_phys_alloc(cur_config->page_size / 0x1000, 0);
      vm_map_range(this_cpu->cur_spc, phys_buffer, segment->base + offset,
                   cur_config->page_size, calculate_prot(segment->prot));

      // Copy in the parent page...
      memcpy((void*)(phys_buffer + VM_MEM_OFFSET),
             (void*)((uintptr_t)ppg->metadata + VM_MEM_OFFSET), 0x1000);

      // Finally, make our own copy 'pg', and insert it into our pagelist
      htab_insert(&segment->pagelist, &offset, sizeof(size_t), pg);
      pg->metadata = (void*)phys_buffer;
      pg->refcount = 1;
      pg->present  = 1;

      // Lower the parent's refcount, since we no longer rely on it
      ppg->refcount -= 1;
      goto finished;
    }
  }

  // Otherwise, allocate a zero page, and map it in...
  uintptr_t phys_window = (uintptr_t)vm_phys_alloc(cur_config->page_size / 0x1000, VM_ALLOC_ZERO);
  vm_map_range(this_cpu->cur_spc, phys_window, segment->base + offset,
               cur_config->page_size, calculate_prot(segment->prot));

  // Fill in the page metadata and push it in
  htab_insert(&segment->pagelist, &offset, sizeof(size_t), pg);
  pg->metadata = (void*)phys_window;
  pg->present  = 1;
  pg->refcount = 1;

finished:
  return true;
}

static struct vm_seg* anon_clone(struct vm_seg* segment, void* space) {
  // Copy the segment perfectly, except for the pagelist.
  struct vm_seg* new_segment = kmalloc(sizeof(struct vm_seg));
  *new_segment = *segment;
  new_segment->context = segment;
  memset(&new_segment->pagelist, 0, sizeof(struct hash_table));

  // Make sure the parent segment has permissions
  if (segment->prot & PROT_NONE) {
    klog("seg: attempt to clone a segment with no protection!");
    return segment;
  }

  // Next, map every present page in as read-only/read-write...
  for (size_t i = 0; i < segment->len; i += cur_config->page_size) {
    struct vm_page* pg = htab_find(&segment->pagelist, &i, sizeof(size_t));
    if (pg == NULL) {
      continue;
    } else {
      int flags = calculate_prot(segment->prot);
      if (segment->mode & MAP_PRIVATE)
        flags &= ~VM_PERM_WRITE; // Copy-on-Write

      pg->refcount++;
      vm_map_range(space, (uintptr_t)pg->metadata, segment->base + i, cur_config->page_size, flags);
    }
  }

  return new_segment;
}

static bool anon_unmap(struct vm_seg* segment, uintptr_t unmap_base, size_t unmap_len) {
  // Run some sanity checks on the unmap base/len
  if (unmap_base + unmap_len > segment->base + segment->len)
    return false;
  else if (unmap_base < segment->base || unmap_len > segment->len)
    return false;

  // Iterate over all the pages, deleting/unref'ing them if needed!
  for (size_t i = unmap_base; i < unmap_len; i += cur_config->page_size) {
    struct vm_page* pg = htab_find(&segment->pagelist, &i, sizeof(size_t));
    if (pg == NULL) {
      continue;
    } else {
      pg->refcount--;

      if (pg->refcount != 0) {
        vm_unmap_range(this_cpu->cur_spc, segment->base + i, cur_config->page_size);
        pg->unmapped = 1;
        continue;
      }

      vm_phys_free(pg->metadata, cur_config->page_size / VM_PAGE_SIZE);
      vm_unmap_range(this_cpu->cur_spc, segment->base + i, cur_config->page_size);
      htab_delete(&segment->pagelist, &i, sizeof(size_t));
      kfree(pg);
    }
  }

  return true;
}

static struct vm_seg* anon_create(vm_space_t* space, uintptr_t hint, uint64_t len, int prot, int mode) {
  // Make sure the flags are valid
  if (mode & MAP_SHARED) {
    klog("vm/seg: MAP_SHARED is not yet supported for anonymous mappings!");
    set_errno(ENOTSUP);
    return NULL;
  } else if (!(mode & MAP_PRIVATE)) {
    klog("vm/seg: either MAP_PRIVATE or MAP_SHARED is required for a anonymous mapping!");
	set_errno(EINVAL);
    return NULL;
  }

  // Create the initial segment
  struct vm_seg* segment = kmalloc(sizeof(struct vm_seg));
  segment->len = len;
  segment->prot = prot;
  segment->mode = mode;
  segment->ops.fault = anon_fault;
  segment->ops.clone = anon_clone;
  segment->ops.unmap = anon_unmap;

  // Find a suitable base for this segment
  if (!hint || !(mode & MAP_FIXED) || (hint % 0x1000 != 0)) {
    segment->base = alloc_mmap_base(space, len);
  } else {
    if (vm_find_seg(hint, NULL) != NULL) {
      klog("vm/seg: (WARN) hint 0x%lx tried to overwrite existing mapping!", hint);
      segment->base = alloc_mmap_base(space, len);
    } else {
      segment->base = hint;
    }
  }

  // Add segment to the current space's mappings
  memset(&segment->pagelist, 0, sizeof(struct hash_table));
  vec_push(&space->mappings, segment);
  return segment;
}

//////////////////////////////////
//      Segment functions
//////////////////////////////////
struct vm_seg* vm_find_seg(uintptr_t addr, size_t* offset) {
  vm_space_t* spc = this_cpu->cur_spc;

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

struct vm_seg* vm_create_seg(int mode, ...) {
  va_list va;
  va_start(va, mode);

  int prot = va_arg(va, int);
  uint64_t len = va_arg(va, uint64_t);
  uint64_t hint = 0;
  vm_space_t* space = this_cpu->cur_spc;
  struct vm_seg* sg = NULL;

  // Gather the remaining args
  if (mode & MAP_FIXED)
    hint = va_arg(va, uint64_t);

  if (mode & MAP_ANON) {
    sg = anon_create(space, hint, len, prot, mode);
  }

  va_end(va);
  return sg;
}
