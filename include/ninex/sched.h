#ifndef NINEX_SCHED_H
#define NINEX_SCHED_H

#include <ninex/proc.h>

struct thread_node {
  thread_t *value;
  struct thread_node *next;
};

struct thread_queue {
  int n_elem;
  struct thread_node *head;
  struct thread_node *tail;
};

void sched_queue(thread_t* thread);
void sched_dequeue(thread_t* thread);
void sched_leave();
void sched_setup();

#endif // NINEX_SCHED_H
