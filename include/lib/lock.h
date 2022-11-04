#pragma once

#include <arch/cpu.h>

// All architectures supported by 9x should support atomic instructions
#include <stdatomic.h>

#ifdef SANITIZE_LOCKS
typedef struct {
  volatile uint32_t lock_bits;
  atomic_ulong waiters;
  int cpu;
} lock_t;

#define SPINLOCK_INIT {0, 0, -1}
#else
typedef volatile uint32_t lock_t;

#define SPINLOCK_INIT 0
#endif

static inline void spinlock(lock_t* lck) {
#ifdef SANITIZE_LOCKS
  if (lck->cpu == cpunum())
    __builtin_trap();

  lck->waiters++;
  while (__sync_lock_test_and_set(&lck->lock_bits, 1) != 0)
    cpu_pause();
  
  // To make sure loads/stores to the spinlock structure
  // don't cause any race conditions, don't move any loads
  // and stores past this point...
  __sync_synchronize();
  
  lck->waiters--;
  lck->cpu = cpunum();
#else
  while (__sync_lock_test_and_set(lck, 1) != 0)
    cpu_pause();
#endif
}

static inline void spinrelease(lock_t* lck) {
#ifdef SANITIZE_LOCKS
  if (lck->cpu != cpunum())
    __builtin_trap();
  else
    lck->cpu = -1;

  // To make sure loads/stores to the spinlock structure
  // don't cause any race conditions, don't move any loads
  // and stores past this point...
  __sync_synchronize();
  
  __sync_lock_release(&lck->lock_bits);
#else
  __sync_lock_release(lck);
#endif
}
