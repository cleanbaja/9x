#ifndef SYS_HAT_H
#define SYS_HAT_H

#include <stdint.h>

// The HAT (Hardware Address Translation) layer provides a abstraction for accessing raw PTEs
// in the kernel. Its responsible for the lower-level paging and asid code, and should ONLY
// be called by the VM...


void hat_map_page(uintptr_t root, uintptr_t phys, uintptr_t virt, int flags);
void hat_map_huge_page(uintptr_t root, uintptr_t phys, uintptr_t virt, int flags); // Map 2MB page, instead of the usual 4KB
void hat_unmap_page(uintptr_t root, uintptr_t virt);
uint64_t* hat_resolve_addr(uintptr_t root, uintptr_t virt);

#define INVL_SINGLE_ADDR 0x10
#define INVL_SINGLE_ASID 0x11
#define INVL_ALL_ASIDS   0x13
#define INVL_ENTIRE_TLB  0x12
void hat_invl(uintptr_t root, uintptr_t virt, uint32_t asid, int mode);

#endif // SYS_HAT_H

