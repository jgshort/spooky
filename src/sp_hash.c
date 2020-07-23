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

/* pre-generated primes */
static const unsigned long primes[] = {
  #include "primes.dat"
};

static const size_t SPOOKY_HASH_DEFAULT_ATOM_ALLOC = 64;
static const size_t SPOOKY_HASH_DEFAULT_STRING_ALLOC = 4096;

typedef struct spooky_array_limits {
  size_t len;
  size_t capacity;
  size_t reallocs;
} spooky_array_limits;

typedef struct spooky_hash_bucket {
  unsigned long prime;
  spooky_array_limits atoms_limits;
  spooky_str * atoms;
} spooky_hash_bucket;

typedef struct spooky_hash_table_impl {
  unsigned long prime;

  size_t atoms_alloc;
  size_t strings_alloc;

  spooky_array_limits buckets_limits;
  spooky_hash_bucket * buckets;

  spooky_array_limits strings_limits;
  const char ** strings;
} spooky_hash_table_impl;

static errno_t spooky_hash_ensure(const spooky_hash_table * self, const char * s, size_t s_len, const spooky_str ** out_str);
static errno_t spooky_hash_find_internal(const spooky_hash_bucket * bucket, const char * s, size_t s_len, unsigned long hash, const spooky_str ** atom);
static errno_t spooky_hash_find(const spooky_hash_table * self, const char * s, size_t s_len, const spooky_str ** atom);
static const char * spooky_hash_move_string_to_strings(const spooky_hash_table * self, const char * s, size_t s_len, size_t * out_len);
static void spooky_hash_clear_strings(const spooky_hash_table * self);
static void spooky_hash_print_stats(const spooky_hash_table * self);

static const spooky_str * spooky_hash_atom_alloc(const spooky_hash_table * self, spooky_hash_bucket * bucket, const char * s, size_t s_len, size_t * out_len);

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
  self->find = &spooky_hash_find;
  self->print_stats = &spooky_hash_print_stats;
  return self;
}

const spooky_hash_table * spooky_hash_table_acquire() {
  return spooky_hash_table_init((spooky_hash_table *)(uintptr_t)spooky_hash_table_alloc());
}

const spooky_hash_table * spooky_hash_table_ctor(const spooky_hash_table * self) {
  spooky_hash_table_impl * impl = calloc(1, sizeof * self->impl); 
  if(!impl) goto err0;

  impl->atoms_alloc = SPOOKY_HASH_DEFAULT_ATOM_ALLOC;
  impl->strings_alloc = SPOOKY_HASH_DEFAULT_STRING_ALLOC;

  impl->buckets_limits.capacity = impl->prime = primes[SPOOKY_HASH_DEFAULT_PRIME];
  impl->buckets_limits.len = 0;
  impl->buckets = calloc(impl->buckets_limits.capacity, sizeof * impl->buckets);

  impl->strings_limits.capacity = impl->strings_alloc;
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
    if(bucket->prime && bucket->atoms) {
      free(bucket->atoms), bucket->atoms = NULL;
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

errno_t spooky_hash_ensure(const spooky_hash_table * self, const char * s, size_t s_len, const spooky_str ** out_str) {
  if(!s) { return SP_FAILURE; }
  if(s_len <= 0) { return SP_FAILURE; }

  assert(s_len <= SPOOKY_MAX_STRING_LEN);

  spooky_hash_table_impl * impl = self->impl;

  register unsigned long hash = spooky_hash_str(s);
  register unsigned long index = hash % impl->prime;

  assert(index < impl->prime);
  spooky_hash_bucket * bucket = &impl->buckets[index];

  if(!bucket->prime) {
    /* bucket hasn't been initialized: */
    bucket->prime = primes[index];
    bucket->atoms_limits.len = 0;
    bucket->atoms_limits.capacity = impl->atoms_alloc;
    bucket->atoms = calloc(bucket->atoms_limits.capacity, sizeof * bucket->atoms);
    if(!bucket->atoms) { goto err0; }
   
    impl->buckets_limits.len++;
  } else {
    /* check if it already exists */
    const spooky_str * found = NULL;
    if(spooky_hash_find_internal(bucket, s, s_len, hash, &found) == SP_SUCCESS) {
      spooky_str_inc_ref_count((spooky_str *)(uintptr_t)found);
      if(out_str) { *out_str = found; }
      return SP_SUCCESS; 
    }
  
    /* bucket exists but str wasn't found, above; allocate string and stuff it into a bucket */
    if(bucket->atoms_limits.len + 1 > bucket->atoms_limits.capacity) {
      bucket->atoms_limits.capacity *= 2;
      spooky_str * temp_atoms = realloc(bucket->atoms, (sizeof * temp_atoms) * bucket->atoms_limits.capacity);
      if(!temp_atoms) { goto err0; }
      
      bucket->atoms = temp_atoms;
      bucket->atoms_limits.reallocs++;
    }
  }

  assert(bucket && bucket->prime);

  size_t out_len = 0;
  const spooky_str * temp_str = spooky_hash_atom_alloc(self, bucket, s, s_len, &out_len);
  assert(out_len == temp_str->len);

  bucket->atoms_limits.len++;
  if(out_str) { *out_str = temp_str; }

  return SP_SUCCESS;

err0:
  abort();
}

static const spooky_str * spooky_hash_atom_alloc(const spooky_hash_table * self, spooky_hash_bucket * bucket, const char * s, size_t s_len, size_t * out_len) {
  assert(self && bucket && bucket->prime);

  const char * s_cp = spooky_hash_move_string_to_strings(self, s, s_len, out_len);
  spooky_str * atom = &bucket->atoms[bucket->atoms_limits.len];
  memset(atom, 0, sizeof * atom);
  spooky_str_ref(s_cp, strnlen(s_cp, SPOOKY_MAX_STRING_LEN), bucket->atoms_limits.len, atom);
  spooky_str_inc_ref_count(atom);

  return atom;
}

void spooky_hash_print_stats(const spooky_hash_table * self) {
  spooky_hash_table_impl * impl = self->impl;
  fprintf(stdout, "Prime: %lu, buckets: (%lu, %lu) strings: (%lu, %lu)\n", impl->prime, (unsigned long)impl->buckets_limits.capacity, (unsigned long)impl->buckets_limits.len, (unsigned long)impl->strings_limits.capacity, (unsigned long)impl->strings_limits.len);
  fprintf(stdout, "Indices (%lu, %lu):\n", impl->buckets_limits.capacity, impl->buckets_limits.len);

  fprintf(stdout, "Strings:\n");
  for(size_t i = 0; i < impl->strings_limits.len; i++) {
    fprintf(stdout, "'%s'", impl->strings[i]);
    if(i > 256) { fprintf(stdout, "...\n"); break; }
    if(i < impl->strings_limits.len - 1) { fprintf(stdout, ","); }
    if(i == impl->strings_limits.len - 1) { fprintf(stdout, "\n"); }
  }

  fprintf(stdout, "Buckets:\n");
  int collisions = 0;
  for(size_t i = 0; i < impl->prime; i++) {
    const spooky_hash_bucket * bucket = &impl->buckets[i];
    if(bucket->prime) {
      // Print all keys: fprintf(stdout, "  %i %lu, atoms: (%lu, %lu, [%lu])\n", impl->buckets[i].is_init, impl->buckets[i].prime, impl->buckets[i].atoms_limits.capacity, impl->buckets[i].atoms_limits.len, impl->buckets[i].atoms_limits.reallocs);
      if(bucket->atoms_limits.len > 1) {
        for(size_t x = 0; x < bucket->atoms_limits.len; x++) {
          const spooky_str * outer = &bucket->atoms[x];
          for(size_t y = 0; y < bucket->atoms_limits.len; y++) {
            const spooky_str * inner = &bucket->atoms[y];
            if(outer != inner && outer->hash == inner->hash) {
              collisions++;
              fprintf(stdout, "Hash Collisions!\n");
              fprintf(stdout, "    X: '%s': len: %lu, hash: %lu, ordinal: %lu\n", outer->str, outer->len, outer->hash, outer->ordinal);
              fprintf(stdout, "    Y: '%s': len: %lu, hash: %lu, ordinal: %lu\n", inner->str, inner->len, inner->hash, inner->ordinal);
            }
          }
        }
      }
    }
  }
  fprintf(stdout, "Total Collisions: %i\n", collisions / 2);
}

errno_t spooky_hash_find_internal(const spooky_hash_bucket * bucket, const char * s, size_t s_len, unsigned long hash, const spooky_str ** out_atom) {
  if(!bucket->prime) { return SP_FAILURE; }

  const spooky_str * atom = bucket->atoms;
  const spooky_str * end = bucket->atoms + bucket->atoms_limits.len;
  do {
    if(s_len == atom->len && hash == atom->hash) {
      const char * str_str = atom->str;
      bool found = 
           str_str == s
        || strncmp(str_str, s, SPOOKY_MAX_STRING_LEN) == 0;

      if(found) {
        if(out_atom) { *out_atom = atom; }
        return SP_SUCCESS;
      }
    }
  } while(++atom < end);

  return SP_FAILURE;
}

errno_t spooky_hash_find(const spooky_hash_table * self, const char * s, size_t s_len, const spooky_str ** atom) {
  spooky_hash_table_impl * impl = self->impl;
  register unsigned long hash = spooky_hash_str(s);
  register unsigned long index = hash % impl->prime;
  assert(index < impl->prime);
  spooky_hash_bucket * bucket = &impl->buckets[index];
  return spooky_hash_find_internal(bucket, s, s_len, hash, atom); 
}

const char * spooky_hash_move_string_to_strings(const spooky_hash_table * self, const char * s, size_t s_len, size_t * out_len) {
  spooky_hash_table_impl * impl = self->impl;

  if(!s) { return NULL; }

  assert(s_len > 0);
  if(s_len <= 0) { return NULL; }

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

  *out_len = strnlen(temp, SPOOKY_MAX_STRING_LEN); 
  assert(*out_len == s_len);
  if(*out_len != s_len) { abort(); }

  impl->strings[impl->strings_limits.len] = temp;
  impl->strings_limits.len++;

  return temp;

err0:
  abort();
}

