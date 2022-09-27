#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <misc/queue.h>

enum lvm_page_type {
  /*
   * Represents free and zero memory, unused by the system.
   */
  LVM_PAGE_ZERO = 1,

  /*
   * Represents free, unused memory, that isn't yet zeroed.
   * All usable system memory at boot is of this type...
   */
  LVM_PAGE_UNUSED,

  /*
   * Page is used to back VMOs (Virtual Memory Objects)
   */
  LVM_PAGE_OBJ,

  /*
   * Page is used to back kernel memory allocations (kmalloc)
   */
  LVM_PAGE_ALLOC,

  /*
   * Represents pages used in misc allocations, like 
   * page table backings and kernel stacks
   */
  LVM_PAGE_SYSTEM = 5
};

struct lvm_page {
  union {
    struct {
      /*
       * See 'enum lvm_page_type' for type values
       */
      uint64_t type : 8;

      /*
       * Repersents the 4KB aligned physical address, with the bottom
       * 12 bits masked off. To expand, one would do something like this...
       * 
       * real_address = page_frame << 12;
       */
      uint64_t page_frame : 32;

      /*
       * Used for making page accesses atomic, therefore code should not
       * modify this structure, unless lock_bit is 0
       */
      uint64_t lock_bit : 1;

      /*
       * TODO: mark these bits as reserved, and create seperate 
       * variable for keeping track of refcount.
       */
      uint64_t refcount : 16;
    };

    uint64_t raw;
  };

  /*
   * We use a slist since its more compact than other types,
   * therefore saving more memory.
   */
  STAILQ_ENTRY(lvm_page) link;
};

#define LVM_ALLOC_PAGE(zero_mem, mem_type) ({     \
  struct lvm_page* result = lvm_palloc(zero_mem); \
  result->type = mem_type;                        \
  (uintptr_t)(result->page_frame << 12);          \
})
#define LVM_FREE_PAGE(page_addr) lvm_pfree(lvm_find_page(page_addr));

STAILQ_HEAD(pagelist, lvm_page);

struct lvm_page* lvm_palloc(bool zero_mem);
struct lvm_page* lvm_find_page(uintptr_t addr);
void lvm_pfree(struct lvm_page* pg);
   
extern struct lvm_page *lvm_pfndb;
extern struct pagelist modified_list;
extern struct pagelist zero_list;
extern uint64_t lvm_pagecount;
