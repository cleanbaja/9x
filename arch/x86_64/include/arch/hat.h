#ifndef ARCH_HAT_H
#define ARCH_HAT_H

#include <ninex/init.h>
#include <vm/virt.h>
#include <vm/vm.h>

// The HAT (Hardware Address Translation) layer provides a abstraction for accessing raw PTEs
// in the kernel. Its responsible for the lower-level paging and asid code, and should ONLY
// be called by the VM...

uint64_t hat_create_pte(vm_flags_t flags, uintptr_t phys, bool is_block);
void hat_scrub_pde(uintptr_t root, int level);

#define INVL_SINGLE_ADDR 0x10
#define INVL_SINGLE_ASID 0x11
#define INVL_ALL_ASIDS   0x13
#define INVL_ENTIRE_TLB  0x12
void hat_invl(uintptr_t root, uintptr_t virt, uint32_t asid, int mode);

#define TRANSLATE_DEPTH_NORM 0xE1
#define TRANSLATE_DEPTH_HUGE 0xE2
uint64_t* hat_translate_addr(uintptr_t root,
                             uintptr_t virt,
                             bool create,
                             int depth);

// Sets up handlers, and does some information gathering...
EXPORT_STAGE(hat_init_stage);

#endif  // ARCH_HAT_H
