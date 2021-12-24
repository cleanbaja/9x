#ifndef VM_H
#define VM_H

#include <internal/stivale2.h>

#define VM_MEM_OFFSET 0xFFFF800000000000

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

void vm_init_virt();


#endif // VM_H

