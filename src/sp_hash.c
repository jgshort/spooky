#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "sp_limits.h"
#include "sp_error.h"
#include "sp_hash.h"

typedef struct spooky_array_limits {
  size_t len;
  size_t capacity;
} spooky_array_limits;

/* pre-generated primes */
static const unsigned long primes[] = {
  #include "primes.dat"
};

static errno_t spooky_hash_ensure(const spooky_hash_table * self, const char * str, const spooky_atom ** atom, unsigned long * hash, unsigned long * bucket_index);
static errno_t spooky_hash_get_atom(const spooky_hash_table * self, const char * str, spooky_atom ** atom);
static errno_t spooky_hash_find_by_id(const spooky_hash_table * self, int id, const char ** str);
static char * spooky_hash_move_string_to_strings(const spooky_hash_table * self, const char * str);
static void spooky_hash_clear_strings(const spooky_hash_table * self);

typedef struct spooky_hash_bucket_item spooky_hash_bucket_item;

typedef struct spooky_hash_bucket_item {
  const spooky_atom * atom; 
  spooky_hash_bucket_item * next;
  spooky_hash_bucket_item * prev;
} spooky_hash_bucket_item;

typedef struct spooky_hash_bucket_index {
  unsigned long prime;
  spooky_array_limits items_limits;
  spooky_hash_bucket_item * items;
} spooky_hash_bucket_index;

typedef struct spooky_hash_table_impl {
  unsigned long prime;

  spooky_array_limits indices_limits;
  spooky_hash_bucket_index ** indices;

  spooky_array_limits strings_limits;
  char ** strings;

  spooky_array_limits ids_limits;
  int * ids;
} spooky_hash_table_impl;

static spooky_hash_table * SP_UNCONST(const spooky_hash_table * self) {
  return (spooky_hash_table *)(uintptr_t)self;
}

static spooky_hash_table_impl * SP_DCAST(const spooky_hash_table * self) {
  return (spooky_hash_table_impl *)(uintptr_t)((spooky_hash_table *)(uintptr_t)(self))->impl;
}

const spooky_hash_table * spooky_hash_table_alloc() {
  spooky_hash_table * table = calloc(1, sizeof * table);
  if(!table) goto err0;

  return table;

err0:
  abort();
}

const spooky_hash_table * spooky_hash_table_init(spooky_hash_table * self) {
	self->ctor = &spooky_hash_table_ctor;
  self->dtor = &spooky_hash_table_dtor;
  self->free = &spooky_hash_table_free;
  self->release = &spooky_hash_table_release;
  self->ensure = &spooky_hash_ensure;
  self->get_atom = &spooky_hash_get_atom;
  self->find_by_id = &spooky_hash_find_by_id;
  return self;
}

const spooky_hash_table * spooky_hash_table_acquire() {
  return spooky_hash_table_init(SP_UNCONST(spooky_hash_table_alloc()));
}

const spooky_hash_table * spooky_hash_table_ctor(const spooky_hash_table * self) {
  spooky_hash_table_impl * impl = calloc(1, sizeof * self->impl); 
  if(!impl) goto err0;

  memset(impl, 0, sizeof * impl);

  impl->indices_limits.capacity = impl->prime = primes[1 << 10];
  impl->indices_limits.len = 0;
  impl->indices = calloc(sizeof * impl->indices, impl->indices_limits.capacity);

  impl->strings_limits.capacity = 1 << 10;
  impl->strings = calloc(sizeof * impl->strings, impl->strings_limits.capacity);
  impl->strings_limits.len = 0;

  impl->ids_limits.capacity = 1 << 10;
  impl->ids = calloc(sizeof * impl->ids, impl->ids_limits.capacity);
  impl->ids_limits.len = 0;

  SP_UNCONST(self)->impl = impl;
 
  return self;

err0:
  abort();
}

void spooky_hash_clear_strings(const spooky_hash_table * self) {
  spooky_hash_table_impl * impl = SP_DCAST(self);
  char ** next = impl->strings;
  char ** end = impl->strings + impl->strings_limits.len;
  while(next != end) {
    char * temp = *next;
    free(temp), temp = NULL;
    next++;
  }
  impl->strings_limits.len = impl->strings_limits.capacity = 0;
  free(impl->strings), impl->strings = NULL;
}

void spooky_hash_clear_indices(const spooky_hash_table * self) {
  spooky_hash_table_impl * impl = SP_DCAST(self);

  spooky_hash_bucket_index * next = impl->indices[0];
  spooky_hash_bucket_index * end = impl->indices[impl->indices_limits.len];

  while(next != end) {
    spooky_hash_bucket_index * bucket = next;
    free(bucket), bucket = NULL;
    next++;
  }

  free(impl->indices), impl->indices = NULL;
  impl->indices_limits.len = impl->indices_limits.capacity = 0;
}

/* Destruct (dtor) impl */
const spooky_hash_table * spooky_hash_table_dtor(const spooky_hash_table * self) {
  if(self) {
    spooky_hash_table_impl * impl = SP_DCAST(self);
    if(impl) {
      spooky_hash_clear_indices(self);
      spooky_hash_clear_strings(self);
    }
  }
  return self;
}

void spooky_hash_table_free(const spooky_hash_table * self) {
  if(self) {
    free((void *)(uintptr_t)self->impl), SP_UNCONST(self)->impl = NULL;
    free((void *)(uintptr_t)self), self = NULL;
  }
}

void spooky_hash_table_release(const spooky_hash_table * self) {
  self->free(self->dtor(self));
}

errno_t spooky_hash_ensure(const spooky_hash_table * self, const char * str, const spooky_atom ** atom, unsigned long * hash, unsigned long * bucket_index) {
  if(!str) { return SP_FAILURE; }

  spooky_hash_table_impl * impl = SP_DCAST(self);

  register unsigned long h = spooky_hash_str(str);
  register unsigned long index = h % impl->prime;

  spooky_hash_bucket_index * bucket = impl->indices[index];

  if(!bucket) {
    bucket = calloc(1, sizeof * bucket);
    bucket->prime = primes[index];
    bucket->items_limits.len = 0;
    bucket->items_limits.capacity = 1 << 10;
    bucket->items = calloc(sizeof * bucket->items, bucket->items_limits.capacity);

    char * temp = spooky_hash_move_string_to_strings(self, str);

    const spooky_atom * temp_atom = spooky_atom_acquire();
    temp_atom = temp_atom->ctor(temp_atom, temp);
    
    spooky_hash_bucket_item * item = &bucket->items[0];
    item->next = item;
    item->prev = item;
    item->atom = temp_atom;

    *hash = h;
    *bucket_index = index;
    if(atom) { *atom = item->atom; }

    return SP_SUCCESS;
  }

  char * temp = spooky_hash_move_string_to_strings(self, str);

  const spooky_atom * temp_atom = spooky_atom_acquire();
  temp_atom = spooky_atom_ctor(temp_atom, temp);

  if(!temp) {
    *hash = 0;
    *bucket_index = 0;
  } else {
    *hash = h;
    *bucket_index = index;
  }
  *atom = temp_atom;

  return SP_SUCCESS;
}

errno_t spooky_hash_get_atom(const spooky_hash_table * self, const char * str, spooky_atom ** atom) {
  (void)self;
  (void)str;
  (void)atom;
  return 0;
}

errno_t spooky_hash_find_by_id(const spooky_hash_table * self, int id, const char ** str) {
  (void)self;
  (void)id;
  (void)str;
  return 0;
}

char * spooky_hash_move_string_to_strings(const spooky_hash_table * self, const char * str) {
  spooky_hash_table_impl * impl = SP_DCAST(self);

  if(!str) { return NULL; }

  if(impl->strings_limits.len + 1 > impl->strings_limits.capacity) {
    /* reallocate strings */ 
    impl->strings_limits.capacity += (size_t)(1 << 10);
		char ** temp = realloc(impl->strings, sizeof * impl->strings * impl->strings_limits.capacity);
		if(!temp) { 
			abort();
		}
    impl->strings = temp;
  }

  size_t len = strnlen(str, SPOOKY_MAX_STRING_LEN); /* not inc null-terminating char */

  char * temp = calloc(1, len + 1);
  if(!temp) abort();

  memcpy(temp, str, len);
  temp[len] = '\0';

  impl->strings[impl->strings_limits.len] = temp;

  return temp;
}


/*
static int spooky_hash_sort_ids(const void * left, const void * right) {
   return *(const int*)left - *(const int*)right;
}

static int spooky_hash_get_next_id(const spooky_hash_table * self) {
  spooky_hash_table_impl * impl = SP_DCAST(self);

  if(impl->ids_limits.len + 1 > impl->ids_limits.capacity) {
    impl->ids_limits.capacity += (size_t)(1 << 10);
    impl->ids = realloc(impl->ids, sizeof * impl->ids * impl->ids_limits.capacity);
  }

  int * ids = impl->ids;
  int next_id = ids[impl->ids_limits.len] + 1;

  size_t offset = 0;
  for(size_t i = 0; i < impl->ids_limits.len; i++) {
    if(id < ids[i]) {
       offset = i;
       break;
    }
  }

  for(size_t i = impl->ids_limits.len; i >= offset; i--)
    ids[i] = ids[i - 1];
   
  ids[offset] = next_id;

  qsort(ids, impl->ids_limits.len, sizeof * impl->ids, spooky_hash_sort_ids);

  ids[impl->ids_limits.len] = next_id;
  impl->ids_limits.len++;

  return next_id;
}
*/


