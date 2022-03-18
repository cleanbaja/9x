#ifndef VM_H
#define VM_H

#include <internal/stivale2.h>
#include <stddef.h>

// Hard-coded kernel virtual/physical memory constants
#define VM_MEM_OFFSET 0xffff800000000000
#define VM_KERN_OFFSET 0xffffffff80000000
#define VM_PAGE_SIZE 0x1000

// Bootstraps the entire VM, including allocators and Virtual Memory
void
vm_init(struct stivale2_struct_tag_memmap* mm_tag);

// Liballoc (aka kmalloc) defs
#define PREFIX(func) k##func
void* PREFIX(malloc)(size_t);
void* PREFIX(realloc)(void*, size_t);
void PREFIX(free)(void*);

#endif // VM_H
