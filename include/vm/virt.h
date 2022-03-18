#ifndef VM_VIRT_H
#define VM_VIRT_H

#include <stdbool.h>
#include <stdint.h>

// CPU features that 9x is aware of, and utilizes (except for 1GB pages)
#define MM_FEAT_NX 0x0
#define MM_FEAT_GLOBL 0x1
#define MM_FEAT_SMEP 0x2
#define MM_FEAT_SMAP 0x3
#define MM_FEAT_PCID 0x4
#define MM_FEAT_1GB 0x5
#define MMU_CHECK(k) (mmu_features & (1 << k))

extern uint64_t mmu_features;

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
  VM_CACHE_FLAG_UNCACHED = (1 << 15),
  VM_CACHE_FLAG_WRITE_COMBINING = (2 << 15),
  VM_CACHE_FLAG_WRITE_PROTECT = (3 << 15),
} vm_flags_t;

// Repersents a virtual memory space, in which pages and objects are mapped
typedef struct
{
  uint64_t pml4;
  uint16_t pcid;
  bool active;
} vm_space_t;

// Functions for manipulating a address space
void
vm_virt_map(vm_space_t* spc, uintptr_t phys, uintptr_t virt, int flags);
void
vm_virt_unmap(vm_space_t* spc, uintptr_t virt);
uint64_t*
virt2pte(vm_space_t* spc, uintptr_t virt);

// Misc virt functions...
void
vm_init_virt();
void
vm_invl(vm_space_t* spc, uintptr_t addr);
void
vm_load_space(vm_space_t* spc);
void
percpu_init_vm();

// The kernel's space, which all others inherit from
extern vm_space_t kernel_space;

#endif // VM_VIRT_H
