#include <arch/hat.h>
#include <fs/vfs.h>
#include <lib/builtin.h>
#include <lib/elf.h>
#include <lib/kcon.h>
#include <ninex/extension.h>
#include <vm/phys.h>
#include <vm/virt.h>
#include <vm/vm.h>

CREATE_STAGE_NODEP(kext_stage, kern_load_extensions);
static vec_t(struct kernel_extension*) extensions;
static uintptr_t ext_bump_base = 0;

static uintptr_t find_base_for_mod(size_t req_len) {
  if (ext_bump_base == 0) {
    ext_bump_base = hat_get_base(HAT_BASE_KEXT);
  }

  ext_bump_base += req_len;
  return ext_bump_base - req_len;
}

static struct kernel_extension* load_extension(struct backing* file) {
  uintptr_t base = find_base_for_mod(file->st.st_size);

  // Check the EHDR file header.
  Elf64_Ehdr ehdr;
  file->read(file, &ehdr, 0, sizeof(Elf64_Ehdr));
  if (!(ehdr.e_ident[0] == 0x7F && ehdr.e_ident[1] == 'E' &&
        ehdr.e_ident[2] == 'L' && ehdr.e_ident[3] == 'F')) {
    klog("kext: invalid signature for kernel extension!");
    return NULL;
  }

  // Load all PHDRs.
  struct Elf64_Dyn* dynamic = NULL;

  for (int i = 0; i < ehdr.e_phnum; i++) {
    Elf64_Phdr phdr;
    file->read(file, &phdr, ehdr.e_phoff + i * ehdr.e_phentsize,
               sizeof(Elf64_Phdr));

    if (phdr.p_type == PT_LOAD) {
      uintptr_t misalign = phdr.p_vaddr & (VM_PAGE_SIZE - 1);
      if (phdr.p_memsz <= 0) {
        klog("kext: p_memsz is invalid for kernel extension!");
        return NULL;
      }

      // Map pages for the segment...
      int pf = VM_PERM_READ | VM_PERM_WRITE;
      if (phdr.p_flags & PF_X)
        pf |= VM_PERM_EXEC;

      uintptr_t pa = (uintptr_t)vm_phys_alloc(
          ALIGN_UP(misalign + phdr.p_memsz, 0x1000), VM_ALLOC_ZERO);
      uintptr_t va = (base + phdr.p_vaddr) & ~(VM_PAGE_SIZE - 1);
      vm_map_range(&kernel_space, pa, va,
                   ALIGN_UP(misalign + phdr.p_memsz, 0x1000), pf);

      // Fill the segment.
      memset((void*)(base + phdr.p_vaddr), 0, phdr.p_memsz);
      file->read(file, (void*)(base + phdr.p_vaddr), phdr.p_offset,
                 phdr.p_filesz);

      // Now, we can remove write permissions, if they aren't needed.
      if (!(phdr.p_flags & PF_W)) {
        vm_unmap_range(&kernel_space, va, ALIGN_UP(misalign + phdr.p_memsz, 0x1000));
        vm_map_range(&kernel_space, pa, va, ALIGN_UP(misalign + phdr.p_memsz, 0x1000),
                     pf & ~VM_PERM_WRITE);
      }
    } else if (phdr.p_type == PT_DYNAMIC) {
      dynamic = (struct Elf64_Dyn*)(base + phdr.p_vaddr);
    } else if (phdr.p_type == PT_NOTE || phdr.p_type == PT_GNU_EH_FRAME ||
               phdr.p_type == PT_GNU_STACK || phdr.p_type == PT_GNU_RELRO ||
               phdr.p_type == PT_GNU_PROPERTY) {
      // Ignore the PHDR.
    } else {
      klog("kext: Unknown PHDR type 0x%lx!", phdr.p_type);
      return NULL;
    }
  }

  // Extract symbol & relocation tables from DYNAMIC.
  char* str_tab = NULL;
  struct Elf64_Sym* sym_tab = NULL;
  const Elf64_Word* hash_tab = NULL;
  const char* plt_rels = NULL;
  size_t plt_rel_sectionsize = 0;
  uint64_t rela_offset = 0;
  uint64_t rela_size = 0;

  for (size_t i = 0; dynamic[i].d_tag != DT_NULL; i++) {
    struct Elf64_Dyn* ent = dynamic + i;
    switch (ent->d_tag) {
      // References to sections that we need to extract:
      case DT_STRTAB:
        str_tab = (char*)(base + ent->d_ptr);
        break;
      case DT_SYMTAB:
        sym_tab = (const struct Elf64_Sym*)(base + ent->d_ptr);
        break;
      case DT_HASH:
        hash_tab = (const Elf64_Word*)(base + ent->d_ptr);
        break;
      case DT_JMPREL:
        plt_rels = (char*)(base + ent->d_ptr);
        break;

      // Data that we need to extract:
      case DT_PLTRELSZ:
        plt_rel_sectionsize = ent->d_val;
        break;

      // Make sure those entries match our expectation:
      case DT_SYMENT:
        if (ent->d_val != sizeof(struct Elf64_Sym)) {
          klog("kext: ent->d_val has an improper size of %u (DT_SYMENT)",
               ent->d_val);
          return NULL;
        }
        break;

      case DT_RELA:
        rela_offset = ent->d_val;
        break;
 
      case DT_RELASZ:
        rela_size = ent->d_val;
        break;

      case DT_RELAENT:
	if (ent->d_val != sizeof(struct Elf64_Rela)) {
	  klog("kext: ent->d_val has an improper size of %u! (DT_RELAENT)", ent->d_val);
	  return NULL;
	}
	break;

      // Ignore the following entries:
      case DT_STRSZ:
      case DT_PLTGOT:
      case DT_PLTREL:
      case DT_GNU_HASH:
      case DT_RELACOUNT:
        break;
      default:
        klog("kext: unknown entry in dynamic section (type: 0x%x)", ent->d_tag);
        break;
    }
  }

  if (!str_tab || !sym_tab || !hash_tab) {
    klog("kext: missing elements in dynamic section!");
    return NULL;
  }

  // Perform Relocations.
  for (uint64_t offset = 0; offset < rela_size; offset += sizeof(struct Elf64_Rela)) {
    struct Elf64_Rela relocation;
    file->read(file, &relocation, rela_offset + offset, sizeof(struct Elf64_Rela));

    struct Elf64_Sym* symbol = sym_tab + ELF64_R_SYM(relocation.r_info);
    uint64_t* target = (uint64_t*)(base + relocation.r_offset); 
    switch (ELF64_R_TYPE(relocation.r_info)) {
      case R_X86_64_RELATIVE: {
        *target = base + relocation.r_addend;	
        break;			      
      }
      case R_X86_64_64: {
        *target = (base + symbol->st_value) + relocation.r_addend;
	break;
      }
      case R_X86_64_GLOB_DAT: {
        *target = (base + symbol->st_value);
        break;
      }
      default:
	klog("kext: unknown relocation type 0x%x!", ELF64_R_TYPE(relocation.r_info));
        break;
    } 
  }

  // Perform Patch-Ins of functions.
  for (size_t off = 0; off < plt_rel_sectionsize;
       off += sizeof(struct Elf64_Rela)) {
    const struct Elf64_Rela* reloc = (const struct Elf64_Rela*)(plt_rels + off);
    if (ELF64_R_TYPE(reloc->r_info) != R_X86_64_JUMP_SLOT) {
      klog("kext: non-JUMP_SLOT relocation in '.rela.plt'");
      return NULL;
    }
 
    uint64_t* rp = (uint64_t*)(base + reloc->r_offset);
    struct Elf64_Sym* symbol = sym_tab + ELF64_R_SYM(reloc->r_info);
    char* sym_name = str_tab + symbol->st_name;
    *rp = strace_get_symbol(sym_name);
   
    if (*rp == 0x0) {
      *rp = (base + symbol->st_value);
    }
  }

  // Look up sections.
  Elf64_Shdr shstr_tab;
  struct kernel_extension* ext_info = NULL;
  file->read(file, &shstr_tab,
             ehdr.e_shoff + ehdr.e_shstrndx * ehdr.e_shentsize,
             sizeof(Elf64_Shdr));

  for (size_t i = 0; i < ehdr.e_shnum; i++) {
    Elf64_Shdr shdr;
    file->read(file, &shdr, ehdr.e_shoff + i * ehdr.e_shentsize,
               sizeof(Elf64_Shdr));

    char name[6];
    file->read(file, name, shstr_tab.sh_offset + shdr.sh_name, 5);
    name[5] = 0;

    if (memcmp(name, ".kext", 5) == 0) {
      ext_info = (struct kernel_extension*)(base + shdr.sh_addr);
      break;
    }
  }
  
  return ext_info;
}

void kern_load_extensions() {
  // By default, kernel extensions are located in /initrd
  struct vfs_resolved_node res = vfs_resolve(NULL, "/initrd", 0);
  kfree(res.raw_string);
  if (res.target == NULL || res.target->children.length == 0) {
    res = vfs_resolve(NULL, "/lib/extensions", 0);
    if (res.target == NULL || res.target->children.length == 0)
      return; // No idea where the extensions are, so bail out
  }

  struct vfs_node* kext_parent = res.target;
  for (int i = 0; i < kext_parent->children.length; i++) {
    struct kernel_extension* ext =
        load_extension(kext_parent->children.data[i]->backing);
    if (ext == NULL) {
      klog("kext: failed to load %s!", kext_parent->children.data[i]->name);
    } else {
      if (ext->init())
        vec_push(&extensions, ext);

      // TODO: Unload a extension when it fails to load!
      klog("kext: loaded %s v%s", ext->name, ext->version);
    }
  }
}

