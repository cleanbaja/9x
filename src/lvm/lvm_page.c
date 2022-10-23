#include <lvm/lvm.h>
#include <lvm/lvm_page.h>
#include <lib/lock.h>
#include <lib/libc.h> 

// The pfndb is mapped virtually/compressed, so that holes
// in physical memory don't waste ram (entries placed next to each
// other, instead of being spaced)
struct lvm_page* lvm_pfndb = NULL;

struct pagelist modified_list = STAILQ_HEAD_INITIALIZER(modified_list);
struct pagelist zero_list = STAILQ_HEAD_INITIALIZER(zero_list);
static lock_t pfndb_lock = SPINLOCK_INIT;
uint64_t lvm_pagecount = 0;

struct lvm_page* lvm_palloc(bool zero_mem) {
    struct lvm_page *result = NULL;
    spinlock(&pfndb_lock);

    if (STAILQ_EMPTY(&modified_list) && STAILQ_EMPTY(&zero_list)) {
      spinrelease(&pfndb_lock);
      return NULL;
    }

    if (zero_mem) {
      if (STAILQ_EMPTY(&zero_list)) {
        result = STAILQ_FIRST(&modified_list);
        STAILQ_REMOVE_HEAD(&modified_list, link);
        memset((void*)((result->page_frame << 12) + LVM_HIGHER_HALF), 0, LVM_PAGE_SIZE);
      } else {
        result = STAILQ_FIRST(&zero_list);
        STAILQ_REMOVE_HEAD(&zero_list, link);
      }
    } else {
      if (STAILQ_EMPTY(&modified_list)) {
        result = STAILQ_FIRST(&zero_list);
        STAILQ_REMOVE_HEAD(&zero_list, link);
      } else {
        result = STAILQ_FIRST(&modified_list);
        STAILQ_REMOVE_HEAD(&modified_list, link);
      }
    }

    spinrelease(&pfndb_lock);
    return result;
}

struct lvm_page* lvm_find_page(uintptr_t addr) {
  int low = 0, high = lvm_pagecount;
  uintptr_t frame = addr >> 12;
  spinlock(&pfndb_lock);

  // Perform a binary search (cause its faster)
  while (low <= high) {
    int mid = low + (high - low) / 2;

    if (lvm_pfndb[mid].page_frame == frame) {
      spinrelease(&pfndb_lock);
      return &lvm_pfndb[mid];
    }

    if (lvm_pfndb[mid].page_frame < frame)
      low = mid + 1;
    else
      high = mid - 1;
  }

  spinrelease(&pfndb_lock);
  return NULL;
}

void lvm_pfree(struct lvm_page *pg) {
  if (!pg)
    return;

  spinlock(&pfndb_lock);
  STAILQ_INSERT_TAIL(&modified_list, pg, link);
  spinrelease(&pfndb_lock);
}
