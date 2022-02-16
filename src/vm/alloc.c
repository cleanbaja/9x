#include <9x/vm.h>
#include <lib/builtin.h>

#define NUM_PAGES(sz) (((sz) + 4096 - 1) / 4096)
#define DIV_ROUNDUP(A, B)                                                      \
  ({                                                                           \
    typeof(A) _a_ = A;                                                         \
    typeof(B) _b_ = B;                                                         \
    (_a_ + (_b_ - 1)) / _b_;                                                   \
  })
typedef struct
{
  size_t pages;
  size_t size;
} alloc_metadata_t;

void*
kmalloc(size_t size)
{
  size_t page_count = size / 4096;

  if (size % 4096)
    page_count++;

  char* ptr = vm_phys_alloc(page_count + 1);

  if (!ptr) {
    return (void*)0;
  }

  ptr += (size_t)VM_MEM_OFFSET;

  alloc_metadata_t* metadata = (alloc_metadata_t*)ptr;
  ptr += 4096;

  metadata->pages = page_count;
  metadata->size = size;

  return (void*)ptr;
}

void
kfree(void* ptr)
{
  alloc_metadata_t* metadata = (alloc_metadata_t*)((size_t)ptr - 4096);
  vm_phys_free((void*)((size_t)metadata - VM_MEM_OFFSET), metadata->pages + 1);
}

void*
krealloc(void* ptr, size_t new)
{
  /* check if 0 */
  if (!ptr)
    return kmalloc(new);
  if (!new) {
    kfree(ptr);
    return (void*)0;
  }

  /* Reference metadata page */
  alloc_metadata_t* metadata = (alloc_metadata_t*)((size_t)ptr - 4096);

  if ((metadata->size + 4096 - 1) / 4096 == (new + 4096 - 1) / 4096) {
    metadata->size = new;
    return ptr;
  }

  char* new_ptr;
  if ((new_ptr = kmalloc(new)) == 0) {
    return (void*)0;
  }

  if (metadata->size > new)
    /* Copy all the data from the old pointer to the new pointer,
     * within the range specified by `size`. */
    memcpy(new_ptr, (char*)ptr, new);
  else
    memcpy(new_ptr, (char*)ptr, metadata->size);

  kfree(ptr);
  return new_ptr;
}
