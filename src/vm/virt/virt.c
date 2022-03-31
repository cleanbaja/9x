#include <arch/asm.h>
#include <arch/hat.h>
#include <lib/builtin.h>
#include <lib/log.h>
#include <lib/stivale2.h>

#include <vm/phys.h>
#include <vm/virt.h>
#include <vm/vm.h>

vm_space_t kernel_space;
uint32_t asid_bitmap[VM_ASID_MAX / 8] = {0};

static uint32_t alloc_asid() {
  for (uint32_t i = 0; i < VM_ASID_MAX; i++) {
    if (!BIT_TEST(asid_bitmap, i)) {
      BIT_SET(asid_bitmap, i);
      return i;
    }
  }

  PANIC(NULL, "vm/virt: Out of ASIDs");
  return 0;
}

static void free_asid(uint32_t asid) {
  if (asid > VM_ASID_MAX)
    return;
 
  BIT_CLEAR(asid_bitmap, asid);
}


vm_space_t* vm_create_space() {
  vm_space_t* trt = (vm_space_t*)kmalloc(sizeof(vm_space_t));
  trt->asid = (uint16_t)alloc_asid();
  trt->root = (uint64_t)vm_phys_alloc(1, VM_ALLOC_ZERO);

  // Copy over the higher half from the kernel space
  uint64_t* pml4 = (uint64_t*)trt->root;
  for (int i = 256; i < 512; i++) {
    pml4[i] = ((uint64_t*)kernel_space.root)[i];
  } 

  return trt;
}

void
vm_virt_map(vm_space_t* spc, uintptr_t phys, uintptr_t virt, int flags)
{
  if (flags & VM_PAGE_HUGE) {
    hat_map_huge_page(spc->root, phys, virt, flags);
  } else {
    hat_map_page(spc->root, phys, virt, flags);
  }
}

uint64_t*
virt2pte(vm_space_t* spc, uintptr_t virt)
{
  return hat_resolve_addr(spc->root, virt);
}

void
vm_virt_unmap(vm_space_t* spc, uintptr_t virt)
{
  uint64_t* pte   = hat_resolve_addr(spc->root, virt);
  bool was_active = (pte == NULL) ? false : (*pte & (1 << 7));
  if (pte != NULL)
    *pte = 0;

  if (spc->active && was_active)
    vm_invl_addr(spc, virt);
}

void
vm_virt_fragment(vm_space_t* spc, uintptr_t virt, int flags)
{
  // Check to see if fragmentation is required in the first place
  uint64_t* pte = virt2pte(spc, virt);
  if ((pte == NULL) || !(*pte & (1 << 7)))
    return;

  // Turn a 2MB mapping to a much smaller 4KB mapping
  uintptr_t aligned_address = (virt / (VM_PAGE_SIZE * 0x200)) * (VM_PAGE_SIZE * 0x200);
  vm_virt_unmap(spc, aligned_address);

  for (size_t i = aligned_address; i < (aligned_address + 0x200000); i += 0x1000) {
    vm_virt_map(spc, aligned_address - VM_MEM_OFFSET, aligned_address, flags);
  }

  // Then update the TLB
  vm_invl_range(spc, aligned_address, aligned_address + 0x200000);
}

void
vm_load_space(vm_space_t* spc)
{
  uint64_t cr3_val = spc->root;

  if (CPU_CHECK(CPU_FEAT_PCID)) {
    if (spc->asid >= VM_ASID_MAX) {
      PANIC(NULL, "PCID %u is larger than VM_ASID_MAX (%d), resuling in a overflow!\n", spc->asid, VM_ASID_MAX);
    } else {
      // Set bit 63, to prevent flushing and set the PCID
      cr3_val |= (uint16_t)spc->asid | (1ull << 63);
    }
  }

  asm_write_cr3(cr3_val);
  spc->active = true;
}

struct stivale2_struct_tag_framebuffer* d;

void
percpu_init_vm()
{
  // Load the kernel VM space
  vm_load_space(&kernel_space);

  // Load the PAT with our custom value, which changes 2 registers.
  //   PA6 => Formerly UC-, now Write Protect
  //   PA7 => Formerly UC, now Write Combining
  //
  // NOTE: The rest remain at the default, see AMD Programmer's Manual Volume 2,
  // Section 7.8.2
  asm_wrmsr(0x277, 0x105040600070406);

  // Clear the entire CPU TLB
  hat_invl(kernel_space.root, 0, 0, INVL_ENTIRE_TLB);
}

void
vm_init_virt(struct stivale2_struct_tag_memmap* mmap)
{
  // Setup the kernel pagemap
  kernel_space.root   = 1;
  kernel_space.active = false;
  kernel_space.root   = (uintptr_t)vm_phys_alloc(1, VM_ALLOC_ZERO);

  // Map some memory...
  for(size_t i = 0, phys = 0; i < 0x400; i++, phys += 0x200000) {
    vm_virt_map(&kernel_space, phys, phys + VM_KERN_OFFSET, VM_PERM_READ | VM_PERM_WRITE | VM_PERM_EXEC | VM_PAGE_HUGE);
  }
  for(size_t i = 0, phys = 0; i < 0x800; i++, phys += 0x200000) {
    vm_virt_map(&kernel_space, phys, phys + VM_MEM_OFFSET, VM_PERM_READ | VM_PERM_WRITE | VM_PAGE_HUGE);
  }

  // Map the remaining parts of memory
  for(size_t i = 0, phys = 0; i < mmap->entries; i++) {
    phys = (mmap->memmap[i].base / 0x200000) * 0x200000;
    for(size_t j = 0; j < DIV_ROUNDUP(mmap->memmap[i].length, 0x200000); j++) {
      vm_virt_map(&kernel_space, phys, phys + VM_MEM_OFFSET, VM_PERM_READ | VM_PERM_WRITE | VM_PAGE_HUGE);
      phys += 0x200000;
    }
  }

  // Map the stivale2-provided framebuffer
  struct stivale2_struct_tag_framebuffer* d;
  d = (struct stivale2_struct_tag_framebuffer*)stivale2_find_tag(STIVALE2_STRUCT_TAG_FRAMEBUFFER_ID);
  uintptr_t real_base = (((uintptr_t)d->framebuffer_addr - VM_MEM_OFFSET) / (0x200000)) * (0x200000);
  uintptr_t real_size = DIV_ROUNDUP((d->framebuffer_height * d->framebuffer_pitch), 0x200000);

  for (uintptr_t i = real_base; i < real_size; i += 0x200000) {
    vm_virt_map(&kernel_space, real_base, real_base + VM_MEM_OFFSET, VM_PERM_READ | VM_PERM_WRITE | VM_PAGE_HUGE | VM_CACHE_WRITE_COMBINING);
  }

  // Finally, finish bootstraping the VM
  percpu_init_vm();
}

