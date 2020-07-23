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

#define SPOOKY_HASH_DEFAULT_PRIME (1 << 11)

static const size_t SPOOKY_HASH_DEFAULT_ATOM_ALLOC = 64;
static const size_t SPOOKY_HASH_DEFAULT_STR_ALLOC = 4096;

typedef struct spooky_array_limits {
  size_t len;
  size_t capacity;
  size_t reallocs;
} spooky_array_limits;

/* pre-generated primes */
static const unsigned long primes[] = {
  #include "primes.dat"
};

static errno_t spooky_hash_ensure(const spooky_hash_table * self, const char * s, const spooky_str ** str);
static errno_t spooky_hash_find_by_id(const spooky_hash_table * self, int id, const char ** str);
static const char * spooky_hash_move_string_to_strings(const spooky_hash_table * self, const char * s);
static void spooky_hash_clear_strings(const spooky_hash_table * self);
static void spooky_hash_print_stats(const spooky_hash_table * self);

typedef struct spooky_hash_bucket {
  unsigned long prime;
  bool is_init;
  char padding[7]; /* not portable */
  
  spooky_array_limits strs_limits;
  spooky_str * strs;
} spooky_hash_bucket;

typedef struct spooky_hash_table_impl {
  unsigned long prime;

  spooky_array_limits buckets_limits;
  spooky_hash_bucket * buckets;

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

  impl->buckets_limits.capacity = impl->prime = primes[SPOOKY_HASH_DEFAULT_PRIME];
  impl->buckets_limits.len = 0;
  impl->buckets = calloc(impl->buckets_limits.capacity, sizeof * impl->buckets);

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

void spooky_hash_clear_buckets(const spooky_hash_table * self) {
  spooky_hash_table_impl * impl = self->impl;

  for(size_t i = 0; i < impl->prime; i++) {
    spooky_hash_bucket * bucket = &impl->buckets[i];
    if(bucket->is_init && bucket->strs) {
      free(bucket->strs), bucket->strs = NULL;
    }
  }

  impl->buckets_limits.len = impl->buckets_limits.capacity = 0;
  free(impl->buckets), impl->buckets = NULL;
}

/* Destruct (dtor) impl */
const spooky_hash_table * spooky_hash_table_dtor(const spooky_hash_table * self) {
  spooky_hash_clear_buckets(self);
  spooky_hash_clear_strings(self);
  
  free((spooky_hash_table *)(uintptr_t)self->impl), ((spooky_hash_table *)(uintptr_t)self)->impl = NULL;
  return self;
}

void spooky_hash_table_free(const spooky_hash_table * self) {
  free((void *)(uintptr_t)self), self = NULL;
}

void spooky_hash_table_release(const spooky_hash_table * self) {
  self->free(self->dtor(self));
}

errno_t spooky_hash_ensure(const spooky_hash_table * self, const char * s, const spooky_str ** out_str) {
  if(!s) { return SP_FAILURE; }

  spooky_hash_table_impl * impl = self->impl;

  register unsigned long h = spooky_hash_str(s);
  register unsigned long index = h % impl->prime;

  assert(index < impl->prime);
  spooky_hash_bucket * bucket = &impl->buckets[index];

  if(!bucket->is_init) {
    /* bucket hasn't been initialized: */
    bucket->prime = primes[index];
    bucket->strs_limits.len = 0;
    bucket->strs_limits.capacity = SPOOKY_HASH_DEFAULT_ATOM_ALLOC;
    bucket->strs = calloc(bucket->strs_limits.capacity, sizeof * bucket->strs);
    if(!bucket->strs) { goto err0; }

    const char * s_cp = spooky_hash_move_string_to_strings(self, s);
    spooky_str * temp_str = &bucket->strs[bucket->strs_limits.len];
    spooky_str_ref(s_cp, strnlen(s, SPOOKY_MAX_STRING_LEN), temp_str);
    spooky_str_inc_ref_count(temp_str);
 
    bucket->strs_limits.len++;
    if(out_str) { *out_str = temp_str; }

    impl->buckets_limits.len++;
    bucket->is_init = true;

    return SP_SUCCESS;
  }
 
  /* check if it already exists */
  for(size_t i = 0; i < bucket->strs_limits.len; i++) {
    const spooky_str * str = &(bucket->strs[i]);
    const char * str_str = spooky_str_get_str(str);
    if(str_str == s) {
      /* pointers are equal; return it: */
      spooky_str_inc_ref_count((spooky_str *)(uintptr_t)str);
      if(out_str) { *out_str = str; }
      return SP_SUCCESS;
    } else if(strncmp(str_str, s, SPOOKY_MAX_STRING_LEN) == 0) {
      /* strings are equal; return it: */
      spooky_str_inc_ref_count((spooky_str *)(uintptr_t)str);
      if(out_str) { *out_str = str; }
      return SP_SUCCESS;
    }
  }
 
  /* bucket exists but str wasn't found, above; allocate string and stuff it into a bucket */

  if(bucket->strs_limits.len + 1 > bucket->strs_limits.capacity) {
    bucket->strs_limits.capacity *= 2;
    spooky_str * temp_strs = realloc(bucket->strs, (sizeof * temp_strs) * bucket->strs_limits.capacity);
    if(!temp_strs) { goto err0; }
    
    bucket->strs = temp_strs;
    bucket->strs_limits.reallocs++;
  }

  const char * s_cp = spooky_hash_move_string_to_strings(self, s);

  spooky_str * temp_str = &bucket->strs[bucket->strs_limits.len];
  spooky_str_ref(s_cp, strnlen(s_cp, SPOOKY_MAX_STRING_LEN), temp_str);
  spooky_str_inc_ref_count(temp_str);

  bucket->strs_limits.len++;

  if(out_str) { *out_str = temp_str; }

  return SP_SUCCESS;

err0:
  abort();
}

void spooky_hash_print_stats(const spooky_hash_table * self) {
  spooky_hash_table_impl * impl = self->impl;
  fprintf(stdout, "Prime: %lu, buckets: (%lu, %lu) strings: (%lu, %lu)\n", impl->prime, (unsigned long)impl->buckets_limits.capacity, (unsigned long)impl->buckets_limits.len, (unsigned long)impl->strings_limits.capacity, (unsigned long)impl->strings_limits.len);
  fprintf(stdout, "Indices (%lu, %lu):\n", impl->buckets_limits.capacity, impl->buckets_limits.len);

#if 1 == 0
  fprintf(stdout, "Strings:\n");
  for(size_t i = 0; i < impl->strings_limits.len; i++) {
    fprintf(stdout, "\t'%s'\n", impl->strings[i]);
  }
  for(size_t i = 0; i < impl->prime; i++) {
    if(impl->buckets[i].is_init) {
      fprintf(stdout, "\t %i %lu, strs: (%lu, %lu, [%lu])\n", impl->buckets[i].is_init, impl->buckets[i].prime, impl->buckets[i].strs_limits.capacity, impl->buckets[i].strs_limits.len, impl->buckets[i].strs_limits.reallocs);
    }
  }
  #endif
}

errno_t spooky_hash_find_by_id(const spooky_hash_table * self, int id, const char ** str) {
  (void)self;
  (void)id;
  (void)str;
  return SP_SUCCESS;
}

const char * spooky_hash_move_string_to_strings(const spooky_hash_table * self, const char * s) {
  spooky_hash_table_impl * impl = self->impl;

  if(!s) { return NULL; }

  if(impl->strings_limits.len + 1 > impl->strings_limits.capacity) {
    /* reallocate strings */ 
    impl->strings_limits.capacity *= 2;
		const char ** temp = realloc(impl->strings, sizeof * impl->strings * impl->strings_limits.capacity);
		if(!temp) { goto err0; }
    
    impl->strings_limits.reallocs++;
    impl->strings = temp;
  }

  char * temp = strndup(s, SPOOKY_MAX_STRING_LEN);
  if(!temp) { goto err0; }
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


