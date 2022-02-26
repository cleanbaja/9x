#ifndef LIB_LOCK_H
#define LIB_LOCK_H

#include <internal/asm.h>
#include <sys/cpu.h>

#define ATOMIC_READ(j)        __atomic_load_n(j, __ATOMIC_SEQ_CST)
#define ATOMIC_WRITE(ptr, j)  __atomic_store_n(ptr, j, __ATOMIC_SEQ_CST)
#define ATOMIC_INC(i)         __sync_add_and_fetch((i), 1)

struct spinlock {
  volatile int locked;
  volatile bool intr_enabled;
};

#define CREATE_SPINLOCK(name) struct spinlock name = {.locked = 0, .intr_enabled = false};
static inline void spinlock_acquire(struct spinlock* lock) {
  // Read rflags
  uint64_t rflags = 0;
  asm("pushf;"
      "pop %0"
      : "=r" (rflags));

  // Mask interrupts (if needed)
  if (rflags & (1 << 9)) {
    lock->intr_enabled = true;
    __asm__ volatile ("cli"); 
  }

  // Enter the busy loop
  while (lock->locked == 1) {                        
    asm volatile(                         
	"lock; cmpxchgl %1, %0\n"         
        : "=m"(lock->locked)                         
        : "r"(1), "a"(0)                  
        : "cc");                          
    __asm__ volatile ("pause");          
  }	
}
static inline void spinlock_release(struct spinlock* lock) {
  // Clear the lock to zero (available)
  __atomic_clear(&lock->locked, __ATOMIC_SEQ_CST);

  // Restore interrupts (once again, if needed)
  if (lock->intr_enabled)
    __asm__ volatile ("sti");
}

#endif // LIB_LOCK_H
