#include <arch/asm.h>
#include <arch/hat.h>
#include <lib/builtin.h>
#include <lib/kcon.h>
#include <lib/stivale2.h>

#include <vm/phys.h>
#include <vm/virt.h>
#include <vm/vm.h>

vm_space_t kernel_space;

//////////////////////////
//   ASID Management
//////////////////////////
uint32_t *asid_bitmap = NULL;

static uint32_t alloc_asid() {
  for (uint32_t i = 0; i < cur_config->asid_max; i++) {
    if (!BIT_TEST(asid_bitmap, i)) {
      BIT_SET(asid_bitmap, i);
      return i;
    }
  }

  PANIC(NULL, "vm/virt: Out of ASIDs");
  return 0;
}
static void free_asid(uint32_t asid) {
  if (asid > cur_config->asid_max) return;

  BIT_CLEAR(asid_bitmap, asid);
}

//////////////////////////
//    Range Functions
//////////////////////////
void vm_map_range(vm_space_t *space,
                  uintptr_t phys,
                  uintptr_t virt,
                  size_t len,
                  int flags) {
  struct vm_config *cfg = cur_config;

  // Perform necissary alignments
  if ((virt % cfg->page_size) != 0) ALIGN_DOWN(virt, cfg->page_size);
  if ((phys % cfg->page_size) != 0) ALIGN_DOWN(phys, cfg->page_size);
  if ((len % cfg->page_size) != 0) ALIGN_UP(len, cfg->page_size);

  // TODO: Use a huge mapping if we can
  int mode =
      (flags & VM_PAGE_HUGE) ? TRANSLATE_DEPTH_HUGE : TRANSLATE_DEPTH_NORM;
  size_t inc_size =
      (flags & VM_PAGE_HUGE) ? cfg->huge_page_size : cfg->page_size;

  for (uintptr_t start_phys = phys, start_virt = virt;
       start_virt < (virt + len);
       start_virt += inc_size, start_phys += inc_size) {
    uint64_t *pte = hat_translate_addr(space->root, start_virt, true, mode);
    if (pte == NULL) return;  // OOM has occured!

    *pte = hat_create_pte(flags, start_phys,
                          (flags & VM_PAGE_HUGE) ? true : false);
  }
}

void vm_unmap_range(vm_space_t *space, uintptr_t virt, size_t len) {
  struct vm_config *cfg = cur_config;

  // Perform necissary alignments
  if ((virt % cfg->page_size) != 0) ALIGN_DOWN(virt, cfg->page_size);
  if ((len % cfg->page_size) != 0) ALIGN_UP(len, cfg->page_size);

  for (uintptr_t start = virt; start < (virt + len);) {
    uint64_t *pte = hat_translate_addr(space->root, start, false, 0);
    if (pte == NULL) {
      start += cfg->page_size;
#ifdef __x86_64__
    } else if (*pte & (1 << 7)) {
      *pte = 0;
      start += cfg->huge_page_size;
#endif
    } else {
      *pte = 0;
      start += cfg->huge_page_size;
    }
  }

  // Update the TLB
  vm_invl(space, virt, len);
}

//////////////////////////
//   Space Management
//////////////////////////
vm_space_t *vm_space_create() {
  vm_space_t *trt = (vm_space_t *)kmalloc(sizeof(vm_space_t));
  trt->asid = alloc_asid();
  trt->root = (uint64_t)vm_phys_alloc(1, VM_ALLOC_ZERO);

  // Copy over the higher half from the kernel space
  uint64_t *pml4 = (uint64_t *)(trt->root + VM_MEM_OFFSET);
  for (int i = 256; i < 512; i++) {
    pml4[i] = ((uint64_t *)(kernel_space.root + VM_MEM_OFFSET))[i];
  }

  return trt;
}

void vm_space_destroy(vm_space_t *s) {
  free_asid(s->asid);
  hat_scrub_pde(s->root, cur_config->levels);
  for (int i = 0; i < s->mappings.length; i++) {
    struct vm_seg *sg = s->mappings.data[i];
    sg->ops.unmap(sg, sg->base, sg->len);
  }

  kfree(s);
}

void vm_space_fork(vm_space_t *old, vm_space_t *cur) {
  for (int i = 0; i < old->mappings.length; i++) {
    struct vm_seg *sg = old->mappings.data[i];
    sg->ops.clone(sg, cur);
  }
}

//////////////////////////
//    Misc Functions
//////////////////////////
void vm_invl(vm_space_t *spc, uintptr_t addr, size_t len) {
  if (addr == (uintptr_t)-1)
    hat_invl(spc->root, 0, spc->asid, INVL_SINGLE_ASID);

  for (uintptr_t index = addr; index < (addr + len);
       index += cur_config->page_size) {
    hat_invl(spc->root, index, spc->asid, INVL_SINGLE_ADDR);
  }
}

bool vm_fault(uintptr_t location, enum vm_fault flags) {
  uintptr_t offset;
  struct vm_seg *seg = vm_find_seg(location, &offset);
  if (seg == NULL) return false;

  if (!seg->ops.fault(seg, offset, flags)) return false;

  return true;
}

void vm_virt_init() {
  // First off, allocate the ASID bitmap (reserve ASID #0 for the kernel)
  asid_bitmap = kmalloc(sizeof(uint32_t) * (cur_config->asid_max / 8));
  BIT_SET(asid_bitmap, 0);

  // Setup the kernel space
  kernel_space.root = (uintptr_t)vm_phys_alloc(1, VM_ALLOC_ZERO);
  kernel_space.asid = 0;
  kernel_space.active = true;

  // Copy in the higher half (from the bootloader)
  uint64_t *bootloader_cr3 = (uint64_t *)asm_read_cr3();
  for (int i = 256; i < 512; i++) {
    ((uint64_t *)kernel_space.root)[i] = bootloader_cr3[i];
  }

  // Load in the kernel space
  vm_space_load(&kernel_space);

  // Scrub the TLB
  hat_invl(kernel_space.root, 0, 0, INVL_ENTIRE_TLB);
}
