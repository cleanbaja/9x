#ifndef LIB_LOCK_H
#define LIB_LOCK_H

#include <internal/asm.h>
#include <sys/cpu.h>

#define ATOMIC_READ(j) __atomic_load_n(j, __ATOMIC_SEQ_CST)
#define ATOMIC_WRITE(ptr, j) __atomic_store_n(ptr, j, __ATOMIC_SEQ_CST)
#define ATOMIC_INC(i) __sync_add_and_fetch((i), 1)

#define CREATE_LOCK(name) volatile int name = 0;
#define SPINLOCK_ACQUIRE(k) ({ asm_spinlock_acquire(&k); })
#define SLEEPLOCK_ACQUIRE(k)                                                   \
  ({                                                                           \
    if (cpu_features & (1 << 0)) {                                             \
      asm_sleeplock_acquire(&k);                                               \
    } else {                                                                   \
      asm_spinlock_acquire(&k);                                                \
    }                                                                          \
  })
#define LOCK_RELEASE(k) __atomic_clear(&k, __ATOMIC_SEQ_CST)

#endif // LIB_LOCK_H
