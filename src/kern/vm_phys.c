#include <vm.h>
#include <stddef.h>
#include <lib/log.h>
#include <lib/builtin.h>

#define MEMSTATS_USED  0
#define MEMSTATS_FREE  1
#define MEMSTATS_LIMIT 2

static uint64_t memstats[3] = {0};
static uint8_t* bitmap;
 
void vm_init_phys(struct stivale2_struct_tag_memmap* mmap) {
  uint64_t total_mem = 0;

  for (size_t i = 0; i < mmap->entries; i++) {
    struct stivale2_mmap_entry entry = mmap->memmap[i];

    // Ignore entries under 1MB
    if (entry.base + entry.length <= 0x100000)
      continue;

    // Reserved memory regions aren't always on physical ram 
    // (could be mmio, which is never free), so don't index them
    if (entry.type == STIVALE2_MMAP_USABLE || entry.type == STIVALE2_MMAP_BOOTLOADER_RECLAIMABLE) {
      uint64_t new_limit = entry.base + entry.length;
      if (new_limit > memstats[MEMSTATS_LIMIT])
        memstats[MEMSTATS_LIMIT] = new_limit;
    }
    if (entry.type == STIVALE2_MMAP_USABLE
        || entry.type == STIVALE2_MMAP_BOOTLOADER_RECLAIMABLE
        || entry.type == STIVALE2_MMAP_KERNEL_AND_MODULES) {
      total_mem += entry.length;
    }
  }

  uint64_t bitmap_size = memstats[MEMSTATS_LIMIT] / (4096 * 8);
  
  log("phys: Total memory -> %u MB (max=0x%x)", total_mem / (1024 * 1024), memstats[MEMSTATS_LIMIT]);
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
}

void* vm_phys_alloc(uint64_t pages) {

}

void vm_phys_free(void* start) {

}

