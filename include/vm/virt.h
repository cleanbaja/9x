#ifndef VM_VIRT_H
#define VM_VIRT_H

#include <lib/stivale2.h>
#include <lib/vec.h>
#include <vm/seg.h>

// Flags passed to vm_virt_map()...
typedef enum {
  // Permission flags
  VM_PERM_READ = (1 << 3),
  VM_PERM_WRITE = (1 << 4),
  VM_PERM_EXEC = (1 << 5),

  // Page attribute flags
  VM_PERM_USER = (1 << 6),
  VM_PAGE_GLOBAL = (1 << 7),
  VM_PAGE_HUGE = (1 << 8),

  // Cache flags
  VM_CACHE_MASK = (4 << 15),
  VM_CACHE_UNCACHED = (1 << 15),
  VM_CACHE_WRITE_COMBINING = (2 << 15),
  VM_CACHE_WRITE_PROTECT = (3 << 15),
} vm_flags_t;

// Repersents a virtual memory space, in which pages and objects are mapped
typedef struct {
  uint64_t root;
  uint32_t asid;
  bool active;

  vec_t(struct vm_seg *) mappings;
  uintptr_t mmap_base;
} vm_space_t;

// Functions for manipulating the virtual address range...
void vm_map_range(vm_space_t *space,
                  uintptr_t phys,
                  uintptr_t virt,
                  size_t len,
                  int flags);
void vm_unmap_range(vm_space_t *space, uintptr_t virt, size_t len);

// Functions related to the VM address space
void vm_space_load(vm_space_t *space);
void vm_space_destroy(vm_space_t *space);
void vm_space_fork(vm_space_t *old, vm_space_t *cur);
vm_space_t *vm_space_create();

// Misc virt functions...
void vm_invl(vm_space_t *spc, uintptr_t addr, size_t len);
bool vm_fault(uintptr_t location, enum vm_fault flags);
void vm_virt_init();

// The kernel's space, which all others inherit from
extern vm_space_t kernel_space;

#endif  // VM_VIRT_H
