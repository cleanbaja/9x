#ifndef VM_VIRT_H
#define VM_VIRT_H

#include <lib/stivale2.h>
#include <stdbool.h>

// Flags passed to vm_virt_map()...
typedef enum
{
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
typedef struct
{
  uint64_t root;
  uint16_t asid;
  bool active;
} vm_space_t;

// Functions for messing with the lower level PTE structures...
void
vm_virt_map(vm_space_t* spc, uintptr_t phys, uintptr_t virt, int flags);
void
vm_virt_unmap(vm_space_t* spc, uintptr_t virt);
void
vm_virt_fragment(vm_space_t* spc, uintptr_t virt, int flags);
uint64_t*
virt2pte(vm_space_t* spc, uintptr_t virt);

// Misc virt functions...
void
vm_init_virt(struct stivale2_struct_tag_memmap* mmap);
void
vm_invl(vm_space_t* spc, uintptr_t addr);
void
vm_load_space(vm_space_t* spc);
vm_space_t*
vm_create_space();
void
percpu_init_vm();

// The kernel's space, which all others inherit from
extern vm_space_t kernel_space;

#endif // VM_VIRT_H
