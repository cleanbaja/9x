#ifndef PROC_SCHED_H
#define PROC_SCHED_H

#include <proc/proc.h>

void
sched_init();
void
sched_queue(thread_t* thrd);

// Functions for managing threads
void
sched_dequeue(thread_t* thrd);
void
sched_kill(thread_t* thrd);

#endif // PROC_SCHED_H
