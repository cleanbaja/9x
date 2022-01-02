#ifndef VM_H
#define VM_H

#include <internal/stivale2.h>

#define VM_MEM_OFFSET   0xffff800000000000
#define VM_KERN_OFFSET  0xffffffff80000000

void vm_init(struct stivale2_struct_tag_memmap* mm_tag);

// Physical memory defs ==============================
void vm_init_phys(struct stivale2_struct_tag_memmap* mmap);
void* vm_phys_alloc(uint64_t pages);
void vm_phys_free(void* start, uint64_t pages);

// Virtual memory defs ===============================

#define MM_FEAT_NX    0x0
#define MM_FEAT_GLOBL 0x1
#define MM_FEAT_SMEP  0x2
#define MM_FEAT_SMAP  0x3
#define MM_FEAT_PCID  0x4
#define MM_FEAT_1GB   0x5
#define MMU_CHECK(k) (mmu_features & (1 << k))

extern uint64_t mmu_features;

typedef struct {
  union {
    struct {
      uint8_t present   : 1;
      uint8_t writeable : 1;
      uint8_t userpage  : 1;
      uint8_t pat_lo    : 2;
      uint8_t accessed  : 1;
      uint8_t dirty     : 1;
      uint8_t pat_hi    : 1;
      uint8_t global    : 1; // Ignored if MM_FEAT_GLOBL is not active or supported
      uint64_t frame    : 50;
      uint8_t pkey      : 4;
      uint8_t no_exec   : 1;
    };
    uint64_t raw;
  };
} pte_t;

typedef struct {
  union {
    struct {
      uint8_t present   : 1;
      uint8_t writeable : 1;
      uint8_t userpage  : 1;
      uint8_t pat_lo    : 2;
      uint8_t accessed  : 1;
      uint8_t dirty     : 1;
      uint8_t page_size : 1; // 1GB huge pages require MM_FEAT_1GB
      uint8_t global    : 1; // Ignored if MM_FEAT_GLOBL is not active or supported
      uint8_t ignored   : 3;
      uint8_t pat_hi    : 1;
      uint64_t frame    : 46;
      uint8_t pkey      : 4;
      uint8_t no_exec   : 1;
    };
    uint64_t raw;
  };
} pte_huge_t;

typedef enum {
  VM_PERM_READ   = (1 << 3),
  VM_PERM_WRITE  = (1 << 4),
  VM_PERM_EXEC   = (1 << 5),
  VM_PERM_USER   = (1 << 6),
  VM_PAGE_GLOBAL = (1 << 7),
  VM_PAGE_HUGE   = (1 << 8)
} vm_flags_t;

typedef struct {
  uint64_t pml4;
  uint16_t pcid;
} vm_space_t;

void vm_init_virt();

void vm_virt_map(vm_space_t* spc, uintptr_t phys, uintptr_t virt, int flags);
void vm_virt_unmap(vm_space_t* spc, uintptr_t virt);

#endif // VM_H

