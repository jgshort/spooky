#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "sp_limits.h"
#include "sp_error.h"
#include "sp_hash.h"

#define SPOOKY_HASH_DEFAULT_ATOM_ALLOC 128
#define SPOOKY_HASH_DEFAULT_STR_ALLOC 1024

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
static const char * spooky_hash_move_string_to_strings(const spooky_hash_table * self, const char * str);
static void spooky_hash_clear_strings(const spooky_hash_table * self);
static void spooky_hash_print_stats(const spooky_hash_table * self);
typedef struct spooky_hash_bucket_item spooky_hash_bucket_item;

typedef struct spooky_hash_bucket_index {
  unsigned long prime;
  bool is_init;
  char padding[7]; /* not portable */

  spooky_array_limits atoms_limits;
  const spooky_atom * atoms;
} spooky_hash_bucket_index;

typedef struct spooky_hash_table_impl {
  unsigned long prime;

  spooky_array_limits indices_limits;
  spooky_hash_bucket_index * indices;

  spooky_array_limits strings_limits;
  const char ** strings;
} spooky_hash_table_impl;

const spooky_hash_table * spooky_hash_table_alloc() {
  spooky_hash_table * self = calloc(1, sizeof * self);
  if(!self) goto err0;

  return self;

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
  self->print_stats = &spooky_hash_print_stats;
  return self;
}

const spooky_hash_table * spooky_hash_table_acquire() {
  return spooky_hash_table_init((spooky_hash_table *)(uintptr_t)spooky_hash_table_alloc());
}

const spooky_hash_table * spooky_hash_table_ctor(const spooky_hash_table * self) {
  spooky_hash_table_impl * impl = calloc(1, sizeof * self->impl); 
  if(!impl) goto err0;

  impl->indices_limits.capacity = impl->prime = primes[1 << 12];
  impl->indices_limits.len = 0;
  impl->indices = calloc(impl->indices_limits.capacity, sizeof * impl->indices);

  impl->strings_limits.capacity = SPOOKY_HASH_DEFAULT_STR_ALLOC;
  impl->strings_limits.len = 0;
  impl->strings = calloc(impl->strings_limits.capacity, sizeof * impl->strings);

  ((spooky_hash_table *)(uintptr_t)self)->impl = impl;
 
  return self;

err0:
  abort();
}

void spooky_hash_clear_strings(const spooky_hash_table * self) {
  spooky_hash_table_impl * impl = self->impl;
  const char ** next = impl->strings;
  const char ** end = impl->strings + impl->strings_limits.len;
  while(next != end) {
    char * temp = (char *)(uintptr_t)(*next);
    free(temp), temp = NULL;
    next++;
  }
  impl->strings_limits.len = impl->strings_limits.capacity = 0;
  free(impl->strings), impl->strings = NULL;
}

void spooky_hash_clear_indices(const spooky_hash_table * self) {
  spooky_hash_table_impl * impl = self->impl;


  /* TODO: */
  /*
  spooky_hash_bucket_index * next = &impl->indices[0];
  spooky_hash_bucket_index * end = &impl->indices[impl->indices_limits.len];

  while(next != end) {
    spooky_hash_bucket_index * bucket = next;
    free(bucket), bucket = NULL;
    next++;
  }
  */
  free(impl->indices), impl->indices = NULL;
  impl->indices_limits.len = impl->indices_limits.capacity = 0;
}

/* Destruct (dtor) impl */
const spooky_hash_table * spooky_hash_table_dtor(const spooky_hash_table * self) {
  spooky_hash_clear_indices(self);
  spooky_hash_clear_strings(self);
  
  /* TODO: Clear the buckets */

  free((void *)(uintptr_t)self->impl), ((spooky_hash_table *)(uintptr_t)self)->impl = NULL;
  return self;
}

void spooky_hash_table_free(const spooky_hash_table * self) {
  free((void *)(uintptr_t)self), self = NULL;
}

void spooky_hash_table_release(const spooky_hash_table * self) {
  self->free(self->dtor(self));
}

errno_t spooky_hash_ensure(const spooky_hash_table * self, const char * str, const spooky_atom ** atom, unsigned long * hash, unsigned long * bucket_index) {
  if(!str) { return SP_FAILURE; }

  spooky_hash_table_impl * impl = self->impl;

  register unsigned long h = spooky_hash_str(str);
  register unsigned long index = h % impl->prime;

  assert(index >= 0 && index < impl->prime);
  spooky_hash_bucket_index * bucket = &impl->indices[index];

  if(!bucket->is_init) {
    /* bucket hasn't been initialized: */
    bucket->prime = primes[index];
    bucket->atoms_limits.len = 0;
    bucket->atoms_limits.capacity = SPOOKY_HASH_DEFAULT_ATOM_ALLOC;
    bucket->atoms = calloc(bucket->atoms_limits.capacity, sizeof * bucket->atoms);
    if(!bucket->atoms) { goto err0; }

    const char * str_copy = spooky_hash_move_string_to_strings(self, str);

    const spooky_atom * temp_atom = spooky_atom_init((spooky_atom *)(uintptr_t)(&bucket->atoms[0]));
    temp_atom = temp_atom->ctor(temp_atom, str_copy);
    temp_atom->inc_ref_count(temp_atom);

    bucket->atoms_limits.len++;

    *hash = h;
    *bucket_index = index;

    if(atom) { *atom = temp_atom; }

    impl->indices_limits.len++;
    bucket->is_init = true;

    return SP_SUCCESS;
  }

  *hash = h;
  *bucket_index = index;
  
  /* check if it already exists */

  for(size_t i = 0; i < bucket->atoms_limits.len; i++) {
    const spooky_atom * next_atom = &bucket->atoms[i];
    const spooky_str * atom_str = next_atom->get_str(next_atom);
    const char * atom_str_str = atom_str->str;
    if(atom_str_str == str) {
      /* pointers are equal; return it: */
      *atom = next_atom;
      next_atom->inc_ref_count(next_atom);
      return SP_SUCCESS;
    } else if(strncmp(atom_str_str, str, SPOOKY_MAX_STRING_LEN) == 0) {
      /* strings are equal; return it: */
      *atom = next_atom;
      next_atom->inc_ref_count(next_atom);
      return SP_SUCCESS;
    }
  }

  /* bucket exists but str wasn't found, above; allocate string and stuff it into a bucket */
  const char * temp_str = spooky_hash_move_string_to_strings(self, str);

  if(bucket->atoms_limits.len + 1 > bucket->atoms_limits.capacity) {
    bucket->atoms_limits.capacity += SPOOKY_HASH_DEFAULT_ATOM_ALLOC;
    const spooky_atom * temp_atoms = realloc((spooky_atom *)(uintptr_t)bucket->atoms, sizeof * bucket->atoms * bucket->atoms_limits.capacity);
    if(!temp_atoms) { goto err0; }
    bucket->atoms = temp_atoms;
  }

  const spooky_atom * temp_atom = &bucket->atoms[bucket->atoms_limits.len];
  temp_atom = spooky_atom_init((spooky_atom *)(uintptr_t)temp_atom);
  temp_atom = temp_atom->ctor(temp_atom, temp_str);
  temp_atom->inc_ref_count(temp_atom);
  bucket->atoms_limits.len++;

  if(!temp_str) {
    *hash = 0;
    *bucket_index = 0;
  }

  *atom = temp_atom;

  return SP_SUCCESS;

err0:
  abort();
}

void spooky_hash_print_stats(const spooky_hash_table * self) {
  spooky_hash_table_impl * impl = self->impl;
  fprintf(stdout, "Prime: %lu, indices: (%lu, %lu) strings: (%lu, %lu)\n", impl->prime, (unsigned long)impl->indices_limits.capacity, (unsigned long)impl->indices_limits.len, (unsigned long)impl->strings_limits.capacity, (unsigned long)impl->strings_limits.len);

  fprintf(stdout, "Strings:\n");
  for(size_t i = 0; i < impl->strings_limits.len; i++) {
    fprintf(stdout, "\t'%s'\n", impl->strings[i]);
  }
  
  fprintf(stdout, "Indices (%lu, %lu):\n", impl->indices_limits.capacity, impl->indices_limits.len);
  for(size_t i = 0; i < impl->prime; i++) {
    if(impl->indices[i].is_init) {
      fprintf(stdout, "\t %i %lu, atoms: (%lu, %lu)\n", impl->indices[i].is_init, impl->indices[i].prime, impl->indices[i].atoms_limits.capacity, impl->indices[i].atoms_limits.len);
    }
  }
}

errno_t spooky_hash_get_atom(const spooky_hash_table * self, const char * str, spooky_atom ** atom) {
  (void)self;
  (void)str;
  (void)atom;
  return SP_SUCCESS;
}

errno_t spooky_hash_find_by_id(const spooky_hash_table * self, int id, const char ** str) {
  (void)self;
  (void)id;
  (void)str;
  return SP_SUCCESS;
}

const char * spooky_hash_move_string_to_strings(const spooky_hash_table * self, const char * str) {
  spooky_hash_table_impl * impl = self->impl;

  if(!str) { return NULL; }

  if(impl->strings_limits.len + 1 > impl->strings_limits.capacity) {
    /* reallocate strings */ 
    impl->strings_limits.capacity += SPOOKY_HASH_DEFAULT_STR_ALLOC;
		const char ** temp = realloc(impl->strings, sizeof * impl->strings * impl->strings_limits.capacity);
		if(!temp) { goto err0; }

    impl->strings = temp;
  }

  size_t len = strnlen(str, SPOOKY_MAX_STRING_LEN); /* not inc null-terminating char */

  char * temp = calloc(1, len + 1);
  if(!temp) { goto err0; }

  memmove(temp, str, len);
  temp[len] = '\0';

  impl->strings[impl->strings_limits.len] = temp;
  impl->strings_limits.len++;

  return temp;

err0:
  abort();
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


