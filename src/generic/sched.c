#include <lib/builtin.h>
#include <lib/kcon.h>
#include <generic/sched.h>
#include <generic/smp.h>
#include <arch/apic.h>
#include <arch/cpu.h>
#include <vm/phys.h>
#include <vm/vm.h>

#define MY_QUEUE (&cpu_queues[cpunum()])
#define AFFINITY_PRESENT (1ull << 31ull)
static struct thread_queue* cpu_queues;
static int sched_slot = 0;
static thread_t** idle_threads;

// NOTE: These functions don't lock, since its the caller's job to do it!
static void
queue_insert_back(struct thread_queue* q, thread_t* th)
{
  if (th->affinity & AFFINITY_PRESENT) {
    queue_insert_back(&cpu_queues[(th->affinity & ~AFFINITY_PRESENT)], th);
    return;
  }

  if (q->head == NULL) {
    q->head = th;
  } else {
    thread_t* tmp = q->head;
    while (tmp->next != NULL) {
      tmp = tmp->next;
    }

    tmp->next = th;
    th->prev = tmp;
  }

  q->n_elem++;
}

static thread_t*
queue_pop_front(struct thread_queue* q)
{
  thread_t* to_return = NULL;

  if (q->head != NULL) {
    thread_t* tmp = q->head;

    q->head = q->head->next;
    to_return = tmp;

    if (q->head != NULL)
      q->head->prev = NULL;

    q->n_elem--;
  }

  if (to_return != NULL)
    to_return->next = to_return->prev = NULL;

  return to_return;
}

static void
queue_remove(struct thread_queue* q, thread_t* th)
{
  thread_t* cur_thread = q->head;
  while (cur_thread != NULL) {
    if (cur_thread == th) {
      // Weave the 2 threads together, and remove this one from the middle
      thread_t* old = cur_thread->prev;
      thread_t* new = cur_thread->next;
      old->next = new;
      new->prev = old;

      cur_thread->prev = cur_thread->next = NULL;
      return;
    }

    cur_thread = cur_thread->next;
  }
}

static bool
check_thread(thread_t* th, bool stealing)
{
  switch (th->thread_state) {
    case THREAD_STATE_READY:
      return true;
    case THREAD_STATE_DEAD:
      vm_phys_free(th->fpu_save_area, 1);
      kfree(th);
      return false;
    case THREAD_STATE_RUNNING:
    case THREAD_STATE_PAUSED:
    case THREAD_STATE_BLOCKED: // TODO: deal with blocked threads
      return false;
  }
}

thread_t*
find_new_thread(struct thread_queue* q, bool stealing)
{
  spinlock_acquire(&q->guard);

  thread_t* new_thread = queue_pop_front(q);
  spinlock_release(&q->guard);

  if (new_thread == NULL) {
    return NULL;
  } else {
    bool result = check_thread(new_thread, stealing);
    if (result)
      return new_thread;
    else
      return NULL;
  }
}

static thread_t*
steal_from_cpu()
{
  for (int i = 0; i < (total_cpus); i++) {
    if (cpu_queues[i].n_elem == 0)
      continue;

    thread_t* result = find_new_thread(&cpu_queues[i], true);
    if (result == NULL)
      continue;

    return result;
  }

  return NULL;
}

static void
idle()
{
  asm volatile("sti");
  for (;;) {
    asm volatile("hlt");
  }
}

static void
reschedule(cpu_ctx_t* context, void* userptr)
{
  (void)userptr;

  // First things first, try to save the old context, if needed
  if (per_cpu(cur_thread) != NULL &&
      per_cpu(cur_thread) != idle_threads[cpunum()]) {
    per_cpu(cur_thread)->context = *context;
    fpu_save(per_cpu(cur_thread)->fpu_save_area);
    if (per_cpu(cur_thread)->thread_state == THREAD_STATE_RUNNING)
      per_cpu(cur_thread)->thread_state = THREAD_STATE_READY;

    sched_queue(per_cpu(cur_thread));
    per_cpu(cur_thread) = NULL;
  }

  // Find a new thread
  if (MY_QUEUE->n_elem != 0) {
    thread_t* new_thread = find_new_thread(MY_QUEUE, false);
    if (new_thread != NULL) {
      per_cpu(cur_thread) = new_thread;
      goto found;
    }
  }

  // Try to steal a thread from another CPU
  thread_t* another_thread = steal_from_cpu();
  if (another_thread != NULL) {
    per_cpu(cur_thread) = another_thread;
  } else {
    // We've ran out of options, so try again in a little bit
    per_cpu(cur_thread) = idle_threads[cpunum()];
  }

found:
  // Load the new thread
  *context = per_cpu(cur_thread)->context;
  fpu_restore(per_cpu(cur_thread)->fpu_save_area);
  apic_oneshot(sched_slot, per_cpu(cur_thread)->timeslice);
  per_cpu(cur_thread)->thread_state = THREAD_STATE_RUNNING;

  // Update CR3/IA32_FS_BASE if needed
  if (per_cpu(cur_thread)->parent->space->root != asm_read_cr3()) {
    per_cpu(cur_space) = per_cpu(cur_thread)->parent->space;
    vm_load_space(per_cpu(cur_thread)->parent->space);
    asm_wrmsr(IA32_FS_BASE, per_cpu(cur_thread)->fs_base);
  }
}

void
sched_init()
{
  if (cpu_queues == NULL) {
    cpu_queues = (struct thread_queue*)kmalloc(sizeof(struct thread_queue) *
                                               (total_cpus + 1));
    idle_threads = kmalloc(sizeof(void*) * (total_cpus + 1));
    for (int i = 0; i < (total_cpus + 1); i++) {
      idle_threads[i] = proc_create_kthread((uint64_t)idle, 0);
    }

    sched_slot = find_irq_slot((struct irq_handler){
      .is_irq = true, .should_return = true, .hnd = reschedule });
  }

  apic_oneshot(sched_slot, DEFAULT_TIMESLICE);
}

void
sched_queue(thread_t* thrd)
{
  if (thrd->thread_state != THREAD_STATE_READY)
    return;

  spinlock_acquire(&MY_QUEUE->guard);
  queue_insert_back(MY_QUEUE, thrd);
  spinlock_release(&MY_QUEUE->guard);
}

// The thread can have three states...
//   1. We are running it, so we panic
//   2. Another CPU is running it, so we shoot them down, and follow case 3
//   3. Nobody is running it, so we purge it from all queues
void
sched_dequeue(thread_t* thrd)
{
  // Set the status to paused, so that it dosen't get pushed back in to the
  // queue
  if (thrd->thread_state == THREAD_STATE_RUNNING) {
    thrd->thread_state = THREAD_STATE_PAUSED;
    return;
  }

  // Next, see if the thread is QUEUE'd somewhere in one of the threads
  for (int i = 0; i < (total_cpus); i++) {
    if (cpu_queues[i].n_elem == 0)
      continue;

    queue_remove(&cpu_queues[i], thrd);
  }
}

void
sched_kill_thread(thread_t* thrd)
{
  bool was_running =
    (thrd->thread_state == THREAD_STATE_RUNNING) ? true : false;
  thrd->thread_state = THREAD_STATE_DEAD;

  // Send a shootdown, if the thread was running
  if (was_running) {
    for (int i = 0; i < (total_cpus); i++) {
      if (cpu_locals[i] == NULL || cpu_locals[i]->cur_thread == NULL)
        continue;

      if (cpu_locals[i]->cur_thread == thrd)
        apic_send_ipi(sched_slot, cpu_locals[i]->lapic_id, IPI_SPECIFIC);
    }
  }
}
