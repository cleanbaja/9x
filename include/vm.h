#ifndef VM_H
#define VM_H

#include <internal/stivale2.h>

#define VM_MEM_OFFSET 0xFFFF800000000000

void vm_init(struct stivale2_struct_tag_memmap* mm_tag);

// Physical memory defs ==============================
void vm_init_phys(struct stivale2_struct_tag_memmap* mmap);
void* vm_phys_alloc(uint64_t pages);
void vm_phys_free(void* start);

#endif // VM_H

