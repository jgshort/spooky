#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "sp_error.h"
#include "sp_str.h"

#define SPOOKY_STR_BUFFER_CAPACITY_DEFAULT 1024

static const size_t SPOOKY_STR_MAX_STR_LEN = 2048;

typedef struct spooky_strings {
  size_t len;
  size_t capacity;
  spooky_str * strings;
} spooky_strings;

static spooky_strings strings_buf = { 
  .len = 0,
  .capacity = SPOOKY_STR_BUFFER_CAPACITY_DEFAULT,
  .strings = NULL
};

static bool is_init = false;

void spooky_str_init() {
  assert(strings_buf.capacity > 0);
  strings_buf.strings = calloc(strings_buf.capacity, sizeof * strings_buf.strings); 
  is_init = true;
}

void spooky_str_quit() {
  free(strings_buf.strings), strings_buf.strings = NULL;
  strings_buf.len = 0;
  strings_buf.capacity = SPOOKY_STR_BUFFER_CAPACITY_DEFAULT;
  is_init = false;
}

static spooky_str * spooky_str_get_next() {
  assert(is_init && strings_buf.strings);

  if(strings_buf.len + 1 > strings_buf.capacity) {
    strings_buf.capacity += SPOOKY_STR_BUFFER_CAPACITY_DEFAULT;
    spooky_str * temp = realloc(strings_buf.strings, sizeof * temp * strings_buf.capacity);
    if(!temp) { abort(); }
    strings_buf.strings = temp;
  }
  spooky_str * res = &(strings_buf.strings[strings_buf.len]);
  strings_buf.len++;
  return res;
}

unsigned long spooky_hash_str(const char * restrict str) {
  /* See: http://www.cse.yorku.ca/~oz/hash.html */
  register unsigned long hash = 5381;
  register char c = '\0';

  while((c = *(str++))) {
    // >> hash(i) = hash(i - 1) * 33 ^ str[i]
    hash = ((hash << 5) + hash) + (unsigned long)c; /* hash * 33 + c */
  }

  return hash;
}

errno_t spooky_str_ref(const char * s, size_t len, const spooky_str ** out_str, const spooky_ex ** ex) {
  assert(s && len > 0 && out_str);
  if(!s || len == 0 || !out_str) { goto err0; }

  /* fallback len verification */
  size_t s_nlen = strnlen(s, SPOOKY_STR_MAX_STR_LEN);
  assert(s_nlen == len);
  if(s_nlen != len) { goto err0; }

  spooky_str * temp = spooky_str_get_next();
  if(!temp) { goto err1; }

  temp->hash = spooky_hash_str(s);
  temp->len = s_nlen;
  temp->str = s;

  *out_str = temp;

  return SP_SUCCESS;

err1:
  if(ex) { *ex = &spooky_alloc_ex; }

err0:
  return SP_FAILURE;
}

