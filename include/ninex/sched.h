#ifndef NINEX_SCHED_H
#define NINEX_SCHED_H

#include <ninex/proc.h>

// Define a threadlist as a TAILQ of threads
TAILQ_HEAD(threadlist, thread);

void sched_queue(thread_t* thread);
void sched_dequeue(thread_t* thread);
void sched_die(thread_t* target);
void sched_dequeue_and_yield();
void sched_yield();

void enter_scheduler();

#endif // NINEX_SCHED_H
