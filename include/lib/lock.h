#ifndef LIB_LOCK_H
#define LIB_LOCK_H

#include <internal/asm.h>

#define ATOMIC_READ(VAR)                                                       \
  ({                                                                           \
    typeof(VAR) ret = 0;                                                       \
    __asm__ volatile("lock xadd %1, %0" : "+r"(ret), "+m"(VAR) : : "memory");  \
    ret;                                                                       \
  })

#define ATOMIC_WRITE(VAR, VAL)                                                 \
  ({                                                                           \
    typeof(VAR) ret = VAL;                                                     \
    asm volatile("lock xchg %1, %0" : "+r"(ret), "+m"(VAR) : : "memory");      \
    ret;                                                                       \
  })

#define LOCK_RELEASE(k) ATOMIC_WRITE(&k, 0)
#define CREATE_LOCK(name) volatile uint64_t name = 0;

#define SPINLOCK_ACQUIRE(k) ({ asm_spinlock_acquire(&k); })

#define SLEEPLOCK_ACQUIRE(k)                                                   \
  ({                                                                           \
    if (cpu_features & (1 << 0)) {                                             \
      asm_sleeplock_acquire(&k);                                               \
    } else {                                                                   \
      asm_spinlock_acquire(&k);                                                \
    }                                                                          \
  })

extern uint64_t cpu_features;

#endif // LIB_LOCK_H
