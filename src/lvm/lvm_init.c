#include <lvm/lvm.h>
#include <lvm/lvm_page.h>
#include <lib/cmdline.h>
#include <lib/libc.h>
#include <lib/panic.h>
#include <lib/print.h>
#include <misc/stivale2.h>

static char* mem_types[] = {
  [STIVALE2_MMAP_USABLE] = "usable",
  [STIVALE2_MMAP_RESERVED] = "reserved",
  [STIVALE2_MMAP_ACPI_RECLAIMABLE] = "acpi_reclaimable",
  [STIVALE2_MMAP_ACPI_NVS] = "acpi_nvs",
  [STIVALE2_MMAP_BAD_MEMORY] = "badmem",
  [STIVALE2_MMAP_BOOTLOADER_RECLAIMABLE] = "bootdata",
  [STIVALE2_MMAP_KERNEL_AND_MODULES] = "kernel+mods",
  [STIVALE2_MMAP_FRAMEBUFFER] = "framebuffer"
};

static void create_pfndb(struct stivale2_struct_tag_memmap *mm_tag) {
  STAILQ_INIT(&modified_list);
  STAILQ_INIT(&zero_list);

  // First, calculate the amount of memory required for the page db
  uintptr_t total_freemem = 0;
  for (int i = 0; i < mm_tag->entries; i++) {
      struct stivale2_mmap_entry entry = mm_tag->memmap[i];
      if (entry.type != STIVALE2_MMAP_USABLE)
        continue;

      total_freemem += entry.length;
  }

  lvm_pagecount = (total_freemem / LVM_PAGE_SIZE);
  size_t pfndb_len = lvm_pagecount * sizeof(struct lvm_page);

  // Find a memory range big enough to hold the pfndb
  for (int i = 0; i < mm_tag->entries; i++) {
      struct stivale2_mmap_entry *entry = &mm_tag->memmap[i];
      if (entry->type != STIVALE2_MMAP_USABLE)
        continue;
      else if (entry->length < pfndb_len)
        continue;

      entry->length -= pfndb_len;
      lvm_pfndb = (struct lvm_page*)(entry->base + LVM_HIGHER_HALF);
      entry->base += pfndb_len;
  }

  // Finally, fill in the pfndb according to the memory map
  struct lvm_page *cur_page = lvm_pfndb;
  for (int i = 0; i < mm_tag->entries; i++) {
      struct stivale2_mmap_entry entry = mm_tag->memmap[i];
      if (entry.type != STIVALE2_MMAP_USABLE)
        continue;

      for (size_t i = 0; i < entry.length; i += LVM_PAGE_SIZE) {
          *cur_page = (struct lvm_page) {
            .page_frame = ((entry.base + i) >> 12),
	    .type = LVM_PAGE_UNUSED
	  };

          STAILQ_INSERT_TAIL(&modified_list, cur_page, link);
	  cur_page++;
      }
  }

  kprint("lvm: pfndb @ 0x%lx, contains %u pages, occupies %u MB\n", 
    (uintptr_t)lvm_pfndb - LVM_HIGHER_HALF, 
    lvm_pagecount, 
    pfndb_len / 1024 / 1024);
}

void lvm_init() {
  struct stivale2_struct_tag_memmap *mm_tag = stivale2_get_tag(STIVALE2_STRUCT_TAG_MEMMAP_ID);
  
  if ((mm_tag->entries < 8) || cmdline_get_bool("verbose", false)) {
    kprint("lvm: dumping %d memmap entries:\n", mm_tag->entries);
    
    for (int i = 0; i < mm_tag->entries; i++) {
      struct stivale2_mmap_entry entry = mm_tag->memmap[i];
      kprint("    (0x%016lx-0x%016lx) %s\n", 
           entry.base, entry.base + entry.length,
           mem_types[entry.type]);
    }
  }

  create_pfndb(mm_tag);
  assert(lvm_pagecount > 8192);

  // Zero out a few pages for the inital mapping later
  for (int i = 0; i < 512; i++) {
    struct lvm_page *dirty_page = STAILQ_FIRST(&modified_list);
    STAILQ_REMOVE_HEAD(&modified_list, link);

    memset((void*)((dirty_page->page_frame << 12) + LVM_HIGHER_HALF), 0, 4096);
    STAILQ_INSERT_TAIL(&zero_list, dirty_page, link);
  }
}
