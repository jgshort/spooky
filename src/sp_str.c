#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "sp_error.h"
#include "sp_str.h"

#define SPOOKY_STR_BUFFER_CAPACITY_DEFAULT 32768 

static const size_t SPOOKY_STR_MAX_STR_LEN = sizeof("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.");

typedef struct spooky_strings {
  size_t len;
  size_t capacity;
  spooky_str * strings;
} spooky_strings;

static spooky_strings * strings_buf = NULL;

static bool is_init = false;

void spooky_str_init() {
  strings_buf = calloc(1, sizeof * strings_buf);
  assert(strings_buf);

  strings_buf->len = 0;
  strings_buf->capacity = SPOOKY_STR_BUFFER_CAPACITY_DEFAULT;
  strings_buf->strings = calloc(strings_buf->capacity, sizeof * strings_buf->strings); 
  assert(strings_buf->capacity > 0);

  is_init = true;
}

void spooky_str_quit() {
  strings_buf->len = 0;
  strings_buf->capacity = SPOOKY_STR_BUFFER_CAPACITY_DEFAULT;
  free(strings_buf->strings), strings_buf->strings = NULL;
  free(strings_buf), strings_buf = NULL;
  is_init = false;
}

static spooky_str * spooky_str_get_next() {
  assert(is_init && strings_buf->strings && strings_buf->capacity > 0);
  if(strings_buf->len + 1 > strings_buf->capacity) {
    strings_buf->capacity *= 2;
    spooky_str * temp = realloc(strings_buf->strings, (sizeof * temp) * strings_buf->capacity);
    if(!temp) { abort(); }
    strings_buf->strings = temp;
  }
  spooky_str * res = &(strings_buf->strings[strings_buf->len]);
  res->ordinal = strings_buf->len;
  strings_buf->len++;
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

errno_t spooky_str_ref(const char * s, size_t len, spooky_str * out_str, const spooky_ex ** ex) {
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

  assert(temp->str);

  *out_str = *temp;

  return SP_SUCCESS;

err1:
  if(ex) { *ex = &spooky_alloc_ex; }

err0:
  return SP_FAILURE;
}

unsigned long spooky_str_get_id(const spooky_str * self) {
  return self->ordinal;
}

unsigned long spooky_str_get_hash(const spooky_str * self) {
  return self->hash;
}

const char * spooky_str_get_str(const spooky_str * self) {
  return self->str;
}

size_t spooky_str_get_ref_count(const spooky_str * self) {
  return self->ref_count;
}

void spooky_str_inc_ref_count(spooky_str * self) {
  self->ref_count++;
}

void spooky_str_dec_ref_count(spooky_str * self) {
  self->ref_count--;
}

