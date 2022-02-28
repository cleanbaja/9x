#include <9x/vm.h>
#include <lib/builtin.h>
#include <lib/lock.h>
#include <lib/log.h>
#include <stddef.h>

#define MEMSTATS_USED 0
#define MEMSTATS_FREE 1
#define MEMSTATS_LIMIT 2

#define BIT_SET(__bit) (bitmap[(__bit) / 8] |= (1 << ((__bit) % 8)))
#define BIT_CLEAR(__bit) (bitmap[(__bit) / 8] &= ~(1 << ((__bit) % 8)))
#define BIT_TEST(__bit) ((bitmap[(__bit) / 8] >> ((__bit) % 8)) & 1)

static uint64_t memstats[3] = { 0 };
static uint8_t* bitmap = NULL;
static uint64_t last_index = 0;
static CREATE_SPINLOCK(pmm_lock);

void
vm_init_phys(struct stivale2_struct_tag_memmap* mmap)
{
  uint64_t total_mem = 0;

  for (size_t i = 0; i < mmap->entries; i++) {
    struct stivale2_mmap_entry entry = mmap->memmap[i];

    // Ignore entries under 1MB
    if (entry.base + entry.length <= 0x100000)
      continue;

    // Reserved memory regions aren't always on physical ram
    // (could be mmio, which is never free), so don't index them
    if (entry.type == STIVALE2_MMAP_USABLE ||
        entry.type == STIVALE2_MMAP_BOOTLOADER_RECLAIMABLE) {
      uint64_t new_limit = entry.base + entry.length;
      if (new_limit > memstats[MEMSTATS_LIMIT])
        memstats[MEMSTATS_LIMIT] = new_limit;
    }
    if (entry.type == STIVALE2_MMAP_USABLE ||
        entry.type == STIVALE2_MMAP_BOOTLOADER_RECLAIMABLE ||
        entry.type == STIVALE2_MMAP_KERNEL_AND_MODULES) {
      total_mem += entry.length;
    }
  }

  uint64_t bitmap_size =
    DIV_ROUNDUP(memstats[MEMSTATS_LIMIT], VM_PAGE_SIZE) / 8;

  log("phys: Total memory -> %u MB (max=0x%x)",
      total_mem / (1024 * 1024),
      memstats[MEMSTATS_LIMIT]);
  log("phys: Using a bitmap %u KB in size", bitmap_size / 1024);

  for (size_t i = 0; i < mmap->entries; i++) {
    struct stivale2_mmap_entry entry = mmap->memmap[i];

    if (entry.type != STIVALE2_MMAP_USABLE)
      continue;

    if (entry.length >= bitmap_size) {
      bitmap = (void*)(entry.base + VM_MEM_OFFSET);

      // Initialise entire bitmap to 1 (non-free)
      memset(bitmap, 0xFF, bitmap_size);

      entry.length -= bitmap_size;
      entry.base += bitmap_size;

      break;
    }
  }

  for (size_t i = 0; i < mmap->entries; i++) {
    struct stivale2_mmap_entry entry = mmap->memmap[i];

    if (entry.type != STIVALE2_MMAP_USABLE)
      continue;

    for (uintptr_t j = 0; j < entry.length; j += VM_PAGE_SIZE)
      BIT_CLEAR((entry.base + j) / VM_PAGE_SIZE);
  }

  // Activate the page frame allocator by making a quick allocation
  void* warmup_ptr = vm_phys_alloc(20);
  vm_phys_free(warmup_ptr, 10);
}

static void*
inner_alloc(size_t count, size_t limit)
{
  size_t p = 0;

  while (last_index < limit) {
    if (!BIT_TEST(last_index++)) {
      if (++p == count) {
        size_t page = last_index - count;
        for (size_t i = page; i < last_index; i++) {
          BIT_SET(i);
        }
        return (void*)(page * VM_PAGE_SIZE);
      }
    } else {
      p = 0;
    }
  }

  return NULL;
}

void*
vm_phys_alloc(size_t pages)
{
  spinlock_acquire(&pmm_lock);

  size_t l = last_index;
  void* ret = inner_alloc(pages, memstats[MEMSTATS_LIMIT] / VM_PAGE_SIZE);
  if (ret == NULL) {
    last_index = 0;
    ret = inner_alloc(pages, l);
  }
  spinlock_release(&pmm_lock);

  if (ret != NULL)
  	memset64((void*)((uintptr_t)ret + VM_MEM_OFFSET), 0, pages * 4096);
  
  return ret;
}

void
vm_phys_free(void* ptr, size_t count)
{
  spinlock_acquire(&pmm_lock);

  size_t page = (size_t)ptr / VM_PAGE_SIZE;
  for (size_t i = page; i < page + count; i++)
    BIT_CLEAR(i);

  spinlock_release(&pmm_lock);
}
