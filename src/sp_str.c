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

#include "../include/sp_error.h"
#include "../include/sp_str.h"
#include "../include/sp_limits.h"

#define SP_STR_BUFFER_CAPACITY_DEFAULT 32768

static const size_t SP_STR_MAX_STR_LEN = sizeof("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.");

/* Reference type:
 * typedef struct sp_str {
 *  size_t len;
 *  size_t ref_count;
 *  unsigned long hash;
 *  const char * str;
 * } sp_str;
 */

void sp_str_copy(sp_str ** dest, const sp_str * src) {
  **dest = *src;
}

void sp_str_swap(sp_str ** left, sp_str ** right) {
  sp_str temp = **left;
  **left = **right;
  **right = temp;
}

/* See: http://www.cse.yorku.ca/~oz/hash.html */
#define SP_HASH_USE_SDBM
inline static uint64_t sp_hash_str_internal(const char * restrict s, size_t s_len) {
  register uint64_t hash;
#ifdef SP_HASH_USE_SDBM
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
#endif /* SP_HASH_USE_SDBM */
  return hash;
}

uint64_t sp_hash_str(const char * restrict s, size_t s_len) {
  return sp_hash_str_internal(s, s_len);
}

errno_t sp_str_new(const char * s, size_t len, sp_str * out_str) {
  assert(s && len > 0 && out_str);

  size_t s_nlen = len >= SP_STR_MAX_STR_LEN ? SP_STR_MAX_STR_LEN : len;
  assert(s_nlen == len);
  if(s_nlen != len) { goto err0; }

  out_str->hash = sp_hash_str_internal(s, s_nlen);
  out_str->len = s_nlen;
  out_str->str = s;

  assert(out_str->str);

  return SP_SUCCESS;

err0:
  return SP_FAILURE;
}

errno_t sp_str_ref(const char * s, size_t len, uint64_t hash, sp_str * out_str) {
  assert(s && len > 0 && out_str);
  if(!s || len == 0 || !out_str) { goto err0; }

  // fallback len verification
  size_t s_nlen = len >= SP_STR_MAX_STR_LEN ? SP_STR_MAX_STR_LEN : len;
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

uint64_t sp_str_get_hash(const sp_str * self) {
  return self->hash;
}

const char * sp_str_get_str(const sp_str * self) {
  return self->str;
}

errno_t sp_str_isspace(int c, bool * out_space) {
  assert(!(NULL == out_space || (EOF != c  && (UCHAR_MAX < c || 0 > c))));

  if(NULL == out_space || (EOF != c  && (UCHAR_MAX < c || 0 > c))) {
    return SP_FAILURE;
  }

  *out_space = isspace((unsigned char)c);

  return SP_SUCCESS;
}

errno_t sp_str_trim(const char * str, size_t str_len, size_t n_max, char ** out_str, size_t * out_str_len) {
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
        if(sp_str_isspace(*start, &space)) { goto err0; }
      }
    } while(space && ++start < end);
  }

  {
    bool space = false;
    do {
      if(end > start) {
        if(sp_str_isspace(*(end - 1),  &space)) { goto err0; }
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

int sp_str_hash_compare(const void * a, const void * b) {
  const sp_str * l = (const sp_str *)a;
  const sp_str * r = (const sp_str *)b;
  if(l->hash < r->hash) return -1;
  if(l->hash > r->hash) return 1;
  return 0;
}

int sp_str_compare(const sp_str * left, const sp_str * right) {
  if(!left) { return -1; }
  if(!right) { return 1; }

  if(!(left->str)) { return -1; }
  if(!(right->str)) { return 1; }

  size_t left_len = left->len, right_len = right->len;
  if(left_len - right_len != 0) {
    assert(left_len < INT_MAX && right_len < INT_MAX);
    if(left_len < INT_MAX && right_len < INT_MAX) {
      return (int)(left_len) - (int)(right_len);
    } else {
      int64_t len_diff = (int64_t)(left_len) - (int64_t)(right_len);
      if(len_diff < 0) { return -1; }
      else if(len_diff > 0) { return 1; }
      else { fprintf(stderr, "Sign error on len comparison.\n"); abort(); }
    }
  }

  uint64_t left_hash = left->hash, right_hash = right->hash;
  if(left_hash > right_hash) { return -1; }
  if(left_hash < right_hash) { return 1; }

  size_t max_len = left_len < right_len ? left_len : right_len;
  if(max_len > SP_MAX_STRING_LEN) { max_len = SP_MAX_STRING_LEN; }

  int diff = strncmp(left->str, right->str, max_len);
  return diff;
}

const char * sp_strcpy(const char * start, const char * end, size_t * text_len) {
  assert(start && end && text_len);
  assert(end >= start);
  assert(*text_len == 0);

  if(end < start) { abort(); }

  ptrdiff_t end_start_diff = end - start;
  assert(end_start_diff >= 0 && end_start_diff < SSIZE_MAX);

  char * temp = NULL;
  if(end_start_diff > 0) {
    *text_len = (size_t)end_start_diff;
    assert(*text_len > 0);

    temp = calloc((*text_len) + 1, sizeof * temp);
    if(!temp) { abort(); }
    assert(temp);

    memmove(temp, start, *text_len);

    temp[*text_len] = '\0';
  } else if(end_start_diff == 0) {
    /* start and end are equal; what do? */
    abort();
  } else {
    abort();
  }

  assert(temp);

  return temp;
}

