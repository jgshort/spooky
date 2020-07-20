#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "sp_str.h"

static const size_t SPOOKY_STR_MAX_STR_LEN = 2048;

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

errno_t spooky_str_alloc(const char * s, size_t len, const spooky_str ** out_str, const spooky_ex ** ex) {
  assert(s && len > 0 && out_str);
  if(!s || len == 0 || !out_str) { goto err0; }

  /* fallback len verification */
  size_t s_nlen = strnlen(s, SPOOKY_STR_MAX_STR_LEN);
  assert(s_nlen == len);
  if(s_nlen != len) { goto err0; }

  spooky_str * temp = calloc(1, sizeof * temp);
  if(!temp) { goto err1; }

  temp->hash = spooky_hash_str(s);
  temp->len = s_nlen;
  temp->str = calloc(s_nlen, sizeof temp->str[0]);
  if(!temp->str) { goto err2; }

  memcpy(temp->str, s, sizeof temp->str[0] * s_nlen);
  temp->str[s_nlen] = '\0';
  *out_str = temp;
  return SP_SUCCESS;

err2: /* *out_str calloc failure */
  free(temp), temp = NULL;
  if(ex) { *ex = &spooky_alloc_ex; }
  goto err0;

err1: /* (*out_str)->str calloc failure */
  if(ex) { *ex = &spooky_alloc_ex; }
  goto err0;

err0:
  return SP_FAILURE;
}

void spooky_str_free(spooky_str * str) {
  free(str->str), str->str = NULL;
  free(str), str = NULL;
}

