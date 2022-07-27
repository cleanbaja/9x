#ifndef LIB_LOCK_H
#define LIB_LOCK_H

#include <arch/asm.h>

#define ATOMIC_READ(j) __atomic_load_n(j, __ATOMIC_SEQ_CST)
#define ATOMIC_WRITE(ptr, j) __atomic_store_n(ptr, j, __ATOMIC_SEQ_CST)
#define ATOMIC_INC(i) __sync_add_and_fetch((i), 1)
#define ATOMIC_CAS(var, cond, write)                                     \
  __atomic_compare_exchange_n(var, cond, write, false, __ATOMIC_SEQ_CST, \
                              __ATOMIC_RELAXED)

typedef volatile int lock_t;
#define spinlock(x)                        \
  while (__sync_lock_test_and_set(x, 1)) { \
    asm("pause");                          \
  }
#define trylock(x) __atomic_test_and_set(x, __ATOMIC_ACQUIRE)
#define spinrelease(x) __atomic_clear(x, __ATOMIC_RELEASE)

// Spinlocks, except they disable IRQs as well
#define spinlock_irq(x)                                      \
  ({                                                         \
    if (asm_check_intr()) asm volatile("cli");               \
                                                             \
    while (__sync_lock_test_and_set(x, 1)) {                 \
      if (asm_check_intr()) asm volatile("sti; pause; cli"); \
      else                                                   \
        asm volatile("pause");                               \
    }                                                        \
                                                             \
    asm_check_intr();                                        \
  })

#define spinrelease_irq(x, irq)          \
  ({                                     \
    __atomic_clear(x, __ATOMIC_RELEASE); \
    if (irq) asm volatile("sti");        \
  })

#endif  // LIB_LOCK_H
