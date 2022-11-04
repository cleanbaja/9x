#include <lvm/lvm.h>
#include <lvm/lvm_space.h>
#include <lvm/lvm_page.h>
#include <lib/tlsf.h>
#include <lib/lock.h>
#include <lib/libc.h>

static tlsf_t* tlsf_context = NULL;
static lock_t tlsf_lock = SPINLOCK_INIT;

void tlsf_init() {
  tlsf_context = tlsf_create_with_pool((void*)LVM_HEAP_START, LVM_HEAP_SIZE);
}

void* kmalloc(size_t size) {
  spinlock(&tlsf_lock);
  void* ptr = tlsf_memalign(tlsf_context, 16, size);
  spinrelease(&tlsf_lock);

  if (ptr)
    memset(ptr, 0, size);

  return ptr;
}

void* krealloc(void* ptr, size_t size) {
  spinlock(&tlsf_lock);
  ptr = tlsf_realloc(tlsf_context, ptr, size);
  spinrelease(&tlsf_lock);

  return ptr;
}

void kfree(void *ptr) {
  spinlock(&tlsf_lock);
  tlsf_free(tlsf_context, ptr);
  spinrelease(&tlsf_lock);
}
