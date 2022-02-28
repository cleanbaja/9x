#ifndef VM_H
#define VM_H

#include <internal/stivale2.h>
#include <stdbool.h>
#include <stddef.h>

#define VM_MEM_OFFSET  0xffff800000000000
#define VM_KERN_OFFSET 0xffffffff80000000

void
vm_init(struct stivale2_struct_tag_memmap* mm_tag);

// ========================
//   Physical memory defs 
// =======================

// Helpfult Macros/Constants
#define DIV_ROUNDUP(A, B)                                                      \
  ({                                                                           \
    typeof(A) _a_ = A;                                                         \
    typeof(B) _b_ = B;                                                         \
    (_a_ + (_b_ - 1)) / _b_;                                                   \
  })
#define VM_PAGE_SIZE 4096

// The actual functions
void
vm_init_phys(struct stivale2_struct_tag_memmap* mmap);
void*
vm_phys_alloc(uint64_t pages);
void
vm_phys_free(void* start, uint64_t pages);

// =======================
//   Virtual memory defs 
// =======================

#define MM_FEAT_NX 0x0
#define MM_FEAT_GLOBL 0x1
#define MM_FEAT_SMEP 0x2
#define MM_FEAT_SMAP 0x3
#define MM_FEAT_PCID 0x4
#define MM_FEAT_1GB 0x5
#define MMU_CHECK(k) (mmu_features & (1 << k))

extern uint64_t mmu_features;

typedef enum
{
  // Permission flags
  VM_PERM_READ   = (1 << 3),
  VM_PERM_WRITE  = (1 << 4),
  VM_PERM_EXEC   = (1 << 5),

  // Page attribute flags
  VM_PERM_USER   = (1 << 6),
  VM_PAGE_GLOBAL = (1 << 7),
  VM_PAGE_HUGE   = (1 << 8),

  // Cache flags
  VM_CACHE_MASK                  = (4 << 15),
  VM_CACHE_FLAG_UNCACHED         = (1 << 15),
  VM_CACHE_FLAG_WRITE_COMBINING  = (2 << 15),
  VM_CACHE_FLAG_WRITE_PROTECT    = (3 << 15),
} vm_flags_t;

typedef struct
{
  uint64_t pml4;
  uint16_t pcid;
  bool active;
} vm_space_t;

void
vm_virt_map(vm_space_t* spc, uintptr_t phys, uintptr_t virt, int flags);
void
vm_virt_unmap(vm_space_t* spc, uintptr_t virt);
uint64_t*
virt2pte(vm_space_t* spc, uintptr_t virt);

void
vm_init_virt();
void
vm_invl(vm_space_t* spc, uintptr_t addr);
void
vm_load_space(vm_space_t* spc);
void
percpu_init_vm();

extern vm_space_t kernel_space;

// =========================
//   Memory allocater defs
// =========================

#define PREFIX(func)		k ## func

void* PREFIX(malloc)(size_t);	
void* PREFIX(realloc)(void *, size_t);	
void  PREFIX(free)(void *);

#endif // VM_H
