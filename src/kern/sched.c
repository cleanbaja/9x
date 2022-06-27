#include <arch/timer.h>
#include <arch/smp.h>
#include <ninex/irq.h>
#include <ninex/sched.h>
#include <lib/kcon.h>
#include <lib/lock.h>
#include <vm/vm.h>

static lock_t queue_lock;
static thread_t* idle_thread = NULL;
static int resched_slot = 0;
static struct thread_queue threadlist;

static void enqueue_thread(struct thread_queue* q, thread_t* value) {
  struct thread_node* node = kmalloc(sizeof(struct thread_node));
  if (node == NULL) {
    return;
  } else if (value->no_queue) {
    kfree(node);
    return;
  }

  node->value = value;
  node->next = NULL;

  if (q->head == NULL) {
    q->head = node;
    q->tail = node;
    q->n_elem = 1;

    return;
  }

  q->tail->next = node;
  q->tail = node;
  q->n_elem += 1;
}

static thread_t* dequeue_thread(struct thread_queue* q) {
  if (q->n_elem == 0)
    return NULL;

  thread_t* value = NULL;
  struct thread_node* tmp = NULL;

  value = q->head->value;
  tmp = q->head;
  q->head = q->head->next;
  q->n_elem -= 1;

  kfree(tmp);
  return value;
}

static void remove_thread(struct thread_queue* q, thread_t* target) {
  if (q->n_elem == 0)
    return;

  struct thread_node* nd = q->head;
  struct thread_node* old_nd = q->head;
  while (nd != NULL) {
    if (nd->value == target) {
      if (nd->next)
        old_nd->next = nd->next;
      else
        old_nd->next = NULL;

      kfree(nd);
      return;
    }

    nd = nd->next;
    old_nd = nd;
  }
}

static void reschedule(struct cpu_context *ctx) {
#ifdef __x86_64__
    if (ctx->cs & 3)
      asm_swapgs();
#endif // __x86_64__

    if (!trylock(&queue_lock)) {
        timer_oneshot(DEFAULT_TIMESLICE, resched_slot);
        #ifdef __x86_64__
        if (ctx->cs & 3)
          asm_swapgs();
        #endif // __x86_64__

        return;
    }

    ATOMIC_WRITE(&this_cpu->yielded, 0);
    if (this_cpu->cur_thread != NULL) {
        cpu_save_thread(ctx);
        enqueue_thread(&threadlist, this_cpu->cur_thread);
    }

    this_cpu->cur_thread = dequeue_thread(&threadlist);
    thread_t* cur_thread = this_cpu->cur_thread;

    if (cur_thread == NULL) {
        timer_oneshot(DEFAULT_TIMESLICE, resched_slot);
        spinrelease(&queue_lock);

        asm ("sti");
        for (;;) asm ("hlt");
    }

    spinrelease(&queue_lock);
    timer_oneshot(DEFAULT_TIMESLICE, resched_slot);
    cpu_restore_thread(ctx);
}

void sched_queue(thread_t* thread) {
  spinlock(&queue_lock);
  enqueue_thread(&threadlist, thread);
  spinrelease(&queue_lock);
}

void sched_dequeue(thread_t* thread) {
  spinlock(&queue_lock);

  if (this_cpu->cur_thread == thread) {
    // An active thread can't be queue'd, so just 
    // mark it as unqueueable
    thread->no_queue = true;
  } else {
    remove_thread(&threadlist, thread);
  }

  spinrelease(&queue_lock);
}

void sched_yield() {
  asm volatile ("cli");
  timer_stop();
  this_cpu->yielded = 1;
  timer_oneshot(1, resched_slot);
  asm volatile ("sti");

  while (this_cpu->yielded)
    asm volatile ("hlt");
}

void sched_leave() {
  asm volatile ("cli");
  sched_dequeue(this_cpu->cur_thread);
  sched_yield();
}

// static _Atomic(int) waitgate = 0;
void sched_setup() {
  if (resched_slot == 0) {
    struct irq_resource* res = alloc_irq_handler(&resched_slot);
    res->HandlerFunc  = reschedule;
    res->procfs_name   = "scheduler";
    res->eoi_strategy = EOI_MODE_TIMER;
  }

  // Wait for the BSP, then start the timer...
  timer_oneshot(DEFAULT_TIMESLICE, resched_slot);
}
