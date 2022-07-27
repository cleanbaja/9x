#include <ninex/proc.h>
#include <arch/smp.h>
#include <lib/builtin.h>
#include <lib/types.h>
#include <lib/kcon.h>
#include <fs/vfs.h>
#include <vm/phys.h>
#include <vm/vm.h>

#define PROC_TABLE_SIZE UINT16_MAX

struct process* kernel_process;
static proc_t* process_table[PROC_TABLE_SIZE];

proc_t* create_process(proc_t* parent, vm_space_t* space, char* ttydev) {
  // Setup the basics...
  proc_t* process = kmalloc(sizeof(proc_t));
  if (process == NULL) {
    return NULL;
  } else if (parent) {
    process->ppid = parent->pid;
    process->cwd  = parent->cwd;
    vec_push(&parent->children, process);
  } else {
    process->cwd = root_node;
  }

  // Setup the address space
  if (!space)
    space = vm_space_create();
  process->space = space;

  // Insert the files
  if (ttydev == NULL)
    ttydev = "/dev/ttyS0";

  for (int i = 0; i < 3; i++) {
    struct handle* hnd = handle_open(process, ttydev, O_RDWR, 0);
    htab_insert(&process->handles, &i, sizeof(int), hnd);
    process->fd_counter++;
  }

  // Finally, allocate a PID for this process
  uint64_t expected = 0;
  for (int i = 0; i < PROC_TABLE_SIZE; i++) {
    if (ATOMIC_CAS(&process_table[i], &expected, process)) {
      process->pid = i;
      return process;
    }
  }

  // No PID, no process!
  kfree(process);
  return NULL;
}

thread_t* kthread_create(uintptr_t entry, uint64_t arg1) {
  if (kernel_process == NULL)
    kernel_process = create_process(NULL, &kernel_space, "/dev/ttyS0");

  thread_t* new_thread = kmalloc(sizeof(thread_t));
  new_thread->parent   = kernel_process;
  new_thread->tid      = kernel_process->children.length;
  vec_push(&kernel_process->threads, new_thread);

  cpu_create_kctx(new_thread, entry, arg1);
  return new_thread; 
}

bool load_elf(vm_space_t* space, const char* path, uintptr_t base, auxval_t* auxval, uintptr_t* entry) {
  // First, try to open the file...
  bool status = false;
  struct vfs_resolved_node res = vfs_resolve(NULL, (char*)path, 0);
  if (res.target == NULL) {
    kfree(res.raw_string);
    return false;
  }
  struct vnode* file = res.target->backing;

  // Check the EHDR file header.
  Elf64_Ehdr ehdr;
  file->read(file, &ehdr, 0, sizeof(Elf64_Ehdr));
  if (!(ehdr.e_ident[0] == 0x7F && ehdr.e_ident[1] == 'E' &&
        ehdr.e_ident[2] == 'L' && ehdr.e_ident[3] == 'F')) {
    klog("proc: invalid signature for %s!", path);
    goto cleanup;
  }

  // Load in the program headers
  Elf64_Phdr* phdrs = kmalloc(ehdr.e_phnum * sizeof(Elf64_Phdr));
  file->read(file, phdrs, ehdr.e_phoff, ehdr.e_phnum * ehdr.e_phentsize);
  auxval->at_phent = sizeof(Elf64_Phdr);
  auxval->at_phnum = ehdr.e_phnum;
  auxval->at_phdr  = 0;

  for (size_t i = 0; i < ehdr.e_phnum; i++) {
    switch (phdrs[i].p_type) {
      case PT_INTERP: {
        // TODO: actually read in the dyld path, instead of predefining
        // it here!
        char* dyld_path = "/lib/ld.so";
        uintptr_t dyld_entry = 0;
        auxval_t dyld_aux = {0};
        if (!load_elf(space, dyld_path, 0x40000000, &dyld_aux, &dyld_entry)) {
          goto cleanup;
        } else {
          *entry = dyld_entry;
        }

        continue;
      }
      case PT_PHDR: {
        auxval->at_phdr = base + phdrs[i].p_vaddr;
        continue;
      }
    }

    if (phdrs[i].p_type != PT_LOAD)
      continue;

    // TODO(cleanbaja): use the segment API to map the executable
    uintptr_t misalign = phdrs[i].p_vaddr & (VM_PAGE_SIZE - 1);
    if (phdrs[i].p_memsz <= 0) {
      klog("proc: invalid p_memsz for %s?", path);
      goto cleanup;
    }

    // Translate prot to ninex format
    int pf = VM_PERM_READ | VM_PERM_USER;
    if (phdrs[i].p_flags & PF_W)
      pf |= VM_PERM_WRITE;
    if (phdrs[i].p_flags & PF_X)
      pf |= VM_PERM_EXEC;

    // Create the segment object that corresponds to this region
    struct vm_seg* n_seg = vm_create_seg(MAP_ANON | __MAP_EMBED_ONLY | MAP_PRIVATE,
                                         phdrs[i].p_flags, ALIGN_UP(misalign + phdrs[i].p_memsz, 0x1000));
    vec_push(&space->mappings, n_seg);

    // Map pages for the segment...
    uintptr_t pa = (uintptr_t)vm_phys_alloc(
        DIV_ROUNDUP(misalign + phdrs[i].p_memsz, 0x1000), VM_ALLOC_ZERO);
    uintptr_t va = (base + phdrs[i].p_vaddr) & ~(VM_PAGE_SIZE - 1);
    vm_map_range(space, pa, va,
                ALIGN_UP(misalign + phdrs[i].p_memsz, 0x1000), pf);

    // Fill the segment.
    memset((void*)(pa + VM_MEM_OFFSET + misalign), 0, phdrs[i].p_memsz);
    file->read(file, (void*)(pa + VM_MEM_OFFSET + misalign), phdrs[i].p_offset,
             phdrs[i].p_filesz);

    // Finally, fill in the pagelist of the segment object
    for (int i = 0; i < n_seg->len; i += VM_PAGE_SIZE) {
      struct vm_page* pg = kmalloc(sizeof(struct vm_page));
      uintptr_t spot = i * 0x1000;

      htab_insert(&n_seg->pagelist, &spot, sizeof(size_t), pg);
      pg->metadata = (void*)(pa + spot);
      pg->refcount = 1;
      pg->present  = 1;
    }

    n_seg->base = va;
  }

  // Set the auxval, and mark success
  auxval->at_entry = base + ehdr.e_entry;
  status = true;
  if (*entry == 0)
	*entry = auxval->at_entry;

cleanup:
  file->close(file);
  kfree(res.raw_string);
  return status;
}

thread_t* uthread_create(proc_t* parent, const char* filepath, struct exec_args arg, bool elf) {
  auxval_t aux;
  uintptr_t entry = 0;

  if (elf) {
    if (!load_elf(parent->space, filepath, 0, &aux, &entry)) {
      klog("proc: failed to load %s!", filepath);
      return NULL;
    }
  }

  thread_t* new_thread = kmalloc(sizeof(thread_t));
  new_thread->parent   = parent;
  new_thread->tid      = parent->children.length;
  vec_push(&parent->threads, new_thread);

  if (elf) {
    arg.entry = entry;
    arg.vec = aux;
  }

  cpu_create_uctx(new_thread, arg, elf);
  return new_thread;
}

