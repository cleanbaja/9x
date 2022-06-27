#ifndef VM_H
#define VM_H

#include <lib/stivale2.h>
#include <stddef.h>

// Repersents all tuneable, arch-specific constants
struct vm_config {
  uintptr_t higher_half_window;
  uint8_t levels;
  uint32_t asid_max, page_size, huge_page_size;
};
extern struct vm_config* cur_config;

// Hard-coded kernel virtual/physical memory constants for x86_64
#define VM_KERN_OFFSET 0xffffffff80000000
#define VM_ASID_MAX    4096
#define VM_PAGE_SIZE   0x1000
#define VM_MEM_OFFSET kernel_vma
extern uintptr_t kernel_vma;

// Bootstraps the entire VM
void vm_setup();

// liballoc (aka kmalloc) defs
#define PREFIX(func) k##func
void* PREFIX(malloc)(size_t);
void* PREFIX(realloc)(void*, size_t);
void PREFIX(free)(void*);

#endif // VM_H
