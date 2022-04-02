#include <lib/kcon.h>
#include <generic/sched.h>
#include <arch/tables.h>
#include <arch/cpu.h>
#include <vm/phys.h>
#include <vm/vm.h>

#define INIT_STACK_SIZE (0x1000 * 2) // 8KB initial stack
#define PROC_TABLE_SIZE UINT16_MAX

struct process* kernel_process;
static process_t* process_table[PROC_TABLE_SIZE];
static CREATE_SPINLOCK(thread_lock);

process_t*
proc_create_process(process_t* parent, vm_space_t* space)
{
  process_t* new_proc = kmalloc(sizeof(process_t));
  new_proc->parent = parent;
  new_proc->space = space;
  new_proc->anon_base = DEFAULT_ANON_BASE;

  // Try to get a PID
  uint64_t expected = 0;
  for (int i = 0; i < PROC_TABLE_SIZE; i++) {
    if (ATOMIC_CAS(&process_table[i], &expected, new_proc)) {
      new_proc->pid = i;
      return new_proc;
    }
  }

  PANIC(NULL, "NO PROCESS SLOTS AVAILABLE!\n");
  return NULL;
}

thread_t*
proc_create_kthread(uint64_t entry, uint64_t arg1)
{
  spinlock_acquire(&thread_lock);
  thread_t* new_thread = kmalloc(sizeof(thread_t));
  if (kernel_process == NULL)
    kernel_process = proc_create_process(NULL, &kernel_space);

  // Create the stack and FPU storage area
  uintptr_t nstack =
    (uintptr_t)vm_phys_alloc((INIT_STACK_SIZE / VM_PAGE_SIZE), VM_ALLOC_ZERO);
  new_thread->context.rsp = nstack + VM_MEM_OFFSET + INIT_STACK_SIZE;
  new_thread->syscall_stack = nstack + VM_MEM_OFFSET + INIT_STACK_SIZE;
  new_thread->fpu_save_area =
    (uint8_t*)((uint64_t)vm_phys_alloc(1, VM_ALLOC_ZERO) + VM_MEM_OFFSET);

  // Setup the CPU registers
  new_thread->context.rip = entry;
  new_thread->context.rdi = arg1;
  new_thread->context.rflags = 0x202;
  new_thread->context.cs = GDT_KERNEL_CODE;
  new_thread->context.ss = GDT_KERNEL_DATA;

  // Setup the remaining values
  new_thread->timeslice = DEFAULT_TIMESLICE;
  new_thread->thread_state = THREAD_STATE_READY;
  new_thread->parent = kernel_process;

  // Add the new threads to queue/parent
  vec_push(&kernel_process->threads, new_thread);
  sched_queue(new_thread);

  spinlock_release(&thread_lock);
  return new_thread;
}
