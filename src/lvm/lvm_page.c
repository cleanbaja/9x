#include <lvm/lvm.h>
#include <lvm/lvm_page.h>

struct pagelist modified_list = STAILQ_HEAD_INITIALIZER(modified_list);
struct pagelist zero_list = STAILQ_HEAD_INITIALIZER(zero_list);

// The pfndb is mapped virtually/compressed, so that holes
// in physical memory don't waste ram (entries placed next to each
// other, instead of being spaced)
struct lvm_page* lvm_pfndb = NULL;
uint64_t lvm_pagecount = 0;

struct lvm_page* lvm_palloc(bool zero_mem) {
    struct lvm_page *result = NULL;
    
    if (zero_mem || STAILQ_EMPTY(&modified_list)) {
        if (STAILQ_EMPTY(&zero_list))
            return NULL;

	result = STAILQ_FIRST(&zero_list);
	STAILQ_REMOVE_HEAD(&zero_list, link);
    } else {
	result = STAILQ_FIRST(&modified_list);
	STAILQ_REMOVE_HEAD(&modified_list, link);
    }
	
    result->refcount++;
    return result;
}

struct lvm_page* lvm_find_page(uintptr_t addr) {
  int low = 0, high = lvm_pagecount;
  uintptr_t raw_addr = addr >> 12;

  // Perform a binary search (cause its faster)
  while (low <= high) {
    int mid = low + (high - low) / 2;

    if (lvm_pfndb[mid].page_frame == raw_addr)
      return &lvm_pfndb[mid];

    if (lvm_pfndb[mid].page_frame < raw_addr)
      low = mid + 1;
    else
      high = mid - 1;
  }

  return NULL;
}

void lvm_pfree(struct lvm_page *pg) {
  if (!pg || (pg->refcount != 1))
    return;

  pg->refcount = 0;
  STAILQ_INSERT_TAIL(&modified_list, pg, link);
}

