#include <lib/builtin.h>
#include <lib/log.h>
#include <vm/phys.h>
#include <vm/vm.h>

struct vm_zone *head_zone, *tail_zone = NULL;

bool
vm_zone_possible(uintptr_t base, uint64_t len)
{
  // Try to align the zone to 2MB
  uintptr_t aligned_base = (base + 0x1FFFFF) & ~(0x1FFFFF);
  uintptr_t top = base + len;

  // Make sure the alignment dosen't exceed the range
  if (aligned_base >= top)
    return false;

  // Now, everything should be page aligned.
  if (((aligned_base % VM_PAGE_SIZE) != 0) || ((top % VM_PAGE_SIZE) != 0))
    return false;

  // Finally, make sure that the zone is big enough to work with
  if ((top - aligned_base) < (32 * 0x100000))
    return false;

  // The zone should be fit for use
  return true;
}

void
vm_create_zone(uintptr_t base, uint64_t len)
{
  // NOTE: it is assumed that the zone has passed all checks already
  uintptr_t aligned_base = (base + 0x1FFFFF) & ~(0x1FFFFF);
  uintptr_t limit = base + len;
  uint64_t bitmap_size = DIV_ROUNDUP((limit - aligned_base), VM_PAGE_SIZE) / 8;

  // Create the zone and bitmap, then realign the base
  struct vm_zone* zone = (struct vm_zone*)(aligned_base + VM_MEM_OFFSET);
  memset(zone, 0, sizeof(struct vm_zone));
  aligned_base += sizeof(struct vm_zone);
  zone->bitmap = (uint8_t*)(aligned_base + VM_MEM_OFFSET);
  aligned_base += bitmap_size;
  aligned_base = (aligned_base + 0x1FFFFF) & ~(0x1FFFFF);

  // Fill in the zone, and clear the bitmap
  zone->prev = zone->next = NULL;
  zone->domain = 0;
  zone->base = aligned_base;
  zone->limit = limit;
  zone->bitmap_len = bitmap_size;
  memset(zone->bitmap, 0, zone->bitmap_len);

  // Insert the zone into the list
  if (head_zone == NULL) {
    head_zone = zone;
    tail_zone = zone;
  } else {
    tail_zone->next = zone;
    zone->prev = head_zone;
  }

  log("vm/zone: created zone [0x%lx - 0x%lx] (%u KiB)",
      aligned_base,
      zone->limit,
      zone->bitmap_len / 1000);
}

void*
vm_zone_alloc(struct vm_zone* zn, size_t pages, size_t align)
{
  spinlock_acquire(&zn->lck);

  // Align the previous allocation to our align
  uintptr_t real_base = DIV_ROUNDUP(zn->base + (zn->last_index * VM_PAGE_SIZE),
                                    align * VM_PAGE_SIZE) *
                        (align * VM_PAGE_SIZE);

  // Find a suitable base, then allocate from it
  for (size_t i = ((real_base - zn->base) / VM_PAGE_SIZE); i < zn->bitmap_len;
       i += align) {
    // Check to see if we have exceeded the boundaries
    if (zn->bitmap_len < (i + pages)) {
      spinlock_release(&zn->lck);
      return NULL;
    }

    // Try to nab all the pages we need
    for (size_t j = i, count = 0; j < (i + pages); j++) {
      if (BIT_TEST(zn->bitmap, j)) {
        real_base += align * VM_PAGE_SIZE;
        break;
      }

      // We have all the pages we need :-)
      if (++count == pages) {
        for (size_t z = 0; z < count; z++) {
          BIT_SET(zn->bitmap, i + z);
        }

        spinlock_release(&zn->lck);
        zn->last_index = (real_base - zn->base) / VM_PAGE_SIZE;
        zn->last_index += pages;
        return (void*)real_base;
      }
    }
  }

  // TODO: Set last_index to zero and try again.
  spinlock_release(&zn->lck);
  return NULL;
}

void
vm_zone_free(struct vm_zone* zn, void* ptr, size_t pages)
{
  spinlock_acquire(&zn->lck);

  uintptr_t idx = (uintptr_t)ptr / VM_PAGE_SIZE;
  for (size_t i = idx; i < idx + pages; i++)
    BIT_CLEAR(zn->bitmap, i);

  spinlock_release(&zn->lck);
}
