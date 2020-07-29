#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <stddef.h>
#include <math.h>

#include "sp_limits.h"
#include "sp_error.h"
#include "sp_hash.h"

static const double spooky_hash_default_load_factor = 0.75;

static const uint64_t spooky_hash_primes[] = {
  2, 3, 5, 7, 11, 13, 17, 23, 29, 37, 47,
  59, 73, 97, 127, 151, 197, 251, 313, 397,
  499, 631, 797, 1009, 1259, 1597, 2011, 2539,
  3203, 4027, 5087, 6421, 8089, 10193, 12853, 16193,
  20399, 25717, 32401, 40823, 51437, 64811, 81649,
  102877, 129607, 163307, 205759, 259229, 326617,
  411527, 518509, 653267, 823117, 1037059, 1306601,
  1646237, 2074129, 2613229, 3292489, 4148279, 5226491,
  6584983, 8296553, 10453007, 13169977, 16593127, 20906033,
  26339969, 33186281, 41812097, 52679969, 66372617,
  83624237, 105359939, 132745199, 167248483, 210719881,
  265490441, 334496971, 421439783, 530980861, 668993977,
  842879579, 1061961721, 1337987929, 1685759167, 2123923447,
  2675975881, 3371518343, 4247846927, 5351951779, 6743036717,
  8495693897, 10703903591, 13486073473, 16991387857,
  21407807219, 26972146961, 33982775741, 42815614441,
  53944293929, 67965551447, 85631228929, 107888587883,
  135931102921, 171262457903, 215777175787, 271862205833,
  342524915839, 431554351609, 543724411781, 685049831731,
  863108703229, 1087448823553, 1370099663459, 1726217406467,
  2174897647073, 2740199326961, 3452434812973, 4349795294267,
  5480398654009, 6904869625999, 8699590588571, 10960797308051,
  13809739252051, 17399181177241, 21921594616111, 27619478504183,
  34798362354533, 43843189232363, 55238957008387, 69596724709081,
  87686378464759, 110477914016779, 139193449418173,
  175372756929481, 220955828033581, 278386898836457,
  350745513859007, 441911656067171, 556773797672909,
  701491027718027, 883823312134381, 1113547595345903,
  1402982055436147, 1767646624268779, 2227095190691797,
  2805964110872297, 3535293248537579, 4454190381383713,
  5611928221744609, 7070586497075177, 8908380762767489,
  11223856443489329, 14141172994150357, 17816761525534927,
  22447712886978529, 28282345988300791, 35633523051069991,
  44895425773957261, 56564691976601587, 71267046102139967,
  89790851547914507, 113129383953203213, 142534092204280003,
  179581703095829107, 226258767906406483, 285068184408560057,
  359163406191658253, 452517535812813007, 570136368817120201,
  718326812383316683, 905035071625626043, 1140272737634240411,
  1436653624766633509, 1810070143251252131, 2280545475268481167,
  2873307249533267101, 3620140286502504283, 4561090950536962147,
  5746614499066534157, 7240280573005008577, 9122181901073924329,
  11493228998133068689llu, 14480561146010017169llu, 18446744073709551557llu
};

//static unsigned long * spooky_generate_primes(size_t limit);
static const size_t SPOOKY_HASH_DEFAULT_ATOM_ALLOC = 1024;
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
  size_t prime_index;
  unsigned long prime;

  size_t atoms_alloc;
  size_t strings_alloc;

  spooky_array_limits buckets_limits;
  spooky_hash_bucket * buckets;

  size_t string_count;
  spooky_string_buffer * buffers;
  spooky_string_buffer * current_buffer;
} spooky_hash_table_impl;

static errno_t spooky_hash_ensure_internal(const spooky_hash_table * self, const char * s, size_t s_len, const spooky_str ** out_str, bool skip_s_cp, bool skip_rebalance) ;
static const spooky_hash_table * spooky_hash_table_cctor(const spooky_hash_table * self, size_t prime_index, spooky_string_buffer * buffers, spooky_string_buffer * current_buffer);
static errno_t spooky_hash_ensure(const spooky_hash_table * self, const char * s, size_t s_len, const spooky_str ** out_str);
static errno_t spooky_hash_find_internal(const spooky_hash_bucket * bucket, const char * s, size_t s_len, unsigned long hash, const spooky_str ** atom);
static errno_t spooky_hash_find(const spooky_hash_table * self, const char * s, size_t s_len, const spooky_str ** atom);
static const char * spooky_hash_move_string_to_strings(const spooky_hash_table * self, const char * s, size_t s_len, size_t * out_len);
static void spooky_hash_clear_strings(const spooky_hash_table * self);
static char * spooky_hash_print_stats(const spooky_hash_table * self);

static const spooky_str * spooky_hash_atom_alloc(const spooky_hash_table * self, spooky_hash_bucket * bucket, const char * s, size_t s_len, unsigned long hash, size_t * out_len, bool skip_s_cp);
static spooky_str * spooky_hash_order_bucket_atoms(const spooky_hash_bucket * bucket, spooky_str * atom);

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

const spooky_hash_table * spooky_hash_table_cctor(const spooky_hash_table * self, size_t prime_index, spooky_string_buffer * buffers, spooky_string_buffer * current_buffer) {
  spooky_hash_table_impl * impl = calloc(1, sizeof * self->impl); 
  if(!impl) goto err0;

  impl->atoms_alloc = SPOOKY_HASH_DEFAULT_ATOM_ALLOC;
  impl->strings_alloc = SPOOKY_HASH_DEFAULT_STRING_ALLOC;

  impl->prime_index = prime_index;
  impl->prime = spooky_hash_primes[impl->prime_index];
  impl->buckets_limits.capacity = (sizeof spooky_hash_primes / sizeof spooky_hash_primes[0]);
  impl->buckets_limits.len = impl->prime_index;
  impl->buckets = calloc(impl->buckets_limits.capacity, sizeof * impl->buckets);
  if(!impl->buckets) { abort(); }

  //fprintf(stdout, "Allocated buckets (%lu, %lu), %lu\n",impl->buckets_limits.capacity, impl->buckets_limits.len, impl->buckets_limits.capacity * sizeof * impl->buckets);

  if(!buffers) {
    impl->buffers = calloc(1, sizeof * impl->buffers);
    impl->buffers->next = NULL;
    impl->buffers->len = 0;
    impl->buffers->capacity = SPOOKY_HASH_DEFAULT_STRING_ALLOC;
    impl->buffers->strings = calloc(impl->buffers->capacity , sizeof * impl->buffers->strings);
    impl->current_buffer = impl->buffers;
  } else {
    impl->buffers = buffers;
    if(current_buffer) {
      impl->current_buffer = current_buffer;
    } else {
      impl->current_buffer = impl->buffers;
    }
  }

  ((spooky_hash_table *)(uintptr_t)self)->impl = impl;

  return self;

err0:
  abort();
}

const spooky_hash_table * spooky_hash_table_ctor(const spooky_hash_table * self) {
  return spooky_hash_table_cctor(self, 0, NULL, NULL);
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

  impl->buckets_limits.len = impl->prime_index;
  impl->buckets_limits.capacity = (sizeof spooky_hash_primes / sizeof spooky_hash_primes[0]);
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

void spooky_hash_rebalance(const spooky_hash_table * self) {
  const spooky_hash_table * old_self = self;
  spooky_hash_table_impl * old_impl = old_self->impl;
  size_t old_buckets_len = old_impl->buckets_limits.len;

  double load_factor = (double)old_impl->string_count / (double)old_buckets_len;

  if(load_factor > spooky_hash_default_load_factor) {
    unsigned long new_prime_index = old_impl->prime_index + 1;
    if(new_prime_index > (sizeof spooky_hash_primes / sizeof spooky_hash_primes[0])) { 
      return;
    }

    //fprintf(stdout, "Rebalancing hash with load factor of %f with %lu keys...\n", load_factor, old_impl->string_count);
    spooky_hash_bucket * old_buckets = old_impl->buckets;
    self = spooky_hash_table_cctor(self, new_prime_index, old_impl->buffers, old_impl->current_buffer);
    self->impl->string_count = old_impl->string_count;
    {
      // Relocate atoms to new hash table:
      spooky_hash_bucket * old_bucket = old_buckets;
      const spooky_hash_bucket * old_bucket_end = old_buckets + old_buckets_len;

      while(old_bucket < old_bucket_end) {
        if(old_bucket->atoms_limits.len > 0) {
          spooky_str * old_atom = old_bucket->atoms;
          const spooky_str * old_atom_end = old_bucket->atoms + old_bucket->atoms_limits.len;

          register unsigned long hash = old_atom->hash;
          register unsigned long index = (hash % spooky_hash_primes[self->impl->prime_index]) & (self->impl->buckets_limits.len - 1);
          spooky_hash_bucket * new_bucket = &(self->impl->buckets[index]);
          assert(index <= self->impl->prime && index < self->impl->buckets_limits.len);
          if(!new_bucket->prime) {
            new_bucket->prime = spooky_hash_primes[index];
            new_bucket->atoms_limits.len = 0;
            new_bucket->atoms_limits.capacity = self->impl->atoms_alloc;
            new_bucket->atoms = calloc(new_bucket->atoms_limits.capacity, sizeof * new_bucket->atoms);
            if(!new_bucket->atoms) { abort(); }
          }
          while(old_atom < old_atom_end) {
            const spooky_str * found = NULL;
            if(spooky_hash_find_internal(new_bucket, old_atom->str, old_atom->len, hash, &found) != SP_SUCCESS) {
              /* not already added */
              if(new_bucket->atoms_limits.len + 1 > new_bucket->atoms_limits.capacity) {
                new_bucket->atoms_limits.capacity *= 2;
                spooky_str * temp_atoms = realloc(new_bucket->atoms, (sizeof * temp_atoms) * new_bucket->atoms_limits.capacity);
                if(!temp_atoms) { abort(); }
                new_bucket->atoms = temp_atoms;
                new_bucket->atoms_limits.reallocs++;
              }

              spooky_str * new_atom = new_bucket->atoms + new_bucket->atoms_limits.len;
              new_bucket->atoms_limits.len++;
              
              spooky_str_copy(&new_atom, old_atom);

              spooky_hash_order_bucket_atoms(new_bucket, new_atom);
            }
            old_atom++;
          }
        }
        old_bucket++;
      }
    }

    {
      // release the old hash table, but not the string buffers which are owned by new self->impl:
      spooky_hash_bucket * old_bucket = old_buckets;
      const spooky_hash_bucket * old_bucket_end = old_buckets + old_buckets_len;
      while(old_bucket < old_bucket_end) {
        free(old_bucket->atoms), old_bucket->atoms = NULL;
        old_bucket++;
      }
      free(old_buckets), old_buckets = NULL; 
    }
    free(old_impl), old_impl = NULL;
  }

  assert(self && self->impl);
}

errno_t spooky_hash_ensure(const spooky_hash_table * self, const char * s, size_t s_len, const spooky_str ** out_str) {
  return spooky_hash_ensure_internal(self, s, s_len, out_str, false, false);
}

errno_t spooky_hash_ensure_internal(const spooky_hash_table * self, const char * s, size_t s_len, const spooky_str ** out_str, bool skip_s_cp, bool skip_rebalance) {
  if(!s) { return SP_FAILURE; }
  if(s_len <= 0) { return SP_FAILURE; }

  assert(s_len <= SPOOKY_MAX_STRING_LEN);

  if(!skip_rebalance) { spooky_hash_rebalance(self); }

  spooky_hash_table_impl * impl = self->impl;
  
  register unsigned long hash = spooky_hash_str(s, s_len);
  register unsigned long index = (hash % spooky_hash_primes[impl->prime_index]) & (impl->buckets_limits.len - 1);

  spooky_hash_bucket * bucket = &(impl->buckets[index]);

  if(!bucket->prime) {
    /* bucket hasn't been initialized: */
    bucket->prime = spooky_hash_primes[index];
    bucket->atoms_limits.len = 0;
    bucket->atoms_limits.capacity = impl->atoms_alloc;
    bucket->atoms = calloc(bucket->atoms_limits.capacity, sizeof * bucket->atoms);
    if(!bucket->atoms) { goto err0; }
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
  
  assert(bucket && bucket->prime > 0);

  size_t out_len = 0;
  const spooky_str * temp_str = spooky_hash_atom_alloc(self, bucket, s, s_len, hash, &out_len, skip_s_cp);
  assert(out_len == temp_str->len);
  impl->string_count++;

  if(out_str) { *out_str = temp_str; }

  return SP_SUCCESS;

err0:
  abort();
}

static spooky_str * spooky_hash_order_bucket_atoms(const spooky_hash_bucket * bucket, spooky_str * atom) {
  static int print_first = 0;
  bool perform_shift = false;
  size_t index = 0;
  for(; index < bucket->atoms_limits.len; index++) {
    // 4
    //|1|2|3|4|5|
    //       ^ 
    const spooky_str * temp = bucket->atoms + index;
    if(atom->hash > temp->hash) { 
      perform_shift = true;
      break;
    }
  }

  if(!print_first && print_first < 10) {
    fprintf(stdout, "Insert '%s' with hash %lu\n", atom->str, atom->hash);
  }

  if(perform_shift) {
    if(!print_first) {
      fprintf(stdout, "Start Hashes: (%lu) ", bucket->atoms_limits.len);
      for(size_t i = 0; i < bucket->atoms_limits.len; i++) {
        const spooky_str * x = bucket->atoms + i;
        fprintf(stdout, "%lu - ", x->hash);
      }
      fprintf(stdout, "\n");
    }
    spooky_str temp = { 0 }, * tp = &temp;
    spooky_str_swap(&atom, &tp);
    
    //         5 len
    //|0|1|2|3|4| 
    //   ^
    //   1 inx
    //         5 - 1 = 4

    size_t shift_len = bucket->atoms_limits.len - index - 1;
    const spooky_str * src = bucket->atoms + index;
    spooky_str * dest = bucket->atoms + (index + 1);
    memmove(dest, src, shift_len * sizeof bucket->atoms[0]);
    atom = &(bucket->atoms[index]);
    spooky_str_swap(&tp, &atom);
    atom->ordinal = index;

    if(!print_first && print_first < 10) {
      fprintf(stdout, "End Hashes: (%lu) ", bucket->atoms_limits.len);
      for(size_t i = 0; i < bucket->atoms_limits.len; i++) {
        const spooky_str * x = bucket->atoms + i;
        fprintf(stdout, "%lu - ", x->hash);
      }
      fprintf(stdout, "\n");
      fprintf(stdout, "Inserted '%s' with hash %lu\n", atom->str, atom->hash);
      print_first++;
    }
  }

  return atom;
}

static const spooky_str * spooky_hash_atom_alloc(const spooky_hash_table * self, spooky_hash_bucket * bucket, const char * s, size_t s_len, unsigned long hash, size_t * out_len, bool skip_s_cp) {
  assert(self && bucket && bucket->prime);

  spooky_str * atom = &bucket->atoms[bucket->atoms_limits.len];
  const char * s_cp = s;
  
  /* skip copy of s if we're rebalancing; our string pointers already point to our internal buffer */
  if(!skip_s_cp) {
    s_cp = spooky_hash_move_string_to_strings(self, s, s_len, out_len);
  } else { 
    *out_len = s_len;
  }
  
  spooky_str_ref(s_cp, s_len, bucket->atoms_limits.len, hash, atom);
  spooky_str_inc_ref_count(atom);
  bucket->atoms_limits.len++;
  atom = spooky_hash_order_bucket_atoms(bucket, atom);
  
  return atom;
}

bool spooky_hash_binary_search(const spooky_str * atoms, size_t low, size_t n, unsigned long hash, size_t * out_index) {
  assert(n > 0);

  if(n > 1) {
    for(size_t x = 0; x < n - 1; x++) {
      const spooky_str * first = atoms + x;
      const spooky_str * second = atoms + x + 1;
      if(!(first->hash >= second->hash)) { fprintf(stdout, "FAILED: %lu, %lu\n", first->hash, second->hash); }
      assert(first->hash >= second->hash);
    }
  }

  if(n == 0) abort();
  int64_t i = (int64_t)low, j = (int64_t)n - 1;
  while(i <= j) {
    int64_t k = i + ((j - i) / 2);
    //fprintf(stdout, "i: %lu, j: %lu, low: %lu, n: %lu, k: %lu\n", i, j, low, n, k);
    if(atoms[k].hash < hash) {
      i = k + 1;
    } else if(atoms[k].hash > hash) {
      j = k - 1; 
    } else {
      if(k >= 0 && k < INT64_MAX) {
        *out_index = (size_t)k;
      }
      return true;
    } 
  }

  return false;
}

errno_t spooky_hash_find_internal(const spooky_hash_bucket * bucket, const char * s, size_t s_len, unsigned long hash, const spooky_str ** out_atom) {
  if(out_atom) { *out_atom = NULL; }
  if(!bucket || !bucket->prime) { return SP_FAILURE; }
  if(bucket->atoms_limits.len == 0) { return SP_FAILURE; }

  size_t index = 0;
  if(spooky_hash_binary_search(bucket->atoms, 0, bucket->atoms_limits.len, hash, &index) == SP_SUCCESS) {
    const spooky_str * atom = &(bucket->atoms[index]);
    const spooky_str * end = bucket->atoms + bucket->atoms_limits.len;
    while(atom < end) {
      if(s_len == atom->len && hash == atom->hash) {
        const char * str = atom->str;
        if(strncmp(str, s, SPOOKY_MAX_STRING_LEN) == 0) {
          if(out_atom) { *out_atom = atom; }
          return SP_SUCCESS;
        }
      } else if(hash != atom->hash) { break; }
      atom++;
    }
  }
  return SP_FAILURE;
}

errno_t spooky_hash_find(const spooky_hash_table * self, const char * s, size_t s_len, const spooky_str ** atom) {
  spooky_hash_table_impl * impl = self->impl;
  register unsigned long hash = spooky_hash_str(s, s_len);
  register unsigned long index = (hash % spooky_hash_primes[impl->prime_index]) & (impl->buckets_limits.len - 1);
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

  size_t n_len = s_len >= SPOOKY_MAX_STRING_LEN ? SPOOKY_MAX_STRING_LEN : s_len;
  char * offset = buffer->strings + buffer->len;
  buffer->len += n_len + 1;

  assert(n_len > 0);
  strncpy(offset, s, n_len);
  *out_len = n_len;

  return offset;

err0:
  abort();
}

char * spooky_hash_print_stats(const spooky_hash_table * self) {
  static const size_t max_buf_len = 2048;
  spooky_hash_table_impl * impl = self->impl;
  char * out = calloc(max_buf_len, sizeof * out);
  char * result = out;
  out += snprintf(out, max_buf_len - (size_t)(out - result), "Load factor: %f\n", (double)impl->string_count / (double)impl->buckets_limits.len);
  out += snprintf(out, max_buf_len - (size_t)(out - result), "Buckets: (%lu, %lu)\n", (unsigned long)impl->buckets_limits.capacity, (unsigned long)impl->buckets_limits.len);

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
  
  for(size_t i = 0; i < impl->buckets_limits.len; i++) {
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

