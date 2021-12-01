#include <lib/builtin.h>
#include <lib/log.h>
#include <stddef.h>
#include <vm.h>

#define MEMSTATS_USED 0
#define MEMSTATS_FREE 1
#define MEMSTATS_LIMIT 2

static uint64_t page_cache[32] = {0}; // A array of free 4KB pages
static uint64_t memstats[3] = {0};
static uint8_t *bitmap;
static uint64_t last_index = 0;

static void cache_add(uint64_t addr) {
  for (int i = 0; i < 32; i++) {
    if (page_cache[i] == 0) {
      page_cache[i] = addr;
      return;
    }
  }

  // The cache is full, return
  return;
}

static uint64_t cache_get() {
  uint64_t ret_addr = -1;
  for (int i = 0; i < 32; i++) {
    if (page_cache[i] != 0) {
      ret_addr = page_cache[i];
      page_cache[i] = 0;
      goto exit;
    }
  }

exit:
  return ret_addr;
}

static int vm_zone_used(uintptr_t start, uint64_t pages) {
  int is_used = 0;

  for (uint64_t i = start; i < start + (pages * 4096); i += 4096) {
    is_used = bitmap[i / (4096 * 8)] & (1 << ((i / 4096) % 8));
    if (!is_used)
      break;
  }

  return is_used;
}

static int vm_phys_reserve(uintptr_t start, uint64_t pages) {
  if (vm_zone_used(start, pages))
    return 0;

  for (uint64_t i = (uint64_t)start; i < ((uint64_t)start) + (pages * 4096);
       i += 4096) {
    bitmap[i / (4096 * 8)] |= 1 << ((i / 4096) % 8);
  }

  return 1;
}

void vm_init_phys(struct stivale2_struct_tag_memmap *mmap) {
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

  uint64_t bitmap_size = memstats[MEMSTATS_LIMIT] / (4096 * 8);

  log("phys: Total memory -> %u MB (max=0x%x)", total_mem / (1024 * 1024),
      memstats[MEMSTATS_LIMIT]);
  log("phys: Using a bitmap %u KB in size", bitmap_size / 1024);

  for (size_t i = 0; i < mmap->entries; i++) {
    struct stivale2_mmap_entry entry = mmap->memmap[i];

    if (entry.type != STIVALE2_MMAP_USABLE)
      continue;

    if (entry.length >= bitmap_size) {
      bitmap = (void *)(entry.base + VM_MEM_OFFSET);

      // Initialise entire bitmap to 1 (non-free)
      memset(bitmap, 0xff, bitmap_size);

      entry.length -= bitmap_size;
      entry.base += bitmap_size;

      break;
    }
  }

  for (size_t i = 0; i < mmap->entries; i++) {
    struct stivale2_mmap_entry entry = mmap->memmap[i];

    if (entry.type != STIVALE2_MMAP_USABLE)
      continue;

    vm_phys_free((void *)entry.base, entry.length / 4096);
  }

  // Guard the bitmap itself
  vm_phys_reserve((uint64_t)bitmap - VM_MEM_OFFSET, (bitmap_size / 4096) + 1);
}

void *vm_phys_alloc(uint64_t pages) {
  if (pages == 1) {
	uint64_t ret_addr;
	if ((ret_addr = cache_get()) != -1)
	  return (void*)ret_addr;
  }

  for (uint64_t i = last_index; i < memstats[MEMSTATS_LIMIT]; i += 4096) {
    if (vm_phys_reserve(i, pages))
      return (void*)i;
  }

  for (uint64_t i = 0; i < last_index; i += 4096) {
    if (vm_phys_reserve(i, pages))
      return (void*)i;
  }

  PANIC(NULL, "Physical OOM (Out Of Memory)");
  return NULL;
}

void vm_phys_free(void *start, uint64_t pages) {
  if (pages == 1) {
    cache_add((uint64_t)start);
  }

  for (uint64_t i = (uint64_t)start; i < ((uint64_t)start) + (pages * 4096);
       i += 4096) {
    bitmap[i / (4096 * 8)] &= ~(1 << ((i / 4096) % 8));
  }
}

