#ifndef VM_H
#define VM_H

#include <lib/stivale2.h>
#include <stddef.h>

// Hard-coded kernel virtual/physical memory constants for x86_64
#define VM_KERN_OFFSET 0xffffffff80000000
#define VM_ASID_MAX    4096
#define VM_PAGE_SIZE   0x1000
extern uintptr_t kernel_vma;
#define VM_MEM_OFFSET kernel_vma

// Bootstraps the entire VM, including allocators and Virtual Memory
void
vm_init(struct stivale2_struct_tag_memmap* mm_tag);

// Liballoc (aka kmalloc) defs
#define PREFIX(func) k##func
void* PREFIX(malloc)(size_t);
void* PREFIX(realloc)(void*, size_t);
void PREFIX(free)(void*);

#endif // VM_H
