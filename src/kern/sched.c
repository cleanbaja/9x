#include <arch/smp.h>
#include <arch/timer.h>
#include <lib/kcon.h>
#include <lib/lock.h>
#include <ninex/irq.h>
#include <ninex/sched.h>
#include <vm/vm.h>

static struct threadlist threadq;
static struct threadlist deadq;
static lock_t queue_lock;
int resched_slot = 0;

void sched_queue(thread_t *thread) {
  spinlock(&queue_lock);
  thread->no_queue = false;
  TAILQ_INSERT_TAIL(&threadq, thread, queue);
  spinrelease(&queue_lock);
}

void sched_dequeue(thread_t *thread) {
  spinlock(&queue_lock);

  if (this_cpu->cur_thread == thread) {
    // An active thread can't be queue'd, so just
    // mark it as unqueueable
    thread->no_queue = true;
  } else {
    TAILQ_REMOVE(&threadq, thread, queue);
  }

  spinrelease(&queue_lock);
}

void sched_yield() {
  asm volatile("cli");
  timer_stop();
  this_cpu->yielded = 1;
  timer_oneshot(1, resched_slot);
  asm volatile("sti");

  while (this_cpu->yielded)
    asm volatile("hlt");
}

void sched_dequeue_and_yield() {
  asm volatile("cli");
  sched_dequeue(this_cpu->cur_thread);
  sched_yield();
}

void sched_die(thread_t *target) {
  if (target == NULL) target = this_cpu->cur_thread;

  asm volatile("cli");
  sched_dequeue(target);

  // Add the thread to the dead-list, so that the janitor
  // can clean up the resources it occupies
  TAILQ_INSERT_TAIL(&deadq, target, queue);
  if (target == this_cpu->cur_thread) sched_yield();

  // TODO: do an IPI if a CPU is running this thread (that's not us)
}

void reschedule(struct cpu_context *ctx) {
#ifdef __x86_64__
  if (ctx->cs & 3) asm_swapgs();
#endif  // __x86_64__

  if (trylock(&queue_lock)) {
    timer_oneshot(DEFAULT_TIMESLICE, resched_slot);
#ifdef __x86_64__
    if (ctx->cs & 3) asm_swapgs();
#endif  // __x86_64__

    return;
  }

  ATOMIC_WRITE(&this_cpu->yielded, 0);
  if (this_cpu->cur_thread != NULL) {
    cpu_save_thread(ctx);
    if (!this_cpu->cur_thread->no_queue)
      TAILQ_INSERT_TAIL(&threadq, this_cpu->cur_thread, queue);

    this_cpu->cur_thread = NULL;
  }

  thread_t *cur_thread = NULL;
  if (!TAILQ_EMPTY(&threadq)) {
    cur_thread = TAILQ_FIRST(&threadq);
    this_cpu->cur_thread = cur_thread;
    TAILQ_REMOVE(&threadq, cur_thread, queue);
  }

  if (cur_thread == NULL) {
    timer_oneshot(DEFAULT_TIMESLICE, resched_slot);
    vm_space_load(&kernel_space);
    this_cpu->cur_spc = &kernel_space;
    spinrelease(&queue_lock);

    asm("sti");
    for (;;)
      asm("hlt");
  }

  spinrelease(&queue_lock);
  timer_oneshot(DEFAULT_TIMESLICE, resched_slot);
  cpu_restore_thread(ctx);
}

void enter_scheduler() {
  if (resched_slot == 0) {
    struct irq_resource *res = alloc_irq_handler(&resched_slot);
    res->HandlerFunc = reschedule;
    res->procfs_name = "scheduler";
    res->eoi_strategy = EOI_MODE_TIMER;

    TAILQ_INIT(&threadq);
    TAILQ_INIT(&deadq);
  }

  // Wait for the BSP, then start the timer...
  timer_oneshot(DEFAULT_TIMESLICE, resched_slot);
}
