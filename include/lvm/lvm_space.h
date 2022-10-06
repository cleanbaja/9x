#pragma once

#include <arch/pmap.h>
#include <lib/lock.h>
#include <stddef.h>
#include <stdbool.h>

#define LVM_CACHE_TYPE(bits) ((bits) << 16)

enum lvm_map_flags {
  LVM_PERM_READ = 1,
  LVM_PERM_WRITE = 2,
  LVM_PERM_EXEC = 4,
  LVM_TYPE_USER = 8,
  LVM_TYPE_HUGE = 16,
  LVM_TYPE_GLOBAL = 32
};

enum lvm_cache_flags {
  LVM_CACHE_DEFAULT,
  LVM_CACHE_WC,
  LVM_CACHE_DEVICE,
  LVM_CACHE_NONE
};

struct lvm_space {
  struct pmap p;
  lock_t lock;
  bool active;
};

void lvm_map_page(struct lvm_space* s, uintptr_t virt, uintptr_t phys, size_t size, int flags);
void lvm_unmap_page(struct lvm_space* s, uintptr_t virt, size_t size);
void lvm_space_load(struct lvm_space *s);

extern struct lvm_space kspace;

