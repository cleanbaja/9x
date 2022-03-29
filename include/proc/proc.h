#ifndef PROC_H
#define PROC_H

#include <internal/posix.h>
#include <lib/lock.h>
#include <lib/vec.h>
#include <sys/irq.h>
#include <vm/virt.h>

#define DEFAULT_TIMESLICE 20 // A default timeslice of 20 milleseconds
#define DEFAULT_ANON_BASE                                                      \
  0x70000000000 // Sys-V ABI stack base (also used for anon mappings)

typedef struct process
{
  // Basic information
  pid_t pid;
  vm_space_t* space;
  struct process* parent;
  uintptr_t anon_base;

  // List of children threads/processes
  vec_t(struct process*) children;
  vec_t(struct kthread*) threads;
} process_t;

typedef struct kthread
{
  // Links to other threads/parent
  struct process* parent;
  struct kthread *prev, *next;

  // Core information of thread
  enum
  {
    THREAD_STATE_READY,
    THREAD_STATE_RUNNING,
    THREAD_STATE_BLOCKED,
    THREAD_STATE_DEAD,
    THREAD_STATE_PAUSED
  } thread_state;
  uint32_t exit_code, timeslice, affinity;
  uint64_t syscall_stack, fs_base, anon_base;

  // CPU contexts
  cpu_ctx_t context;
  uint8_t* fpu_save_area;
} thread_t;

struct thread_queue
{
  struct kthread* head;
  struct spinlock guard;
  uintptr_t n_elem;
};

process_t*
proc_create_process(process_t* parent, vm_space_t* space);
thread_t*
proc_create_kthread(uint64_t entry, uint64_t arg1);
thread_t*
proc_create_uthread(process_t* parent,
                    const char** argv,
                    const char** envp,
                    auxval_t* vector);

#endif // PROC_H
