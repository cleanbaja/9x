#ifndef LIB_HTAB_H
#define LIB_HTAB_H

#include <stddef.h>

struct hash_table {
  void **keys;
  void **data;

  int capacity;
};

void *htab_find(struct hash_table *htab, void *key, size_t key_size);
void htab_insert(struct hash_table *htab, void *key, size_t key_size, void *data);
void htab_delete(struct hash_table *htab, void *key, size_t key_size);

#endif // LIB_HTAB_H
