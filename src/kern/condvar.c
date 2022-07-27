#include <ninex/condvar.h>
#include <arch/smp.h>

void cv_wait(cv_t* c) {
  int status = spinlock_irq(&c->lock);
  TAILQ_INSERT_TAIL(&c->waiters, this_cpu->cur_thread, queue);
  sched_dequeue(this_cpu->cur_thread);
  c->n_waiters++;

  spinrelease_irq(&c->lock, status);
  sched_yield();
}

void cv_signal(cv_t* c) {
  int status = spinlock_irq(&c->lock);

  if (TAILQ_EMPTY(&c->waiters)) {
    spinrelease_irq(&c->lock, status);
    return;
  }

  thread_t* waiter = TAILQ_FIRST(&c->waiters);
  TAILQ_REMOVE(&c->waiters, waiter, queue);
  sched_queue(waiter);
  c->n_waiters--;

  spinrelease_irq(&c->lock, status);
  return;
}

