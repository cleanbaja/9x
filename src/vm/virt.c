#include <internal/asm.h>
#include <internal/cpuid.h>
#include <internal/stivale2.h>
#include <lib/builtin.h>
#include <lib/log.h>
#include <9x/vm.h>

uint64_t mmu_features = 0;
vm_space_t kernel_space;

static void
setup_feature_map()
{
  // Set mmu_features with proper values
  uint32_t eax, ebx, ecx, edx;
  cpuid_subleaf(CPUID_EXTEND_FUNCTION_1, 0x0, &eax, &ebx, &ecx, &edx);

  if (edx & CPUID_EDX_XD_BIT_AVIL) {
    mmu_features |= (1 << 0);
  }
  if (edx & CPUID_EDX_PAGE1GB) {
    mmu_features |= (1 << 5);
  }

  cpuid_subleaf(0x1, 0, &eax, &ebx, &ecx, &edx);
  if (ecx & CPUID_ECX_PCID) {
    mmu_features |= (1 << 4);
  }
  if (edx & CPUID_EDX_PGE) {
    mmu_features |= (1 << 1);
  }

  cpuid_subleaf(0x7, 0, &eax, &ebx, &ecx, &edx);
  if (ebx & CPUID_EBX_SMEP) {
    mmu_features |= (1 << 2);
  }
  if (ebx & CPUID_EBX_SMAP) {
    mmu_features |= (1 << 3);
  }
  if (!(ebx & CPUID_EBX_INVPCID)) {
    // Don't say PCID is supported if there is no invpcid...
    mmu_features &= ~(1 << 4);
  }
}

static void
enable_feature_map()
{
  // Enable IA32_EFER.NX and CR4.SMEP (both should be available on CPUs since
  // ~2011)
  if (MMU_CHECK(MM_FEAT_NX)) {
    asm_wrmsr(IA32_EFER, asm_rdmsr(IA32_EFER) | (1 << 11));
  } else {
    PANIC(NULL,
          "NX (x86_64 Execute Disable Bit) is not supported on this machine!");
  }
  if (MMU_CHECK(MM_FEAT_SMEP)) {
    asm_write_cr4(asm_read_cr4() | (1 << 20));
  } else {
    PANIC(NULL,
          "SMEP (x86_64 Supervisor Mode Execution Protection) is not supported "
          "on this machine!");
  }

  // Enable CR4.SMAP and CR4.PCIDE (both of which are not required, see below)
  // Furthermore, both are not required since PCID is somehow not
  // supported on my AMD Ryzen 5500U (2021), which I find completly ABSURD.
  // PS: AMD, if you're reading this, you guys need to step-up your game.
  if (MMU_CHECK(MM_FEAT_SMAP)) {
    asm_write_cr4(asm_read_cr4() | (1 << 21));
  }
  if (MMU_CHECK(MM_FEAT_PCID)) {
    asm_write_cr4(asm_read_cr4() | (1 << 17));
  }

  // The rest of the mmu features are enabled automatically (if present)...
}

static uint64_t*
next_level(uint64_t* prev_level, uint64_t index, bool create)
{
  if (!(prev_level[index] & 1)) {
    if (!create)
      return NULL;

    prev_level[index] = (uint64_t)vm_phys_alloc(1);
    prev_level[index] |= 0b111;
  }

  return (uint64_t*)((prev_level[index] & ~(0x1ff)) + VM_MEM_OFFSET);
}

static uint64_t
flags_to_pte(uintptr_t phys, int flags, bool huge_page)
{
  uint64_t raw_page = 0;

  if (!(flags & VM_PERM_READ)) {
    log("vm/virt: (WARN) Non-Readable mappings not supported (flags: 0x%x)",
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
  if ((flags & VM_PAGE_GLOBAL) && MMU_CHECK(MM_FEAT_GLOBL)) {
    raw_page |= (1 << 8);
  }

  // Check for cache type
  switch (flags & VM_CACHE_MASK) {
    case VM_CACHE_FLAG_UNCACHED:
      raw_page |= ~(1 << huge_page ? 12 : 7) | (1 << 4) | (1 << 3);
      break;
    case VM_CACHE_FLAG_WRITE_COMBINING:
      raw_page |= (1 << huge_page ? 12 : 7) | (1 << 4) | (1 << 3);
      break;
    case VM_CACHE_FLAG_WRITE_PROTECT:
      raw_page |= (1 << huge_page ? 12 : 7) | (1 << 4) | ~(1 << 3);
      break;

    // TODO: Write Through
    default:
      break; // Use the default memory type (Write Back), which is already optimized 
  }

  return raw_page | phys | (1 << 0); // Present plus address
}

void
vm_invl(vm_space_t* spc, uintptr_t addr)
{
  if (MMU_CHECK(MM_FEAT_PCID)) {
    asm_invpcid(INVL_ADDR, spc->pcid, addr);
  } else {
    asm_invlpg(addr);
  }
}

void
vm_virt_map(vm_space_t* spc, uintptr_t phys, uintptr_t virt, int flags)
{
  // Index the virtual address
  uint64_t pml4_index = (virt & ((uint64_t)0x1ff << 39)) >> 39;
  uint64_t pml3_index = (virt & ((uint64_t)0x1ff << 30)) >> 30;
  uint64_t pml2_index = (virt & ((uint64_t)0x1ff << 21)) >> 21;
  uint64_t pml1_index = (virt & ((uint64_t)0x1ff << 12)) >> 12;

  uint64_t *level4, *level3, *level2, *level1;

  // Do a page walk until we reach our target
  level4 = (uint64_t*)(spc->pml4 + VM_MEM_OFFSET);
  level3 = next_level(level4, pml4_index, true);
  level2 = next_level(level3, pml3_index, true);
  level1 = next_level(level2, pml2_index, true);

  // Finally, fill in the proper page
  level1[pml1_index] = flags_to_pte(phys, flags, false);

  // VM invalidation is completly bugy and unusable!
  /* if (spc->active)
    vm_invl((void*)spc, virt); */
}

uint64_t*
virt2pte(vm_space_t* spc, uintptr_t virt)
{
  uint64_t level4_index = (virt & ((uint64_t)0x1ff << 39)) >> 39;
  uint64_t level3_index = (virt & ((uint64_t)0x1ff << 30)) >> 30;
  uint64_t level2_index = (virt & ((uint64_t)0x1ff << 21)) >> 21;
  uint64_t level1_index = (virt & ((uint64_t)0x1ff << 12)) >> 12;
  uint64_t *level4, *level3, *level2, *level1;

  level4 = (uint64_t*)(spc->pml4 + VM_MEM_OFFSET);
  level3 = next_level(level4, level4_index, true);
  if (level3 == NULL)
    return NULL;

  level2 = next_level(level3, level3_index, true);
  if (level2 == NULL)
    return NULL;

  level1 = next_level(level2, level2_index, true);
  if (level1 == NULL)
    return NULL;

  return &level1[level1_index];
}

void
vm_virt_unmap(vm_space_t* spc, uintptr_t virt)
{
  uint64_t level4_index = (virt & ((uint64_t)0x1ff << 39)) >> 39;
  uint64_t level3_index = (virt & ((uint64_t)0x1ff << 30)) >> 30;
  uint64_t level2_index = (virt & ((uint64_t)0x1ff << 21)) >> 21;
  uint64_t level1_index = (virt & ((uint64_t)0x1ff << 12)) >> 12;
  uint64_t *level4, *level3, *level2, *level1;

  level4 = (uint64_t*)(spc->pml4 + VM_MEM_OFFSET);
  level3 = next_level(level4, level4_index, true);
  if (level3 == NULL)
    return;

  level2 = next_level(level3, level3_index, true);
  if (level2 == NULL)
    return;

  level1 = next_level(level2, level2_index, true);
  if (level1 == NULL)
    return;

  level1[level1_index] = 0;
}

void
vm_load_space(vm_space_t* spc)
{
  uint64_t cr3_val = spc->pml4;

  if (MMU_CHECK(MM_FEAT_PCID)) {
    if (spc->pcid >= 4096) {
      PANIC(NULL, "PCID Overflow detected!");
    } else {
      cr3_val |= spc->pcid;
    }
  }

  asm_write_cr3(spc->pml4);
  spc->active = true;
}

struct stivale2_struct_tag_framebuffer* d;

void
percpu_init_vm()
{
  // Enable all supported VM features
  enable_feature_map();

  // Load the kernel VM space
  vm_load_space(&kernel_space);

  // Load the PAT with our custom value, which changes 2 registers.
  //   PA6 => Formerly UC-, now Write Protect
  //   PA7 => Formerly UC, now Write Combining
  //
  // NOTE: The rest remain at the default, see AMD Programmer's Manual Volume 2,
  // Section 7.8.2
  asm_wrmsr(0x277, 0x105040600070406);

  // Clear the entire CPU tlb
  __asm__ volatile ("wbinvd" ::: "memory");
}

void
vm_init_virt()
{
  char buf[250];

  // Setup and print the feature map
  setup_feature_map();
  snprintf(buf,
           250,
           "virt: Features -> (%s %s %s %s %s %s)",
           MMU_CHECK(MM_FEAT_NX) ? "NX" : "-NX",
           MMU_CHECK(MM_FEAT_GLOBL) ? "GLOBAL" : "-GLOBAL",
           MMU_CHECK(MM_FEAT_SMEP) ? "SMEP" : "-SMEP",
           MMU_CHECK(MM_FEAT_SMAP) ? "SMAP" : "-SMAP",
           MMU_CHECK(MM_FEAT_PCID) ? "PCID" : "-PCID",
           MMU_CHECK(MM_FEAT_1GB) ? "1GB" : "-1GB");
  log(buf);
  
  // Setup the kernel pagemap
  kernel_space.pcid = 1;
  kernel_space.active = false;
  kernel_space.pml4 = (uintptr_t)vm_phys_alloc(1);

  // Map some memory...
  for (uintptr_t p = 0; p < 0x100000000; p += 4096) {
      vm_virt_map(&kernel_space, p, VM_MEM_OFFSET + p, VM_PERM_READ | VM_PERM_WRITE);
  }
  for (uintptr_t p = 0; p < 0x80000000; p += 4096) {
      vm_virt_map(&kernel_space, p, VM_KERN_OFFSET + p, VM_PERM_READ | VM_PERM_WRITE | VM_PERM_EXEC);
  }

  // Map all memory ranges...
  struct stivale2_struct_tag_memmap *memmap_tag = stivale2_find_tag(STIVALE2_STRUCT_TAG_MEMMAP_ID);
  for (size_t i = 0; i < memmap_tag->entries; i++) {
      for (uintptr_t p = memmap_tag->memmap[i].base; p < memmap_tag->memmap[i].length; p += 4096)
          vm_virt_map(&kernel_space, p, VM_MEM_OFFSET + p, VM_PERM_READ | VM_PERM_WRITE);
  }
  
  // Remap the frambuffer as write combining for improved 
  // speed and unmap the first page (to catch bugs)
  struct stivale2_struct_tag_framebuffer* d = 
	  (struct stivale2_struct_tag_framebuffer*)stivale2_find_tag(STIVALE2_STRUCT_TAG_FRAMEBUFFER_ID);
  uintptr_t real_base = (uintptr_t)d->framebuffer_addr - VM_MEM_OFFSET;
 
  for (uintptr_t p = real_base; p < (real_base + (d->framebuffer_width * d->framebuffer_height)); p += 4096) {
    vm_virt_map(&kernel_space, p, p + VM_MEM_OFFSET, VM_PERM_READ | VM_PERM_WRITE | VM_CACHE_FLAG_WRITE_COMBINING); 
  }
  vm_virt_unmap(&kernel_space, VM_MEM_OFFSET);
  
  // Finally, finish bootstraping the VM
  percpu_init_vm();
}

