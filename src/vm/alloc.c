#include <lib/builtin.h>
#include <lib/kcon.h>
#include <vm/phys.h>
#include <vm/vm.h>

struct slab {
  size_t alloc_size;
  uintptr_t free_start;
};

struct big_header {
  size_t pages, size;
};

struct slab_header {
  struct slab* selfptr;
};

static struct slab slablist[10] = {{8, 0},   {16, 0},  {24, 0},  {32, 0},
                                   {48, 0},  {64, 0},  {128, 0}, {256, 0},
                                   {512, 0}, {1024, 0}};

static void slab_setup(struct slab* slb, size_t ent_size) {
  slb->alloc_size = ent_size;
  slb->free_start = (uintptr_t)vm_phys_alloc(1, 0) + VM_MEM_OFFSET;

  size_t avail_size =
      VM_PAGE_SIZE - ALIGN_UP(sizeof(struct slab_header), ent_size);
  struct slab_header* slptr = (struct slab_header*)slb->free_start;
  slptr[0].selfptr = slb;
  slb->free_start += ALIGN_UP(sizeof(struct slab_header), ent_size);

  uint64_t* array = (uint64_t*)slb->free_start;
  size_t max_elem = avail_size / ent_size - 1;
  size_t factor = ent_size / 8;

  array[max_elem * factor] = 0;
  for (size_t i = 0; i < max_elem; i++) {
    array[i * factor] = (uint64_t)&array[(i + 1) * factor];
  }
}

static void* slab_alloc(struct slab* slb) {
  if (slb->free_start == 0)
    slab_setup(slb, slb->alloc_size);

  uint64_t* old_free = (uint64_t*)slb->free_start;
  slb->free_start = old_free[0];
  memset((void*)old_free, 0, slb->alloc_size);

  return (void*)old_free;
}

static void slab_free(struct slab* slb, uintptr_t ptr) {
  if (ptr == 0)
    return;

  uint64_t* new_free = (uint64_t*)ptr;
  new_free[0] = slb->free_start;
  slb->free_start = (uintptr_t)new_free;
}

static struct slab* get_slab_for_size(size_t sz) {
  for (int i = 0; i < 10; i++) {
    if (slablist[i].alloc_size >= sz) {
      return &slablist[i];
    }
  }

  return NULL;
}

//////////////////////////////////////
// Big Alloc (when sz > VM_PAGE_SIZE)
//////////////////////////////////////
static void* big_malloc(size_t size) {
  size_t n_pages = DIV_ROUNDUP(size, VM_PAGE_SIZE);
  uintptr_t raw_ptr = (uintptr_t)vm_phys_alloc(n_pages + 1, VM_ALLOC_ZERO);
  if (raw_ptr == 0)
    return NULL;

  struct big_header* mtd =
      (struct big_header*)((uint64_t)raw_ptr + VM_MEM_OFFSET);
  mtd->pages = n_pages;
  mtd->size = size;

  return (void*)((uint64_t)raw_ptr + VM_MEM_OFFSET + VM_PAGE_SIZE);
}

static void big_free(void* realptr) {
  struct big_header* mtd =
      (struct big_header*)((uint64_t)realptr - VM_PAGE_SIZE);
  vm_phys_free((void*)((uint64_t)mtd - VM_MEM_OFFSET), mtd->pages + 1);
}

static void* big_realloc(void* ptr, size_t nsize) {
  struct big_header* mtd = (struct big_header*)((uint64_t)ptr - VM_PAGE_SIZE);

  if (DIV_ROUNDUP(mtd->size, VM_PAGE_SIZE) ==
      DIV_ROUNDUP(nsize, VM_PAGE_SIZE)) {
    mtd->size = nsize;
    return ptr;
  }

  void* new_ptr = kmalloc(nsize);
  if (new_ptr == NULL)
    return NULL;

  if (mtd->size > nsize) {
    memcpy(new_ptr, ptr, nsize);
  } else {
    memcpy(new_ptr, ptr, mtd->size);
  }

  kfree(ptr);
  return new_ptr;
}

void* kmalloc(size_t size) {
  struct slab* slb = get_slab_for_size(size + 8);
  if (slb == NULL) {
    return big_malloc(size);
  }

  return slab_alloc(slb);
}

void kfree(void* ptr) {
  if (ptr == NULL)
    return;

  if (((uint64_t)ptr & (uint64_t)0xfff) == 0) {
    return big_free(ptr);
  }

  struct slab_header* slab_hdr =
      (struct slab_header*)((uint64_t)ptr & ~(uint64_t)0xfff);
  slab_free(slab_hdr->selfptr, (uintptr_t)ptr);
}

void* krealloc(void* oldptr, size_t new_size) {
  if (oldptr == NULL) {
    return kmalloc(new_size);
  }

  if (((uint64_t)oldptr & (uint64_t)0xfff) == 0) {
    return big_realloc(oldptr, new_size);
  }

  struct slab_header* slab_hdr =
      (struct slab_header*)((uint64_t)oldptr & ~(uint64_t)0xfff);
  struct slab* slb = slab_hdr->selfptr;

  if (new_size > slb->alloc_size) {
    void* newptr = kmalloc(new_size);
    memcpy(newptr, oldptr, slb->alloc_size);
    slab_free(slb, (uintptr_t)oldptr);
    return newptr;
  }

  return oldptr;
}
