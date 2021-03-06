#include <arch/asm.h>
#include <arch/cpu.h>
#include <arch/cpuid.h>
#include <arch/hat.h>
#include <arch/irqchip.h>
#include <lib/cmdline.h>
#include <lib/kcon.h>
#include <vm/phys.h>
#include <vm/virt.h>
#include <vm/vm.h>

struct vm_config possible_x86_modes[] = {
    /* There are two possible modes for x86, that are the
     * same, except for 4/5 level translation */
    /* Higher-Half Base - Levels - ASID count - Page size - Huge Page size */
    {0xFFFF800000000000, 4, 4096, 4096, 4096 * 512},
    {0xFF00000000000000, 5, 4096, 4096, 4096 * 512}};
struct vm_config* cur_config = NULL;
static bool log_pagefault = false;
static lock_t hat_lock;

// Set defaults to match 4LV paging
uintptr_t kernel_vma = 0xFFFF800000000000;

static uint64_t* next_level(uint64_t* prev_level, uint64_t index, bool create) {
  if (!(prev_level[index] & 1)) {
    if (!create)
      return NULL;

    prev_level[index] = (uint64_t)vm_phys_alloc(1, VM_ALLOC_ZERO);
    prev_level[index] |= 0b111;
  }

  return (uint64_t*)((prev_level[index] & ~(0x1ff)) + VM_MEM_OFFSET);
}

uintptr_t hat_get_base(enum base_type bt) {
  if (cur_config->levels == 5) {
    switch (bt) {
    case HAT_BASE_KEXT:
      return 0xFFD4000000000000;
    case HAT_BASE_USEG:
      // 1GB below the end of the usermode address space
      return 0x00fff00000000000 - (0x1000 * 512 * 512);
    }
  } else {
    switch (bt) {
    case HAT_BASE_KEXT:
      return 0xFFFFEA0000000000;
    case HAT_BASE_USEG:
      // 1GB below the end of the usermode address space
      return 0x0000700000000000 - (0x1000 * 512 * 512);
    }
  }

  return 0x0;
}

uint64_t* hat_translate_addr(uintptr_t root,
                             uintptr_t virt,
                             bool create,
                             int depth) {
  uint64_t* cur = (uint64_t*)(root + VM_MEM_OFFSET);
  uint64_t idx_map[] = {
#define INDEX(shift) ((virt & ((uint64_t)0x1ff << shift)) >> shift)
      INDEX(48), INDEX(39), INDEX(30), INDEX(21), INDEX(12)
#undef INDEX
  };

  if (depth == 0)
    depth = TRANSLATE_DEPTH_NORM;

  // Perform the 5th layer translation, if needed...
  bool doing_5lv = (cur_config->levels == 5);
  if (doing_5lv) {
    cur = next_level(cur, idx_map[0], create);
    if (cur == NULL)
      return NULL;
  }

#define CHECK_PTE(sl, idx, final)        \
  if (sl == NULL)                        \
    return NULL; /* Non-Existent Page */ \
  else if (sl[idx] & (1 << 7))           \
    return &sl[idx]; /* Huge page */     \
  else if (final)                        \
    return &sl[idx]; /* Bottom level */

  cur = next_level(cur, idx_map[1], create);
  if (cur == NULL)
    return NULL;

  cur = next_level(cur, idx_map[2], create);
  CHECK_PTE(cur, idx_map[3], (depth == TRANSLATE_DEPTH_HUGE))

  cur = next_level(cur, idx_map[3], create);
  CHECK_PTE(cur, idx_map[4], true)
#undef CHECK_PTE
}

uint64_t hat_create_pte(vm_flags_t flags, uintptr_t phys, bool is_block) {
  uint64_t pte_raw = 1;  // PTE must always be present

  if (flags & VM_PERM_WRITE)
    pte_raw |= (1 << 1);

  if (!(flags & VM_PERM_EXEC))
    pte_raw |= (1ull << 63);

  if (flags & VM_PERM_USER)
    pte_raw |= (1 << 2);

  if (flags & VM_PAGE_GLOBAL)
    pte_raw |= (1 << 8);

  if (is_block)
    pte_raw |= (1 << 7);

  if (!(flags & VM_PERM_READ))
    klog("hat: (WARN) x86_64 does not support non-readable mappings! (0x%x)",
         flags);

  // Set proper cache type
  switch (flags & VM_CACHE_MASK) {
    case VM_CACHE_UNCACHED:
      pte_raw |= ~(1 << (is_block ? 12 : 7)) | (1 << 4) | (1 << 3);
      break;
    case VM_CACHE_WRITE_COMBINING:
      pte_raw |= (1 << (is_block ? 12 : 7)) | (1 << 4) | (1 << 3);
      break;
    case VM_CACHE_WRITE_PROTECT:
      pte_raw |= (1 << (is_block ? 12 : 7)) | (1 << 4) | ~(1 << 3);
      break;
    default:
      break;  // Use the default memory type (Write Back),
              // which is already optimized for standard accesses
  }

  // Encode physical address, and return the completed PTE
  pte_raw |= phys;
  return pte_raw;
}

void hat_invl(uintptr_t root, uintptr_t virt, uint32_t asid, int mode) {
  spinlock(&hat_lock);

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
        new_cr3 &= ~(1ull << 63);  // Clear bit 63, to invalidate the PCID

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
        klog("hat: Invalidation mode %d is not supported!", mode); 
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
      klog("hat: Invalidation mode %d is not supported!", mode);
    }
  }

  spinrelease(&hat_lock);
}

void hat_scrub_pde(uintptr_t root, int level) {
  uintptr_t virt = root + VM_MEM_OFFSET;
  uint64_t* pde = (uint64_t*)virt;

  if (level >= 3) {
    for (size_t i = 0; i < 256; i++)
      if (pde[i] & 1)
        hat_scrub_pde((pde[i] & ~(0x1ff)), level - 1);
      else if (pde[i] & 7)
        vm_phys_free((void*)(pde[i] & ~(0x1ff)), 1);
  } else {
    for (size_t i = 0; i < 256; i++)
      if (pde[i] & 1)
        vm_phys_free((void*)(pde[i] & ~(0x1ff)), 1);
  }

  vm_phys_free((void*)root, 1);
}

// The following VM function is placed here because
// it does arch-specifc things, and I don't feel like
// doing a '#ifdef __x86_64__'...
void vm_space_load(vm_space_t* s) {
  uint64_t cr3 = s->root;
  if (CPU_CHECK(CPU_FEAT_PCID)) {
    cr3 |= (uint16_t)s->asid | (1ull << 63);
  }

  asm_write_cr3(cr3);
  s->active = true;
}

void handle_pf(cpu_ctx_t* context) {
  uint32_t ec = context->ec;
  uintptr_t address = asm_read_cr2();
  bool do_panic = false;

  // Fill in the vm_fault flags...
  enum vm_fault vf = VM_FAULT_NONE;
  if (ec & (1 << 0))
    vf |= VM_FAULT_PROTECTION;
  if (ec & (1 << 4))
    vf |= VM_FAULT_EXEC;
  if (ec & (1 << 1))
    vf |= VM_FAULT_WRITE;

  // Switch GS, so we can access kernel structures
  if (context->cs & 3)
    asm_swapgs();

  // Call the kernel page fault handler, and
  // make sure the page fault was fixed
  if (!vm_fault(address, vf))
    do_panic = true; // PANIC for now, since we can't send signals/kill threads

  // For more information on the bits of the error code,
  // see Intel x86_64 SDM Volume 3a Chapter 4.7
  if (log_pagefault || do_panic) {
    klog("hat: Page Fault at 0x%lx! (%s) (%s) %s", address,
         (ec & (1 << 1)) ? "write" : "read", (ec & (1 << 2)) ? "user" : "kmode",
         (ec & (1 << 4)) ? "(ifetch)" : "");

    if (ec & (1 << 0))
      klog("  -> Cause: Page Protection Violation!");
    else if (ec & (1 << 3))
      klog("  -> Cause: Reserved Bit set!");
    else if (ec & (1 << 5))
      klog("  -> Cause: Protection Key Violation");
    else if (ec & (1 << 15))
      klog("  -> Cause: Intel(r) SGX Violation!");
    else
      klog("  -> Cause: Page Not Present!");

    if (do_panic)
      PANIC(context, NULL);
  }

  // Now that the page fault is successfully handled, switch GS back
  if (context->cs & 3)
    asm_swapgs();
}

// Bootstraps the HAT...
void hat_init() {
  uint32_t a, b, c, d;
  cpuid_subleaf(0x7, 0x0, &a, &b, &c, &d);
  if (cur_config != NULL)
    goto smp_entry;

  // Use 5lv paging, unless its unsupported
  if (c & (1 << 16)) {
    cur_config = &possible_x86_modes[1];
    kernel_vma = cur_config->higher_half_window;
    klog("hat: la57 detected!");
  } else {
    cur_config = &possible_x86_modes[0];
  }

smp_entry:
  // Load the PAT with our custom value, which changes 2 registers.
  //   PA6 => Formerly UC-, now Write Protect
  //   PA7 => Formerly UC, now Write Combining
  //
  // NOTE: The rest remain at the default, see
  // AMD Programmer's Manual Volume 2, Section 7.8.2
  asm_wrmsr(0x277, 0x105040600070406);
}
