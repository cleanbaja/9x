#include <lib/htab.h>
#include <lib/builtin.h>
#include <vm/vm.h>

// Use the excellent FNV (Fowler-Noll-Vo) Hash for its speed...
// See https://en.wikipedia.org/wiki/Fowler-Noll-Vo_hash_function
static uint64_t fnv_hash(char *data, size_t n_bytes) {
  uint64_t hash = 0xcbf29ce484222325; // FNV_offset_basis

  for(size_t i = 0; i < n_bytes; i++) {
    hash ^= *(data + i);
    hash *= 0x100000001b3; // FNV_prime
  }

  return hash;
}

void *htab_find(struct hash_table *htab, void *key, size_t key_size) {
  if (htab->capacity == 0)
    return NULL; // The hash table is empty!

  // Calculate the hash and index (from the key)
  uint64_t hash = fnv_hash(key, key_size);
  size_t index  = hash & (htab->capacity - 1);

  // Search the corresponding buckets for the value
  for(; index < htab->capacity; index++) {
    if(htab->keys[index] != NULL && memcmp(htab->keys[index], key, key_size) == 0) {
      return htab->data[index];
    }
  }

  return NULL;
}

void htab_insert(struct hash_table *htab, void *key, size_t key_size, void *data)  {
  // Create the hash table, if it was previously empty
  if(htab->capacity == 0) {
    htab->capacity = 16;

    htab->data = kmalloc(htab->capacity * sizeof(void*));
    htab->keys = kmalloc(htab->capacity * sizeof(void*));
  }

  // Perform a hash, then insert (if we have space, that is)
  uint64_t hash = fnv_hash(key, key_size);
  size_t index = hash & (htab->capacity - 1);

  // We clone the key on insert, so that if the same key pointer is used
  // twice (with different data), it doesn't return the same result!
  for(; index < htab->capacity; index++) {
    if(htab->keys[index] == NULL) {
      void* cloned_key = kmalloc(key_size);
      memcpy(cloned_key, key, key_size);
      htab->keys[index] = cloned_key;
      htab->data[index] = data;
      return;
    }
  }

  // Create (and copy to) a new hash table, with twice the space
  struct hash_table new_table = {
    .capacity = htab->capacity * 2,
    .data = kmalloc(htab->capacity * 2 * sizeof(void*)),
    .keys = kmalloc(htab->capacity * 2 * sizeof(void*))
  };

  for(size_t i = 0; i < htab->capacity; i++) {
    if(htab->keys[i] != NULL) {
      htab_insert(&new_table, htab->keys[i], key_size, htab->data[i]);
    }
  }

  // Destroy the cloned keys, before destroying the entire table
  for (size_t i = 0; i < htab->capacity; i++)
    if(htab->keys[i] != NULL)
      kfree(htab->keys[index]);
  kfree(htab->keys);
  kfree(htab->data);

  // Insert this element into the new table, and return the new table instead
  htab_insert(&new_table, key, key_size, data);
  *htab = new_table;
}

void htab_delete(struct hash_table *htab, void *key, size_t key_size) {
  if(htab->capacity == 0)
    return; // Empty table!

  // Find the hash and index...
  uint64_t hash = fnv_hash(key, key_size);
  size_t index = hash & (htab->capacity - 1);

  // Finally, elimnate the element, if its actually in there...
  for(; index < htab->capacity; index++) {
    if(htab->keys[index] != NULL && memcmp(htab->keys[index], key, key_size) == 0) {
      kfree(htab->keys[index]); // Free the cloned key
      htab->keys[index] = NULL;
      htab->data[index] = NULL;
      return;
    }
  }
}

