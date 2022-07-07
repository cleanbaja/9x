#ifndef NINEX_PROC_H
#define NINEX_PROC_H

#include <arch/irqchip.h>
#include <lib/htab.h>
#include <lib/vec.h>
#include <lib/elf.h>
#include <vm/virt.h>

#define DEFAULT_TIMESLICE 20 // A default timeslice of 20 milleseconds

struct thread;
typedef struct process {
  uint32_t pid, ppid;
  vec_t(struct process*) children;
  vec_t(struct thread*) threads;

  struct vfs_ent* cwd;
  struct hash_table handles;
  vm_space_t* space;
  int fd_counter, status;
} proc_t;

typedef struct thread {
  struct process* parent;
  uint32_t tid;

  cpu_ctx_t context;
  void* fpu_save_area;
  bool no_queue;
  uintptr_t syscall_stack;

#ifdef __x86_64__
  uintptr_t client_fs, client_gs;
#endif
} thread_t;

struct exec_args {
  const char **argp, **envp;
  auxval_t vec;
  uintptr_t entry;
};

proc_t* create_process(proc_t* parent, vm_space_t* space, char* ttydev);
thread_t* kthread_create(uintptr_t entry, uint64_t arg1);
thread_t* uthread_create(proc_t* parent, const char* filepath, struct exec_args arg);

#endif // NINEX_PROC_H
