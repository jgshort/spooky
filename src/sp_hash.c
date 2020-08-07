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

static const size_t SPOOKY_HASH_DEFAULT_ATOM_ALLOC = 1 << 12;
static const size_t SPOOKY_HASH_DEFAULT_STRING_ALLOC = 1048576 * 2;

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

typedef struct spooky_hash_bucket_item spooky_hash_bucket_item;
typedef struct spooky_hash_bucket_item {
  spooky_str atom;
  spooky_array_limits siblings_limits;
  spooky_hash_bucket_item ** siblings;
  spooky_hash_bucket_item * next;
  spooky_hash_bucket_item * prev;
} spooky_hash_bucket_item;

typedef struct spooky_hash_bucket {
  unsigned long prime;
  spooky_array_limits items_limits;
  spooky_hash_bucket_item * root;
  spooky_hash_bucket_item * items;
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

static errno_t spooky_hash_ensure_internal(const spooky_hash_table * self, const char * s, size_t s_len, spooky_str ** out_str, bool skip_s_cp, bool skip_rebalance) ;
static const spooky_hash_table * spooky_hash_table_cctor(const spooky_hash_table * self, size_t prime_index, spooky_string_buffer * buffers, spooky_string_buffer * current_buffer);
static errno_t spooky_hash_ensure(const spooky_hash_table * self, const char * s, size_t s_len, spooky_str ** out_str);
static errno_t spooky_hash_find_internal(const spooky_hash_bucket * bucket, const char * s, size_t s_len, unsigned long hash, spooky_str ** atom);
static errno_t spooky_hash_find(const spooky_hash_table * self, const char * s, size_t s_len, spooky_str ** atom);
static const char * spooky_hash_move_string_to_strings(const spooky_hash_table * self, const char * s, size_t s_len, size_t * out_len);
static void spooky_hash_clear_strings(const spooky_hash_table * self);
static char * spooky_hash_print_stats(const spooky_hash_table * self);

static spooky_str * spooky_hash_atom_alloc(const spooky_hash_table * self, spooky_hash_bucket * bucket, const char * s, size_t s_len, unsigned long hash, size_t * out_len, bool skip_s_cp);
static spooky_str * spooky_hash_order_bucket_items(const spooky_hash_bucket * bucket, spooky_str * atom);
static double spooky_hash_get_load_factor(const spooky_hash_table * self);

static size_t spooky_hash_get_bucket_length(const spooky_hash_table * self);
static size_t spooky_hash_get_bucket_capacity(const spooky_hash_table * self);
static size_t spooky_hash_get_key_count(const spooky_hash_table * self);

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
  self->get_load_factor = &spooky_hash_get_load_factor;

  self->get_bucket_length = &spooky_hash_get_bucket_length;
  self->get_bucket_capacity = &spooky_hash_get_bucket_capacity;
  self->get_key_count = &spooky_hash_get_key_count;

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

  if(!buffers) {
    static const size_t max_buffers = 5;

    size_t i = 0;
    spooky_string_buffer * first_buffer = NULL;
    spooky_string_buffer * prev = NULL;
    do {
      spooky_string_buffer * buffer = calloc(1, sizeof * impl->buffers);
      if(prev) { prev->next = buffer; }
      if(!first_buffer) { first_buffer = buffer; }
      buffer->next = NULL;
      buffer->len = 0;
      buffer->capacity = SPOOKY_HASH_DEFAULT_STRING_ALLOC;
      buffer->strings = calloc(buffer->capacity , sizeof * buffer->strings);
      prev = buffer;
      i++;
    } while(i < max_buffers);
    impl->buffers = first_buffer; 
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
  return spooky_hash_table_cctor(self, 32, NULL, NULL);
}

void spooky_hash_clear_strings(const spooky_hash_table * self) {
  spooky_hash_table_impl * impl = self->impl;

  spooky_string_buffer * buffer = impl->buffers;
  do {
    spooky_string_buffer * next = buffer->next;
    free(buffer->strings), buffer->strings = NULL;
    free(buffer), buffer = NULL;
    buffer = next;
  } while(buffer);
}

void spooky_hash_clear_buckets(const spooky_hash_table * self) {
  spooky_hash_table_impl * impl = self->impl;

  for(size_t i = 0; i < impl->buckets_limits.len; i++) {
    spooky_hash_bucket * bucket = &impl->buckets[i];
    free(bucket->items), bucket->items = NULL;
  }

  impl->buckets_limits.len = impl->prime_index;
  impl->buckets_limits.capacity = (sizeof spooky_hash_primes / sizeof spooky_hash_primes[0]);
  free(impl->buckets), impl->buckets = NULL;
}

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

static inline unsigned long spooky_hash_get_index(const spooky_hash_table * self, unsigned long hash) {
  return (hash % spooky_hash_primes[self->impl->prime_index]) % (self->impl->prime_index);
}

static spooky_hash_bucket * spooky_hash_bucket_init(const spooky_hash_table * self, unsigned long index) {
  assert(self && self->impl);

  spooky_hash_bucket * bucket = &(self->impl->buckets[index]);

  assert(index <= self->impl->prime && index <= self->impl->buckets_limits.len);
  if(!bucket->prime) {
    bucket->prime = spooky_hash_primes[index];
    bucket->items_limits.len = 0;
    bucket->items_limits.capacity = self->impl->atoms_alloc;
    bucket->items = calloc(bucket->items_limits.capacity, sizeof * bucket->items);
    if(!bucket->items) { abort(); }
  }

  return bucket;
}

static spooky_hash_bucket * spooky_hash_bucket_check_and_realloc(spooky_hash_bucket * bucket) {
  assert(bucket && bucket->items);
  if(bucket->items_limits.len + 1 > bucket->items_limits.capacity) {
    bucket->items_limits.capacity *= 2;
    spooky_hash_bucket_item * temp_items = realloc(bucket->items, (sizeof * temp_items) * bucket->items_limits.capacity);
    if(!temp_items) { abort(); }
    bucket->items = temp_items;
    bucket->items_limits.reallocs++;
  }

  return bucket;
}

static spooky_hash_bucket_item * spooky_hash_bucket_get_next_item(spooky_hash_bucket * bucket) {
  bucket = spooky_hash_bucket_check_and_realloc(bucket);
  spooky_hash_bucket_item * item= bucket->items + bucket->items_limits.len;
  bucket->items_limits.len++;
  assert(item);
  return item;
}

static void spooky_hash_bucket_insert_item(spooky_hash_bucket * bucket, spooky_hash_bucket_item * node, spooky_hash_bucket_item * item) {
  if(item->atom.hash > node->atom.hash) {
    // insert to the right
    if(node->next) { 
      spooky_hash_bucket_insert_item(bucket, node->next, item);
    } else {
      node->next = item;
    }
  }
  else if(item->atom.hash < node->atom.hash) {
    // insert to the left
    if(node->prev) { 
      spooky_hash_bucket_insert_item(bucket, node->prev, item);
    } else {
      node->prev = item;
    }
  }
  else {
    if(node->siblings_limits.len == 0) {
      // no siblings, alloc one:
      node->siblings_limits.len = 0;
      node->siblings_limits.capacity = 16;
      node->siblings = calloc(item->siblings_limits.capacity, sizeof * item->siblings);
      if(!node->siblings) { abort(); }
    } else if(node->siblings_limits.len + 1 > node->siblings_limits.capacity) {
      // siblings overflow, realloc:
      node->siblings_limits.capacity *= 2;
      spooky_hash_bucket_item ** temp = realloc(node->siblings, node->siblings_limits.capacity * sizeof * temp);
      if(!temp) {  abort(); }
      node->siblings = temp;
    }
    // set next sibling:
    spooky_hash_bucket_item ** sibling = node->siblings + node->siblings_limits.len;
    *sibling = item;
    node->siblings_limits.len++;
  }
}

double spooky_hash_get_load_factor(const spooky_hash_table * self) {
  return (double)self->impl->string_count / (double)self->impl->buckets_limits.len;
}

size_t spooky_hash_get_bucket_length(const spooky_hash_table * self) {
  return self->impl->buckets_limits.len;
}

size_t spooky_hash_get_bucket_capacity(const spooky_hash_table * self) {
  return self->impl->buckets_limits.capacity;
}

size_t spooky_hash_get_key_count(const spooky_hash_table * self) {
  return self->impl->string_count;
}
/*
static int spooky_hash_bucket_item_compare(const void * a, const void * b) {
  const spooky_hash_bucket_item * l = (const spooky_hash_bucket_item  *)a;
  const spooky_hash_bucket_item * r = (const spooky_hash_bucket_item  *)b;
  if(l->atom.hash < r->atom.hash) return -1;
  if(l->atom.hash > r->atom.hash) return 1;
  return 0;
}
*/
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

    spooky_hash_bucket * old_buckets = old_impl->buckets;
    self = spooky_hash_table_cctor(self, new_prime_index, old_impl->buffers, old_impl->current_buffer);
    self->impl->string_count = old_impl->string_count;
    {
      // Relocate atoms to new hash table:
      spooky_hash_bucket * old_bucket = old_buckets;
      const spooky_hash_bucket * old_bucket_end = old_buckets + old_buckets_len;

      while(old_bucket < old_bucket_end) {
        if(old_bucket->items_limits.len > 0) {
          spooky_hash_bucket_item * old_item = old_bucket->items;
          const spooky_hash_bucket_item * old_item_end = old_bucket->items + old_bucket->items_limits.len;

          unsigned long hash = old_item->atom.hash;
          unsigned long index = spooky_hash_get_index(self, hash);

          spooky_hash_bucket * new_bucket = spooky_hash_bucket_init(self, index);

          while(old_item < old_item_end) {
            /* not already added */
            new_bucket = spooky_hash_bucket_check_and_realloc(new_bucket);
           
            //static spooky_hash_bucket_item * spooky_hash_bucket_get_next_item(spooky_hash_bucket * bucket)
            //static void spooky_hash_bucket_insert_item(spooky_hash_bucket * bucket, spooky_hash_bucket_item * node, spooky_hash_bucket_item * item) 
            
            spooky_hash_bucket_item * new_item = spooky_hash_bucket_get_next_item(new_bucket);
            new_item->atom = old_item->atom;
            spooky_hash_bucket_insert_item(new_bucket, new_bucket->root, new_item);
            old_item++;
          }
        }
        old_bucket++;
      }
    }
/*
    spooky_hash_bucket * unsorted_bucket = self->impl->buckets;
    const spooky_hash_bucket * unsorted_bucket_end = self->impl->buckets + self->impl->buckets_limits.len;
    while(unsorted_bucket < unsorted_bucket_end) {

      qsort(unsorted_bucket->items, unsorted_bucket->items_limits.len, sizeof unsorted_bucket->items[0], &spooky_hash_bucket_item_compare);
      for(size_t i = 0; i < unsorted_bucket->items_limits.len; i++) {
        spooky_hash_bucket_item * left = unsorted_bucket->items + i;
        spooky_hash_bucket_item * right = NULL;
        if(i <= unsorted_bucket->items_limits.len - 2) { 
          right = unsorted_bucket->items + i + 1;
          left->next = right;
          right->prev = left;
        }
        if(i == 0) { left->prev = NULL; }
        if(i == unsorted_bucket->items_limits.len - 1) { left->next = NULL; }
      }
      unsorted_bucket->root = unsorted_bucket->items;
      unsorted_bucket++;
    }
*/
    /*DIAGNOSTICS:
    * unsorted_bucket = self->impl->buckets;
    * while(unsorted_bucket < unsorted_bucket_end) {
    *   if(unsorted_bucket->prime == 1337987929) {
    *     fprintf(stdout, "bucket %lu\n", unsorted_bucket->prime);
    *     for(size_t i = 0; i < unsorted_bucket->items_limits.len; i++) {
    *       spooky_hash_bucket_item * item = unsorted_bucket->items + i;
    *       fprintf(stdout, "(%lu) ME: %p, PREV: %p, NEXT: %p\n", item->atom.hash, (void*)item, (void*)item->prev, (void*)item->next);
    *     }
    *   }
    *   unsorted_bucket++;
    * }
    */ 

    {
      // release the old hash table, but not the string buffers which are owned by new self->impl:
      spooky_hash_bucket * old_bucket = old_buckets;
      const spooky_hash_bucket * old_bucket_end = old_buckets + old_buckets_len;
      while(old_bucket < old_bucket_end) {
        free(old_bucket->items), old_bucket->items = NULL;
        old_bucket++;
      }
      free(old_buckets), old_buckets = NULL; 
    }
    free(old_impl), old_impl = NULL;
  }


  assert(self && self->impl);
}

errno_t spooky_hash_ensure(const spooky_hash_table * self, const char * s, size_t s_len, spooky_str ** out_str) {
  return spooky_hash_ensure_internal(self, s, s_len, out_str, false, false);
}

errno_t spooky_hash_ensure_internal(const spooky_hash_table * self, const char * s, size_t s_len, spooky_str ** out_str, bool skip_s_cp, bool skip_rebalance) {
  if(!s) { return SP_FAILURE; }
  if(s_len <= 0) { return SP_FAILURE; }

  assert(s_len <= SPOOKY_MAX_STRING_LEN);

  if(!skip_rebalance) { spooky_hash_rebalance(self); }

  spooky_hash_table_impl * impl = self->impl;

  register unsigned long hash = spooky_hash_str(s, s_len);
  register unsigned long index = spooky_hash_get_index(self, hash);

  spooky_hash_bucket * bucket = spooky_hash_bucket_init(self, index);

  if(bucket->prime) {
    /* check if it already exists */
    spooky_str * found = NULL;
    if(spooky_hash_find_internal(bucket, s, s_len, hash, &found) == SP_SUCCESS) {
      if(found) { spooky_str_inc_ref_count((spooky_str *)(uintptr_t)found); }
      if(out_str) { *out_str = found; }
      return SP_SUCCESS; 
    }

    /* bucket exists but str wasn't found, above; allocate string and stuff it into a bucket */
    bucket = spooky_hash_bucket_check_and_realloc(bucket);
  }

  assert(bucket && bucket->prime > 0);

  size_t out_len = 0;
  spooky_str * temp_str = spooky_hash_atom_alloc(self, bucket, s, s_len, hash, &out_len, skip_s_cp);
  assert(out_len == temp_str->len);
  impl->string_count++;

  if(out_str) { *out_str = temp_str; }

  return SP_SUCCESS;
}

static spooky_str * spooky_hash_order_bucket_items(const spooky_hash_bucket * bucket, spooky_str * atom) {
  if(bucket->items_limits.len > 1) {
    if(atom->hash < bucket->items[bucket->items_limits.len - 2].atom.hash) {
      spooky_hash_bucket_item * left = &bucket->items[bucket->items_limits.len - 2];
      spooky_hash_bucket_item * right = &bucket->items[bucket->items_limits.len - 1];
      while(left >= bucket->items && right->atom.hash < left->atom.hash) {
        spooky_str temp = left->atom;
        left->atom = right->atom;
        right->atom = temp;
        if(atom == &(right->atom)) {
          /* restore atom */
          atom = &(left->atom);
        }
        left--;
        right--;
      }
    }
  }

  return atom;
}

static spooky_str * spooky_hash_atom_alloc(const spooky_hash_table * self, spooky_hash_bucket * bucket, const char * s, size_t s_len, unsigned long hash, size_t * out_len, bool skip_s_cp) {
  assert(self && bucket && bucket->prime);

  spooky_str * atom = &bucket->items[bucket->items_limits.len].atom;
  const char * s_cp = s;

  /* skip copy of s if we're rebalancing; our string pointers already point to our internal buffer */
  if(!skip_s_cp) {
    s_cp = spooky_hash_move_string_to_strings(self, s, s_len, out_len);
  } else { 
    *out_len = s_len;
  }

  spooky_str_ref(s_cp, s_len, hash, atom);
  spooky_str_inc_ref_count(atom);
  bucket->items_limits.len++;
  atom = spooky_hash_order_bucket_items(bucket, atom);

  return atom;
}

errno_t spooky_hash_binary_search(const spooky_hash_bucket_item * items, size_t low, size_t n, unsigned long hash, size_t * out_index) {
  assert(n > 0);

  register int64_t i = (int64_t)low;
  register int64_t j = (int64_t)n - 1;
  while(i <= j) {
    register int64_t k = i + ((j - i) / 2);
    register unsigned long a_hash = items[k].atom.hash;
    if(a_hash < hash) {
      i = k + 1;
    } else if(a_hash > hash) {
      j = k - 1; 
    } else {
      assert(k >= 0 && k < (int64_t)n);
      *out_index = (size_t)k;
      return SP_SUCCESS;
    } 
  }

  return SP_FAILURE;
}

errno_t spooky_hash_find_internal(const spooky_hash_bucket * bucket, const char * s, size_t s_len, unsigned long hash, spooky_str ** out_atom) {
  if(out_atom) { *out_atom = NULL; }
  if(!bucket || !bucket->prime) { return SP_FAILURE; }
  if(bucket->items_limits.len == 0) { return SP_FAILURE; }

  size_t index = 0;
  if(spooky_hash_binary_search(bucket->items, 0, bucket->items_limits.len, hash, &index) == SP_SUCCESS) {
    spooky_hash_bucket_item * item = bucket->items + index;
    const spooky_hash_bucket_item * end = bucket->items + bucket->items_limits.len;
    while(item < end) {
      if(hash < item->atom.hash) { break; }
      if(hash == item->atom.hash) {
        register const char * str = item->atom.str;
        if(s_len == item->atom.len && (str == s || strncmp(str, s, SPOOKY_MAX_STRING_LEN) == 0)) {
          if(out_atom) { *out_atom = &(item->atom); }
          return SP_SUCCESS;
        }
      }
      item++;
    }
  }
  return SP_FAILURE;
}

errno_t spooky_hash_find(const spooky_hash_table * self, const char * s, size_t s_len, spooky_str ** atom) {
  spooky_hash_table_impl * impl = self->impl;
  register unsigned long hash = spooky_hash_str(s, s_len);
  register unsigned long index = spooky_hash_get_index(self, hash);
  assert(index < impl->prime);
  spooky_hash_bucket * bucket = &impl->buckets[index];
  return spooky_hash_find_internal(bucket, s, s_len, hash, atom); 
}

const char * spooky_hash_move_string_to_strings(const spooky_hash_table * self, const char * s, size_t s_len, size_t * out_len) {
  spooky_hash_table_impl * impl = self->impl;
  
  if(!s) { return NULL; }
  if(s_len <= 0) { return NULL; }
  if(s_len >= SPOOKY_MAX_STRING_LEN) { s_len = SPOOKY_MAX_STRING_LEN; }

  assert(s_len > 0 && s_len <= SPOOKY_MAX_STRING_LEN);
  spooky_string_buffer * buffer = impl->current_buffer;
  size_t alloc_len = s_len + 1;
  if(buffer->len + alloc_len > buffer->capacity) {
    // reallocate strings
    size_t new_len = 0;
    size_t new_capacity = SPOOKY_HASH_DEFAULT_STRING_ALLOC;
    while(new_len + alloc_len > new_capacity) {
      new_capacity *= 2;
    }
    spooky_string_buffer * new_buffer = buffer->next;
    if(new_buffer == NULL) {
      new_buffer = calloc(1, sizeof * new_buffer);
      buffer->next = new_buffer;
      if(!new_buffer) { goto err0; }
    }

    new_buffer->len = 0;
    if(new_capacity > new_buffer->capacity) {
      new_buffer->capacity = new_capacity;
      char * temp = realloc(new_buffer->strings, new_capacity * sizeof * new_buffer->strings);
      if(!temp) { goto err0; }
      new_buffer->strings = temp;
    }
    
    impl->current_buffer->next = new_buffer;
    impl->current_buffer = new_buffer;
    buffer = new_buffer;
  }

  buffer->string_count++;

  size_t n_len = s_len >= SPOOKY_MAX_STRING_LEN ? SPOOKY_MAX_STRING_LEN : s_len;
  char * offset = buffer->strings + buffer->len;
  buffer->len += n_len + 1;

  assert(n_len > 0);
  memcpy(offset, s, n_len);
  offset[n_len] = '\0';
  *out_len = n_len;

  return offset;

err0:
  abort();
}

char * spooky_hash_print_stats(const spooky_hash_table * self) {
  static const size_t max_buf_len = 1 << 13;
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
  int collisions = 0;
  int reallocs = 0;
  int max_items = 0;

  size_t total_items = 0;
  for(size_t i = 0; i < impl->buckets_limits.len; i++) {
    const spooky_hash_bucket * bucket = &impl->buckets[i];
    total_items += bucket->items_limits.len;
    if(bucket->prime) {
      max_items = (int)bucket->items_limits.len > max_items ? (int)bucket->items_limits.len : max_items;
      if(bucket->items_limits.len > 1) {
        if(bucket->items_limits.reallocs > 0) { reallocs++; };
        for(size_t x = 0; x < bucket->items_limits.len; x++) {
          const spooky_str * outer = &bucket->items[x].atom;
          for(size_t y = 0; y < bucket->items_limits.len; y++) {
            const spooky_str * inner = &bucket->items[y].atom;
            if(outer != inner && outer->hash == inner->hash) {
              collisions++;
              out += snprintf(out, max_buf_len - (size_t)(out - result), "Hash Collisions!\n");
              out += snprintf(out, max_buf_len - (size_t)(out - result), "    X: '%s': len: %lu, hash: %lu\n", outer->str, outer->len, outer->hash);
              out += snprintf(out, max_buf_len - (size_t)(out - result), "    Y: '%s': len: %lu, hash: %lu\n", inner->str, inner->len, inner->hash);
            }
          }
        }
      }
    }
  }

  out += snprintf(out, max_buf_len - (size_t)(out - result), "Average distribution: %f\n", (double)total_items / (double)impl->buckets_limits.len);
  out += snprintf(out, max_buf_len - (size_t)(out - result), "Total chain reallocations: %i\n", reallocs);
  out += snprintf(out, max_buf_len - (size_t)(out - result), "Total key collisions: %i\n", collisions / 2);
  out += snprintf(out, max_buf_len - (size_t)(out - result), "Max bucket chain count: %i\n", max_items);
  out += snprintf(out, max_buf_len - (size_t)(out - result), "Unique keys: %i\n", (int)impl->string_count);

  return result;
}

