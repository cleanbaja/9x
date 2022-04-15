#include <lib/kcon.h>
#include <ninex/proc.h>
#include <arch/cpu.h>
#include <vm/phys.h>
#include <vm/vm.h>

#define PROC_TABLE_SIZE UINT16_MAX

struct process* kernel_process;
static process_t* process_table[PROC_TABLE_SIZE];
static CREATE_SPINLOCK(thread_lock);

process_t*
proc_create(process_t* parent, vm_space_t* space)
{
  process_t* new_proc = kmalloc(sizeof(process_t));
  new_proc->parent = parent;
  new_proc->space = space;

  // Try to get a PID
  uint64_t expected = 0;
  for (int i = 0; i < PROC_TABLE_SIZE; i++) {
    if (ATOMIC_CAS(&process_table[i], &expected, new_proc)) {
      new_proc->pid = i;
      klog("proc: created new process with PID #%d", i);
      return new_proc;
    }
  }

  PANIC(NULL, "NO PROCESS SLOTS AVAILABLE!\n");
  return NULL;
}

static uintptr_t create_stack(bool user) {
  if (user) {
    klog("proc: (TODO) support creating usermode stacks (with mmap)!");
    return 0x0;
  } else {
    // We don't need to map the stack, since all usable memory is already mapped into the higher half!
    return (uintptr_t)vm_phys_alloc(2, VM_ALLOC_ZERO) + (VM_PAGE_SIZE*2) + VM_MEM_OFFSET;
  }
}

thread_t*
proc_mkthread(uint64_t entry, uint64_t arg1, bool queue)
{
  // Create the kernel process, if needed
  if (kernel_process == NULL)
    kernel_process = proc_create(NULL, &kernel_space);
  
  thread_t* thr  = kmalloc(sizeof(thread_t));
  thr->parent    = kernel_process;
  thr->timeslice = DEFAULT_TIMESLICE;
  thr->state     = THREAD_STATE_DEFAULT;
  
  // Create the stacks
  thr->pf_stack        = create_stack(false);
  thr->syscall_stack   = create_stack(false);
  uintptr_t main_stack = create_stack(false);

  // Then setup the CPU context...
  cpu_create_context(thr, main_stack, entry, false);

  // Finally, register the thread to its parent, and queue if needed...
  vec_push(&kernel_process->threads, thr);

  return thr;
}
