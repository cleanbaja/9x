#include <lvm/lvm.h>
#include <lvm/lvm_page.h>
#include <lvm/lvm_space.h>
#include <lib/cmdline.h>
#include <lib/libc.h>
#include <lib/panic.h>
#include <lib/print.h>
#include <misc/limine.h>

static char* mem_types[] = {
  [LIMINE_MEMMAP_USABLE] = "usable",
  [LIMINE_MEMMAP_RESERVED] = "reserved",
  [LIMINE_MEMMAP_ACPI_RECLAIMABLE] = "acpi_reclaimable",
  [LIMINE_MEMMAP_ACPI_NVS] = "acpi_nvs",
  [LIMINE_MEMMAP_BAD_MEMORY] = "badmem",
  [LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE] = "bootdata",
  [LIMINE_MEMMAP_KERNEL_AND_MODULES] = "kernel+mods",
  [LIMINE_MEMMAP_FRAMEBUFFER] = "framebuffer"
};

volatile struct limine_memmap_request mm_req = {
  .id = LIMINE_MEMMAP_REQUEST,
  .revision = 0
};

volatile static struct limine_kernel_address_request kaddr_req = {
  .id = LIMINE_KERNEL_ADDRESS_REQUEST,
  .revision = 0
};

static void create_pfndb(struct limine_memmap_response *mm_resp) {
  STAILQ_INIT(&modified_list);
  STAILQ_INIT(&zero_list);

  // First, calculate the amount of memory required for the page db
  uintptr_t total_freemem = 0;
  for (int i = 0; i < mm_resp->entry_count; i++) {
      struct limine_memmap_entry entry = *mm_resp->entries[i];
      if (entry.type != LIMINE_MEMMAP_USABLE)
        continue;

      total_freemem += entry.length;
  }

  lvm_pagecount = (total_freemem / LVM_PAGE_SIZE);
  size_t pfndb_len = lvm_pagecount * sizeof(struct lvm_page);

  // Find a memory range big enough to hold the pfndb
  for (int i = 0; i < mm_resp->entry_count; i++) {
      struct limine_memmap_entry *entry = mm_resp->entries[i];
      if (entry->type != LIMINE_MEMMAP_USABLE)
        continue;
      else if (entry->length < pfndb_len)
        continue;

      entry->length -= pfndb_len;
      lvm_pfndb = (struct lvm_page*)(entry->base + LVM_HIGHER_HALF);
      entry->base += pfndb_len;
  }

  // Finally, fill in the pfndb according to the memory map
  struct lvm_page *cur_page = lvm_pfndb;
  for (int i = 0; i < mm_resp->entry_count; i++) {
      struct limine_memmap_entry entry = *mm_resp->entries[i];
      if (entry.type != LIMINE_MEMMAP_USABLE)
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

void lvm_setup_kspace() {
  struct limine_memmap_response *mm_resp = mm_req.response;
  uintptr_t kernel_vbase = kaddr_req.response->virtual_base;
	uintptr_t kernel_pbase = kaddr_req.response->physical_base;
  int default_flags = LVM_PERM_READ | LVM_TYPE_GLOBAL | LVM_PERM_WRITE;

  lvm_map_page(&kspace, kernel_vbase, kernel_pbase, 0x400 * 0x1000, default_flags | LVM_PERM_EXEC);
  lvm_map_page(&kspace, LVM_HIGHER_HALF, 0, 0x800ull * 0x200000ull, default_flags);

	for(uint64_t i = 0; i < mm_resp->entry_count; i++) {
    struct limine_memmap_entry entry = *mm_resp->entries[i];
		uint64_t aligned_base = (entry.base / 0x200000) * 0x200000;
		
    lvm_map_page(&kspace, aligned_base + LVM_HIGHER_HALF, aligned_base, 
      ALIGN_DOWN(entry.length, 0x200000), default_flags | LVM_PERM_WRITE);
	}
}

void lvm_init() {
  struct limine_memmap_response *mm_resp = mm_req.response;

  if ((mm_resp->entry_count < 8) || cmdline_get_bool("verbose", false)) {
    kprint("lvm: dumping %d memmap entries:\n", mm_resp->entry_count);
    
    for (int i = 0; i < mm_resp->entry_count; i++) {
      struct limine_memmap_entry entry = *mm_resp->entries[i];
      kprint("    (0x%016lx-0x%016lx) %s\n", 
           entry.base, entry.base + entry.length,
           mem_types[entry.type]);
    }
  }

  create_pfndb(mm_resp);
  assert(lvm_pagecount > 8192);

  // Zero out a few pages for the inital mapping later
  for (int i = 0; i < 1024; i++) {
    struct lvm_page *dirty_page = STAILQ_FIRST(&modified_list);
    STAILQ_REMOVE_HEAD(&modified_list, link);

    memset((void*)((dirty_page->page_frame << 12) + LVM_HIGHER_HALF), 0, 4096);
    STAILQ_INSERT_TAIL(&zero_list, dirty_page, link);
  }

  // Setup arch-specifc structures and paging registers
  pmap_init();
}
