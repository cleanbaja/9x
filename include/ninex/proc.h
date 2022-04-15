#ifndef PROC_H
#define PROC_H

#include <lib/posix.h>
#include <lib/lock.h>
#include <lib/vec.h>
#include <arch/irq.h>
#include <vm/virt.h>
#include <stddef.h>

#define DEFAULT_TIMESLICE 20 // A default timeslice of 20 milleseconds

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

enum thread_state {
  THREAD_STATE_DEFAULT = 0xAF,
  THREAD_STATE_QUEUED,
  THREAD_STATE_PREEMPTED,
  THREAD_STATE_RUNNABLE,
  THREAD_STATE_RUNNING,
  THREAD_STATE_DEAD
};

typedef struct kthread
{
  // Links to other threads/parent
  struct process* parent;

  // Core information of thread
  enum thread_state state;
  uint32_t timeslice, percpu_base;
  bool stopped;

  // CPU specifiic stuff
  cpu_ctx_t context;
  uint8_t* fpu_save_area;
  uintptr_t syscall_stack, pf_stack;
} thread_t;

struct thread_node {
  thread_t *value;
  struct thread_node *next;
};

struct thread_queue {
  _Atomic(size_t) sz, capacity;
  struct thread_node *head;
  struct thread_node *tail;
  struct spinlock lck;
};

process_t*
proc_create(process_t* parent, vm_space_t* space);
thread_t*
proc_mkthread(uint64_t entry, uint64_t arg1, bool queue);

#endif // PROC_H
