#include <arch/asm.h>
#include <arch/hat.h>
#include <vm/virt.h>
#include <vm/phys.h>
#include <vm/vm.h>
#include <lib/log.h>

// Set defaults to match 4LV paging
enum { VM_5LV_PAGING, VM_4LV_PAGING } paging_mode = VM_4LV_PAGING;
uintptr_t kernel_vma = 0xFFFF800000000000;
static CREATE_SPINLOCK(hat_lock);

static uint64_t
create_pte(uintptr_t phys, int flags, bool huge_page)
{
  uint64_t raw_page = 0;

  // Encode permissions
  if (!(flags & VM_PERM_READ)) {
    log("vm/virt: (WARN) Non-Readable mappings not supported on x86 (flags: 0x%x)",
        flags);
  }
  if (flags & VM_PERM_WRITE) {
    raw_page |= (1 << 1);
  }
  if (!(flags & VM_PERM_EXEC)) {
    raw_page |= (1ull << 63);
  }
  if (flags & VM_PERM_USER) {
    raw_page |= (1 << 2);
  }

  // Encode page attributes
  if (flags & VM_PAGE_GLOBAL)
    raw_page |= (1 << 8);
  
  if (huge_page)
    raw_page |= (1 << 7);

  // Check for cache type
  switch (flags & VM_CACHE_MASK) {
    case VM_CACHE_UNCACHED:
      raw_page |= ~(1 << huge_page ? 12 : 7) | (1 << 4) | (1 << 3);
      break;
    case VM_CACHE_WRITE_COMBINING:
      raw_page |= (1 << huge_page ? 12 : 7) | (1 << 4) | (1 << 3);
      break;
    case VM_CACHE_WRITE_PROTECT:
      raw_page |= (1 << huge_page ? 12 : 7) | (1 << 4) | ~(1 << 3);
      break;

    // TODO: Write Through
    default:
      break; // Use the default memory type (Write Back), which is already
             // optimized
  }

  return raw_page | phys | (1 << 0); // Present plus address
}

static uint64_t*
next_level(uint64_t* prev_level, uint64_t index, bool create)
{
  if (!(prev_level[index] & 1)) {
    if (!create)
      return NULL;

    prev_level[index] = (uint64_t)vm_phys_alloc(1, VM_ALLOC_ZERO);
    prev_level[index] |= 0b111;
  }

  return (uint64_t*)((prev_level[index] & ~(0x1ff)) + VM_MEM_OFFSET);
}

void hat_map_page(uintptr_t root, uintptr_t phys, uintptr_t virt, int flags) {
  if (!(phys % 0x1000 == 0))
    log("hat: unaligned 4KB mapping! (phys, virt: 0x%lx, 0x%lx)", phys, virt);

  // Index the virtual address
  uint64_t pml5_index = (virt & ((uint64_t)0x1ff << 48)) >> 48;
  uint64_t pml4_index = (virt & ((uint64_t)0x1ff << 39)) >> 39;
  uint64_t pml3_index = (virt & ((uint64_t)0x1ff << 30)) >> 30;
  uint64_t pml2_index = (virt & ((uint64_t)0x1ff << 21)) >> 21;
  uint64_t pml1_index = (virt & ((uint64_t)0x1ff << 12)) >> 12;

  uint64_t *level4, *level3, *level2, *level1;
  if (paging_mode == VM_4LV_PAGING) {
    level4 = (uint64_t*)(root + VM_MEM_OFFSET);
  } else {
    uint64_t *level5 = (uint64_t*)(root + VM_MEM_OFFSET);
    level4 = next_level(level5, pml5_index, true);
  }

  // Do a page walk until we reach our target
  level3 = next_level(level4, pml4_index, true);
  level2 = next_level(level3, pml3_index, true);
  level1 = next_level(level2, pml2_index, true);

  // Map in the page, and detect if it was previously active
  bool page_was_active = true;
  page_was_active = (level1[pml1_index] & (1 << 5));
  level1[pml1_index] = create_pte(phys, flags, false);

  // TODO: Invalidate address
}

void hat_map_huge_page(uintptr_t root, uintptr_t phys, uintptr_t virt, int flags) {
  if (!(phys % 0x200000 == 0))
    log("hat: unaligned 2MB mapping! (phys, virt: 0x%lx, 0x%lx)", phys, virt);

  // Index the virtual address
  uint64_t pml5_index = (virt & ((uint64_t)0x1ff << 48)) >> 48;
  uint64_t pml4_index = (virt & ((uint64_t)0x1ff << 39)) >> 39;
  uint64_t pml3_index = (virt & ((uint64_t)0x1ff << 30)) >> 30;
  uint64_t pml2_index = (virt & ((uint64_t)0x1ff << 21)) >> 21;
  uint64_t pml1_index = (virt & ((uint64_t)0x1ff << 12)) >> 12;

  uint64_t *level4, *level3, *level2, *level1;
  if (paging_mode == VM_4LV_PAGING) {
    level4 = (uint64_t*)(root + VM_MEM_OFFSET);
  } else {
    uint64_t *level5 = (uint64_t*)(root + VM_MEM_OFFSET);
    level4 = next_level(level5, pml5_index, true);
  }

  // Do a page walk until we reach our target
  level3 = next_level(level4, pml4_index, true);
  level2 = next_level(level3, pml3_index, true);
  
  // Map in the page, and detect if it was previously active
  bool page_was_active = true;
  page_was_active = (level2[pml2_index] & (1 << 5));
  level2[pml2_index] = create_pte(phys, flags, true);

  // TODO: Invalidate the address
}

uint64_t* hat_resolve_addr(uintptr_t root, uintptr_t virt) {
  uint64_t level5_index = (virt & ((uint64_t)0x1ff << 48)) >> 48;
  uint64_t level4_index = (virt & ((uint64_t)0x1ff << 39)) >> 39;
  uint64_t level3_index = (virt & ((uint64_t)0x1ff << 30)) >> 30;
  uint64_t level2_index = (virt & ((uint64_t)0x1ff << 21)) >> 21;
  uint64_t level1_index = (virt & ((uint64_t)0x1ff << 12)) >> 12;
  uint64_t *level4, *level3, *level2, *level1;

  if (paging_mode == VM_4LV_PAGING) {
    level4 = (uint64_t*)(root + VM_MEM_OFFSET);
  } else {
    uint64_t *level5 = (uint64_t*)(root + VM_MEM_OFFSET);
    level4 = next_level(level5, level5_index, false);
    if (level4 == NULL)
      return NULL;
  }

  level3 = next_level(level4, level4_index, false);
  if (level3 == NULL)
    return NULL;

  level2 = next_level(level3, level3_index, false);
  if (level2 == NULL)
    return NULL;
  else if (level2[level2_index] & (1 << 7))
    return &level2[level2_index];

  level1 = next_level(level2, level2_index, false);
  if (level1 == NULL)
    return NULL;

  return &level1[level1_index];
}

void hat_unmap_page(uintptr_t root, uintptr_t virt) {
  uint64_t* pte = hat_resolve_addr(root, virt);
  if (pte != NULL)
    *pte = 0;
}

void hat_invl(uintptr_t root, uintptr_t virt, uint32_t asid, int mode) {
  spinlock_acquire(&hat_lock);

  if (CPU_CHECK(CPU_FEAT_PCID)) {
    // Use the INVPCID instruction, if supported!
    if (CPU_CHECK(CPU_FEAT_INVPCID)) {
      struct invpcid_descriptor {
        uint64_t pcid;
	uint64_t addr;
      } desc = { .pcid = asid, .addr = virt };

      __asm__ volatile("invpcid %1, %0" : : "r"((uint64_t)(mode - 0x10)), "m"(desc) : "memory");
    } else {
      uint64_t old_cr3, new_cr3;
      switch (mode) {
      case INVL_SINGLE_ADDR:
        asm_invlpg(virt);
        break;

      case INVL_SINGLE_ASID:
	old_cr3 = asm_read_cr3();
	new_cr3 = (uint16_t)asid | kernel_space.root;
	new_cr3 &= ~(1ull << 63);                              // Clear bit 63, to invalidate the PCID
        
	asm_write_cr3(new_cr3);
	asm_write_cr3(old_cr3);
	break;

      case INVL_ALL_ASIDS:  // TODO: Find a way to invalidate all ASIDs, without invalidating global pages
      case INVL_ENTIRE_TLB:
        asm_write_cr4(asm_read_cr4() & ~(1 << 7));
        asm_write_cr4(asm_read_cr4() | (1 << 7));
        __asm__ volatile ("wbinvd" ::: "memory");
        break;

      default:
        log("hat: Invalidation mode %d is not supported!", mode); 
      }
    }
  } else {
    // ASIDs (aka PCIDs) are not supported, just run invlpg, or clear the entire TLB
    switch (mode) {
    case INVL_SINGLE_ADDR:
      asm_invlpg(virt);
      break;

    case INVL_ENTIRE_TLB:
      asm_write_cr4(asm_read_cr4() & ~(1 << 7));
      asm_write_cr4(asm_read_cr4() | (1 << 7));
      __asm__ volatile ("wbinvd" ::: "memory");
      break;

    default:
      log("hat: Invalidation mode %d is not supported!", mode); 
    }
  }

  spinlock_release(&hat_lock);
}


