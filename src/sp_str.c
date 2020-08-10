#ifdef __STDC_ALLOC_LIB__
#define __STDC_WANT_LIB_EXT2__ 1
#else
#define _POSIX_C_SOURCE 200809L
#endif

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <stddef.h>
#include <errno.h>

#include "sp_error.h"
#include "sp_str.h"
#include "sp_limits.h"

#define SPOOKY_STR_BUFFER_CAPACITY_DEFAULT 32768 

static const size_t SPOOKY_STR_MAX_STR_LEN = sizeof("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.");

/* Reference type:
 * typedef struct spooky_str {
 *  size_t len;
 *  size_t ref_count;
 *  unsigned long hash;
 *  const char * str;
 * } spooky_str;
 */

void spooky_str_copy(spooky_str ** dest, const spooky_str * src) {
  **dest = *src;
}

void spooky_str_swap(spooky_str ** left, spooky_str ** right) {
  spooky_str temp = **left;
  **left = **right;
  **right = temp;
}

/* See: http://www.cse.yorku.ca/~oz/hash.html */
#define SPOOKY_HASH_USE_SDBM
inline static uint64_t spooky_hash_str_internal(const char * restrict s, size_t s_len) {
  register uint64_t hash;
#ifdef SPOOKY_HASH_USE_SDBM
  /* use SDBM algorithm: */

# define HASH_DEF hash = (((uint64_t)*(s++)) + /* good: 65599; better: */ 65587 * hash)
  hash = 0;
  if (s_len > 0) {
		register uint64_t loop = (s_len + 8 - 1) >> 3;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-default"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
		switch(s_len & (8 - 1)) {
		case 0:
      do {
			  HASH_DEF;
        case 7: HASH_DEF;
        case 6: HASH_DEF;
        case 5: HASH_DEF;
        case 4: HASH_DEF;
        case 3: HASH_DEF;
        case 2: HASH_DEF;
        case 1: HASH_DEF;
			} while (--loop);
		}
#pragma GCC diagnostic pop
#pragma GCC diagnostic pop
	}
# undef HASH_DEF

#else
  /* use djb2 algorithm: */
  (void)s_len;
  hash = 5381;
  register char c = '\0';

  while((c = *(s++))) {
    hash = ((hash << 5) + hash) + (uint64_t)c; /* hash * 33 + c */
  }
#endif /* SPOOKY_HASH_USE_SDBM */
  return hash;
}

uint64_t spooky_hash_str(const char * restrict s, size_t s_len) {
  return spooky_hash_str_internal(s, s_len);
}

errno_t spooky_str_new(const char * s, size_t len, spooky_str * out_str) {
  assert(s && len > 0 && out_str);

  size_t s_nlen = len >= SPOOKY_STR_MAX_STR_LEN ? SPOOKY_STR_MAX_STR_LEN : len;
  assert(s_nlen == len);
  if(s_nlen != len) { goto err0; }

  out_str->hash = spooky_hash_str_internal(s, s_nlen);
  out_str->len = s_nlen;
  out_str->str = s;

  assert(out_str->str);

  return SP_SUCCESS;

err0:
  return SP_FAILURE;
}

errno_t spooky_str_ref(const char * s, size_t len, uint64_t hash, spooky_str * out_str) {
  assert(s && len > 0 && out_str);
  if(!s || len == 0 || !out_str) { goto err0; }

  // fallback len verification
  size_t s_nlen = len >= SPOOKY_STR_MAX_STR_LEN ? SPOOKY_STR_MAX_STR_LEN : len;
  assert(s_nlen == len);
  if(s_nlen != len) { goto err0; }

  out_str->hash = hash;
  out_str->len = s_nlen;
  out_str->str = s;

  assert(out_str->str);

  return SP_SUCCESS;

err0:
  return SP_FAILURE;
}

uint64_t spooky_str_get_hash(const spooky_str * self) {
  return self->hash;
}

const char * spooky_str_get_str(const spooky_str * self) {
  return self->str;
}

errno_t spooky_str_isspace(int c, bool * out_space) {
  assert(!(NULL == out_space || (EOF != c  && (UCHAR_MAX < c || 0 > c))));

  if(NULL == out_space || (EOF != c  && (UCHAR_MAX < c || 0 > c))) {
    return SP_FAILURE; 
  }

  *out_space = isspace((unsigned char)c);

  return SP_SUCCESS;
}

errno_t spooky_str_trim(const char * str, size_t str_len, size_t n_max, char ** out_str, size_t * out_str_len) {
  assert(!(n_max < 1 || !out_str || !out_str_len));

  if(n_max < 1 || !out_str || !out_str_len) { goto err0; }

  char * temp = NULL;
  if(!str) { goto happy; }

  size_t len = strnlen(str, n_max);
  assert(len <= str_len);

  const char * start = str;
  const char * end = str + len;

  assert(start && end);

  {
    bool space = false;
    do {
      if(start < end) {
        if(spooky_str_isspace(*start, &space)) { goto err0; }
      }
    } while(space && ++start < end);
  }

  {
    bool space = false;
    do {
      if(end > start) {
        if(spooky_str_isspace(*(end - 1),  &space)) { goto err0; }
      }
    } while(space && --end > start);
  }

  ptrdiff_t diff = end - start;
  if(diff > 0) {
    size_t new_len = (size_t)diff;
    if(new_len > n_max) { new_len = n_max; }

    errno = 0;
    if(new_len + 1 > SIZE_MAX / sizeof * temp) { goto err0; }
    temp = calloc(new_len + 1, sizeof * temp);
    if(!temp) { goto err1; }

    assert(start >= str);
    memmove(temp, start, new_len);
    temp[new_len] = '\0';
  } else {
    errno = 0;
    if(diff == 0) {
      temp = strndup("", 1);
    } else {
      temp = strndup(str, n_max);
    }
    if(!temp) { goto err1; }
  }

happy:
  if(!temp) {
    *out_str_len = 0;
  } else {
    assert(diff >= 0);
    *out_str_len = (size_t)diff;
  }
  *out_str = temp;

  return SP_SUCCESS;

err1:
  /* handle errno */

err0:
  return SP_FAILURE;
}

int spooky_str_hash_compare(const void * a, const void * b) {
  const spooky_str * l = (const spooky_str *)a;
  const spooky_str * r = (const spooky_str *)b;
  if(l->hash < r->hash) return -1;
  if(l->hash > r->hash) return 1;
  return 0;
}


int spooky_str_compare(const spooky_str * left, const spooky_str * right) {
  if(!left && !right) { return 0; }
  if(!left) { return -1; }
  if(!right) { return 1; }

  if(left->len < right->len) { return -1; }
  else if(left->len > right->len) { return 1; }
  if(left->hash < right->hash) { return -1; }
  else if(left->hash > right->hash) { return 1; }

  if(!(left->str) && !(right->str)) { return 0; }
  if(!(left->str)) { return -1; }
  if(!(right->str)) { return 1; }

  size_t max_len = left->len < right->len ? left->len : right->len;
  if(max_len > SPOOKY_MAX_STRING_LEN) { max_len = SPOOKY_MAX_STRING_LEN; }

  if(left->len == right->len && left->hash == right->hash && strncmp(left->str, right->str, max_len) == 0) { return 0; }

  return 1;
}

