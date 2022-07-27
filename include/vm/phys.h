#ifndef VM_PHYS_H
#define VM_PHYS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "lib/lock.h"

// The kernel physical memory allocator is underpinned
// by a zone-based allocation scheme, in which every free part
// of the memory map is converted into a zone. This not only
// allows for more robust allocations, but it also sets the
// path for NUMA support, which is on the roadmap

// clang-format off
struct vm_zone
{
  struct vm_zone *next;              // VM zones are stored as a singly-linked list
  lock_t lck;                        // Spinlock for protecting bitmap

  uintptr_t base, limit, bitmap_len; // Length of bitmap, along with position of the zone in memory
  uintptr_t last_index;              // Last allocated index
  uint8_t* bitmap;                   // Pointer to bitmap (for keeping track of free/used pages)
  int domain;                        // NUMA domain (zero for now, since NUMA is still not implemented)
};

// Zone creation/mangement functions
bool
vm_zone_possible(uintptr_t base, uint64_t len); // Determines whether a range of memory can be turned into a zone
void
vm_create_zone(uintptr_t base, uint64_t len);
void*
vm_zone_alloc(struct vm_zone* zn, size_t pages, size_t align);
void
vm_zone_free(struct vm_zone* zn, void* ptr, size_t pages);
// clang-format on

// Kernel's list of zones
extern struct vm_zone *head_zone, *tail_zone;

// Physical allocation flags
#define VM_ALLOC_ZERO (1 << 10)
#define VM_ALLOC_HUGE (1 << 11)

// The actual functions
void *vm_phys_alloc(uint64_t pages, int flags);
void vm_phys_free(void *start, uint64_t pages);

#endif  // VM_PHYS_H
