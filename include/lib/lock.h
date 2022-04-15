#ifndef LIB_LOCK_H
#define LIB_LOCK_H

#include <arch/asm.h>
#include <arch/cpu.h>

#define ATOMIC_READ(j)        __atomic_load_n(j, __ATOMIC_SEQ_CST)
#define ATOMIC_WRITE(ptr, j)  __atomic_store_n(ptr, j, __ATOMIC_SEQ_CST)
#define ATOMIC_INC(i)         __sync_add_and_fetch((i), 1)
#define ATOMIC_CAS(var, cond, write) __atomic_compare_exchange_n(var, cond, write, false, __ATOMIC_SEQ_CST, __ATOMIC_RELAXED)

struct spinlock {
  volatile int locked;
  volatile bool intr_enabled;
};

#define CREATE_SPINLOCK(name) struct spinlock name = {.locked = 0, .intr_enabled = false};
static inline void spinlock_acquire(struct spinlock* lock) {
  unsigned int loop_cnt = 0;
  uint64_t rflags = 0;
  
  // Save rflags, so we can manage interrupts later
  asm("pushf;"
      "pop %0"
      : "=r" (rflags));
  
  // Enter the busy loop...
  while (__sync_lock_test_and_set(&lock->locked, 1) && ++loop_cnt < 0xFFFFFFF)
    asm ("pause");                                           
  
  if (loop_cnt >= 0xFFFFFFF) {
    extern void panic(void* frame, char* fmt, ...); // Don't introduce a header dependency...
    panic(0x0, "Spinlock Deadlock Detected!\n");                                   
  }

  // Mask interrupts (if needed)
  if (rflags & (1 << 9)) {
    lock->intr_enabled = true;
    __asm__ volatile ("cli"); 
  }
}
static inline void spinlock_release(struct spinlock* lock) {
  // Clear the lock to zero (available)
  bool ints_enabled = lock->intr_enabled;
  __sync_lock_release(&lock->locked);

  // Restore interrupts (once again, if needed)
  if (ints_enabled)
    __asm__ volatile ("sti");
}

#endif // LIB_LOCK_H
