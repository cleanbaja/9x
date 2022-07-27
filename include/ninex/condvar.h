#ifndef NINEX_CONDVAR_H
#define NINEX_CONDVAR_H

#include <ninex/sched.h>
#include <lib/lock.h>

typedef struct {
  lock_t lock;
  struct threadlist waiters;
  size_t n_waiters;
} cv_t;

#define cv_get_waiters(cvar) (cvar->n_waiters)
#define cv_init(cvar) ({      \
  TAILQ_INIT(&cvar.waiters);  \
  cvar.n_waiters = 0;         \
  cvar.lock = 0;              \
})

void cv_wait(cv_t* condvar);
void cv_signal(cv_t* condvar);

#endif // NINEX_CONDVAR_H
