#ifndef VM_SEG_H
#define VM_SEG_H

#include <lib/vec.h>
#include <stdbool.h>
#include <stddef.h>

#define PROT_NONE 0x00
#define PROT_READ 0x01
#define PROT_WRITE 0x02
#define PROT_EXEC 0x04

#define MAP_FILE 0x00
#define MAP_PRIVATE 0x01
#define MAP_SHARED 0x02
#define MAP_FIXED 0x04
#define MAP_ANON 0x08
#define MAP_ANONYMOUS 0x08
#define MAP_NODEMAND 0x10

enum vm_fault {
  VM_FAULT_NONE = 0,
  VM_FAULT_WRITE = (1 << 2),
  VM_FAULT_EXEC = (1 << 3),
  VM_FAULT_PROTECTION = (1 << 4)
};

struct tracked_page {
  uintptr_t virt, phys;
  int refcount;

  enum { TRACKED_PAGE_NONE, TRACKED_PAGE_PRESENT, TRACKED_PAGE_UNMAPPED } state;
};

struct vm_seg {
  uintptr_t base;
  int prot, mode;
  bool shared;
  size_t len;

  bool (*deal_with_pf)(struct vm_seg*, size_t, enum vm_fault);
  vec_t(struct tracked_page*) pagelist;
};

/* This function has a diffrent amount of parameters for the segment type
 *   - For anonymous segments, an example call would look like this...
 *     vm_create_seg(mode <must be MAP_ANON>, prot, len, <optional> hint)
 *   - For file mappings, the call would look like this...
 *     vm_create_seg(mode <must be MAP_FILE>, prot, len, resource, <optional>
 * hint)
 */
struct vm_seg* vm_create_seg(int mode, ...);

struct vm_seg* vm_find_seg(uintptr_t addr, size_t* offset);
void vm_unmap_seg(struct vm_seg* seg, uintptr_t base, size_t len);
void vm_destroy_seg(struct vm_seg* seg);

#endif  // VM_SEG_H
