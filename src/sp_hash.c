#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <stddef.h>

#include "sp_limits.h"
#include "sp_error.h"
#include "sp_hash.h"

#define SPOOKY_HASH_DEFAULT_PRIME (1 << 10)

/* pre-generated primes */
static const unsigned long primes[] = {
  #include "primes.dat"
};

static errno_t spooky_generate_primes(unsigned int limit);
static const size_t SPOOKY_HASH_DEFAULT_ATOM_ALLOC = 48;
static const size_t SPOOKY_HASH_DEFAULT_STRING_ALLOC = 1048576;

typedef struct spooky_string_buffer spooky_string_buffer;
typedef struct spooky_string_buffer {
  size_t capacity;
  size_t len;
  size_t string_count;
  char * strings;
  spooky_string_buffer * next;
} spooky_string_buffer;

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

  size_t string_count;
  spooky_string_buffer * buffers;
  spooky_string_buffer * current_buffer;
} spooky_hash_table_impl;

static errno_t spooky_hash_ensure(const spooky_hash_table * self, const char * s, size_t s_len, const spooky_str ** out_str);
static errno_t spooky_hash_find_internal(const spooky_hash_bucket * bucket, const char * s, size_t s_len, unsigned long hash, const spooky_str ** atom);
static errno_t spooky_hash_find(const spooky_hash_table * self, const char * s, size_t s_len, const spooky_str ** atom);
static const char * spooky_hash_move_string_to_strings(const spooky_hash_table * self, const char * s, size_t s_len, size_t * out_len);
static void spooky_hash_clear_strings(const spooky_hash_table * self);
static char * spooky_hash_print_stats(const spooky_hash_table * self);

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
  
  (void)spooky_generate_primes;

  impl->atoms_alloc = SPOOKY_HASH_DEFAULT_ATOM_ALLOC;
  impl->strings_alloc = SPOOKY_HASH_DEFAULT_STRING_ALLOC;

  impl->buckets_limits.capacity = impl->prime = primes[SPOOKY_HASH_DEFAULT_PRIME];
  impl->buckets_limits.len = 0;
  impl->buckets = calloc(impl->buckets_limits.capacity, sizeof * impl->buckets);

  impl->buffers = calloc(1, sizeof * impl->buffers);
  impl->buffers->next = NULL;
  impl->buffers->len = 0;
  impl->buffers->capacity = SPOOKY_HASH_DEFAULT_STRING_ALLOC;
  impl->buffers->strings = calloc(impl->buffers->capacity , sizeof * impl->buffers->strings);
  impl->current_buffer = impl->buffers;
  ((spooky_hash_table *)(uintptr_t)self)->impl = impl;
 
  return self;

err0:
  abort();
}

void spooky_hash_clear_strings(const spooky_hash_table * self) {
  spooky_hash_table_impl * impl = self->impl;

  spooky_string_buffer * buffer = impl->buffers;
  while(buffer) {
    free(buffer->strings), buffer->strings = NULL;
    spooky_string_buffer * old = buffer;
    buffer = buffer->next;
    free(old), old = NULL;
  }
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

  register unsigned long hash = spooky_hash_str(s, s_len);
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
      if(found) { spooky_str_inc_ref_count((spooky_str *)(uintptr_t)found); }
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

  spooky_str * atom = &bucket->atoms[bucket->atoms_limits.len];
  const char * s_cp = spooky_hash_move_string_to_strings(self, s, s_len, out_len);
  spooky_str_ref(s_cp, s_len, bucket->atoms_limits.len, atom);
  spooky_str_inc_ref_count(atom);

  return atom;
}

char * spooky_hash_print_stats(const spooky_hash_table * self) {
  static const size_t max_buf_len = 2048;
  spooky_hash_table_impl * impl = self->impl;
  char * out = calloc(max_buf_len, sizeof * out);
  char * result = out;
  out += snprintf(out, max_buf_len - (size_t)(out - result), "Load factor: %f\n", (double)impl->string_count / (double)impl->buckets_limits.capacity);
  out += snprintf(out, max_buf_len - (size_t)(out - result), "Buckets: (%lu, %lu)\n", (unsigned long)impl->buckets_limits.capacity, (unsigned long)impl->buckets_limits.len);
  out += snprintf(out, max_buf_len - (size_t)(out - result), "Indices: (%lu, %lu):\n", impl->buckets_limits.capacity, impl->buckets_limits.len);

  spooky_string_buffer * buffer = impl->buffers;
  int buffer_count = 0;
  size_t buffer_total_len = 0;

  out += snprintf(out, max_buf_len - (size_t)(out - result), "Buffers:\n");
  while(buffer) {
    out += snprintf(out, max_buf_len - (size_t)(out - result), "\t%i: %lu (%lu, %lu)\n", buffer_count++, buffer->string_count, buffer->capacity, buffer->len);
    buffer_total_len += buffer->len;
    buffer = buffer->next;
  }
  out += snprintf(out, max_buf_len - (size_t)(out - result), "Total buffer size: %lu\n", buffer_total_len);
  //out += snprintf(out, max_buf_len - (size_t)(out - result), "Buckets:\n");
  int collisions = 0;
  int reallocs = 0;
  int max_atoms = 0;
  for(size_t i = 0; i < impl->prime; i++) {
    const spooky_hash_bucket * bucket = &impl->buckets[i];
    if(bucket->prime) {
      max_atoms = (int)bucket->atoms_limits.len > max_atoms ? (int)bucket->atoms_limits.len : max_atoms;
      if(bucket->atoms_limits.len > 1) {
        if(bucket->atoms_limits.reallocs > 0) { reallocs++; };
        for(size_t x = 0; x < bucket->atoms_limits.len; x++) {
          const spooky_str * outer = &bucket->atoms[x];
          for(size_t y = 0; y < bucket->atoms_limits.len; y++) {
            const spooky_str * inner = &bucket->atoms[y];
            if(outer != inner && outer->hash == inner->hash) {
              collisions++;
              out += snprintf(out, max_buf_len - (size_t)(out - result), "Hash Collisions!\n");
              out += snprintf(out, max_buf_len - (size_t)(out - result), "    X: '%s': len: %lu, hash: %lu, ordinal: %lu\n", outer->str, outer->len, outer->hash, outer->ordinal);
              out += snprintf(out, max_buf_len - (size_t)(out - result), "    Y: '%s': len: %lu, hash: %lu, ordinal: %lu\n", inner->str, inner->len, inner->hash, inner->ordinal);
            }
          }
        }
      }
    }
  }

  out += snprintf(out, max_buf_len - (size_t)(out - result), "Total chain reallocations: %i\n", reallocs);
  out += snprintf(out, max_buf_len - (size_t)(out - result), "Total key collisions: %i\n", collisions / 2);
  out += snprintf(out, max_buf_len - (size_t)(out - result), "Max bucket chain count: %i\n", max_atoms);
  out += snprintf(out, max_buf_len - (size_t)(out - result), "Unique keys: %i\n", (int)impl->string_count);

  return result;
}

errno_t spooky_hash_find_internal(const spooky_hash_bucket * bucket, const char * s, size_t s_len, unsigned long hash, const spooky_str ** out_atom) {
  if(out_atom) { *out_atom = NULL; }
  if(!bucket || !bucket->prime) { return SP_FAILURE; }

  register const spooky_str * atom = bucket->atoms;
  register const spooky_str * end = bucket->atoms + bucket->atoms_limits.len;
  do {
    if(s_len == atom->len && hash == atom->hash) {
      register const char * str = atom->str;
      bool found = str == s || strncmp(str, s, SPOOKY_MAX_STRING_LEN) == 0;
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
  register unsigned long hash = spooky_hash_str(s, s_len);
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

  spooky_string_buffer * buffer = impl->current_buffer;
  size_t alloc_len = s_len + 1;
  if(buffer->len + alloc_len > buffer->capacity) {
    /* reallocate strings */
    size_t new_len = 0;
    size_t new_capacity = SPOOKY_HASH_DEFAULT_STRING_ALLOC;
    while(new_len + alloc_len > new_capacity) {
      new_capacity *= 2;
    }
    spooky_string_buffer * new_buffer = calloc(1, sizeof * new_buffer);
    if(!new_buffer) { goto err0; }

    new_buffer->len = 0;
    new_buffer->capacity = new_capacity;
    new_buffer->strings = calloc(new_capacity, sizeof * new_buffer->strings);
		if(!new_buffer->strings) { goto err0; }
    
    impl->current_buffer->next = new_buffer;
    impl->current_buffer = new_buffer;
    buffer = new_buffer;
  }

  buffer->string_count++;
  impl->string_count++;

  size_t n_len = s_len >= SPOOKY_MAX_STRING_LEN ? SPOOKY_MAX_STRING_LEN : s_len;
  char * offset = buffer->strings + buffer->len;
  buffer->len += n_len + 1;

  assert(n_len > 0);

  //memcpy(offset, s, n_len + 1);
  //offset[n_len] = '\0';

  strncpy(offset, s, n_len);

  *out_len = n_len;

  return offset;

err0:
  abort();
}

/* Prime generation for hash table function
see https://en.wikipedia.org/wiki/Sieve_of_Atkin 
limit ← 1000000000        // arbitrary search limit

# set of wheel "hit" positions for a 2/3/5 wheel rolled twice as per the Atkin algorithm
s ← {1,7,11,13,17,19,23,29,31,37,41,43,47,49,53,59}

# Initialize the sieve with enough wheels to include limit:
for n ← 60 × w + x where w ∈ {0,1,...,limit ÷ 60}, x ∈ s:
    is_prime(n) ← false

# Put in candidate primes:
#   integers which have an odd number of
#   representations by certain quadratic forms.
# Algorithm step 3.1:
for n ≤ limit, n ← 4x²+y² where x ∈ {1,2,...} and y ∈ {1,3,...} // all x's odd y's
    if n mod 60 ∈ {1,13,17,29,37,41,49,53}:
        is_prime(n) ← ¬is_prime(n)   // toggle state
# Algorithm step 3.2:
for n ≤ limit, n ← 3x²+y² where x ∈ {1,3,...} and y ∈ {2,4,...} // only odd x's
    if n mod 60 ∈ {7,19,31,43}:                                 // and even y's
        is_prime(n) ← ¬is_prime(n)   // toggle state
# Algorithm step 3.3:
for n ≤ limit, n ← 3x²-y² where x ∈ {2,3,...} and y ∈ {x-1,x-3,...,1} //all even/odd
    if n mod 60 ∈ {11,23,47,59}:                                   // odd/even combos
        is_prime(n) ← ¬is_prime(n)   // toggle state

# Eliminate composites by sieving, only for those occurrences on the wheel:
for n² ≤ limit, n ← 60 × w + x where w ∈ {0,1,...}, x ∈ s, n ≥ 7:
    if is_prime(n):
        // n is prime, omit multiples of its square; this is sufficient 
        // because square-free composites can't get on this list
        for c ≤ limit, c ← n² × (60 × w + x) where w ∈ {0,1,...}, x ∈ s:
            is_prime(c) ← false

# one sweep to produce a sequential list of primes up to limit:
output 2, 3, 5
for 7 ≤ n ≤ limit, n ← 60 × w + x where w ∈ {0,1,...}, x ∈ s:
    if is_prime(n): output n
 */

/* prime generation correctness unchecked */
errno_t spooky_generate_primes(unsigned int limit) {
  assert(limit <= 1000000000);
  bool * sieve = calloc(limit, sizeof * sieve);
  bool *s = sieve;

  for (register unsigned int x = 1; x * x < limit; x++) { 
    for (register unsigned int y = 1; y * y < limit; y++) { 
      register unsigned int n = (4 * x * x) + (y * y); 
      if (n <= limit && (n % 12 == 1 || n % 12 == 5)) { *(s + n) ^= true; }

      n = (3 * x * x) + (y * y); 
      if (n <= limit && n % 12 == 7) { *(s + n) ^= true; }

      n = (3 * x * x) - (y * y); 
      if (x > y && n <= limit && n % 12 == 11) { *(s + n) ^= true; }
    } 
  } 

  for (register unsigned int r = 5; r * r < limit; r++) { 
    if (*(s + r)) { 
      for (register unsigned int i = r * r; i < limit; i += r * r) {
        *(s + i) = false; 
      }
    } 
  } 
/*
  int i = 0;
  for (register unsigned int a = 5; a < limit; a++) {
    if (*(s + a)) {
      i++;
      if((i % 10 == 0) && a < limit - 1) { fprintf(stdout, "\n"); }
      fprintf(stdout, "%i",  a);  
      if(a < limit - 3) { fprintf(stdout, ","); }
    }
  }
  fprintf(stdout, "\n");
*/
  return SP_SUCCESS;
}



