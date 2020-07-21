#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <memory.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "sp_limits.h"
#include "sp_error.h"
#include "sp_iter.h"
#include "sp_atom.h"
#include "sp_str.h"

static unsigned long spooky_atom_next_id = 0;

typedef struct spooky_atom_impl {
  unsigned long id;
  unsigned long hash;
  size_t ref_count;
  const spooky_str * str;
} spooky_atom_impl;

static unsigned long spooky_atom_get_id(const spooky_atom * self);
static unsigned long spooky_atom_get_hash(const spooky_atom * self);
static const spooky_str * spooky_atom_get_str(const spooky_atom * self);
static size_t spooky_atom_get_ref_count(const spooky_atom * self);
static void spooky_atom_inc_ref_count(const spooky_atom * self);
static void spooky_atom_dec_ref_count(const spooky_atom * self);
 
static const spooky_atom spooky_atom_funcs = {
  .ctor = &spooky_atom_ctor,
  .dtor = &spooky_atom_dtor,
  .free = &spooky_atom_free,
  .release = &spooky_atom_release,
  .get_id = &spooky_atom_get_id,
  .get_hash = &spooky_atom_get_hash,
  .get_str = &spooky_atom_get_str,
  .get_ref_count = &spooky_atom_get_ref_count,
  .inc_ref_count =  &spooky_atom_inc_ref_count,
  .dec_ref_count =  &spooky_atom_dec_ref_count
};

const spooky_atom * spooky_atom_alloc() {
  spooky_atom * self = calloc(1, sizeof * self);
  if(!self) { goto err0; } 
    
  return self;

err0:
  fprintf(stderr, "Unable to allocate memory.");
  abort();
}

const spooky_atom * spooky_atom_init(spooky_atom * self) {
  assert(self);
  memmove(self, &spooky_atom_funcs, sizeof spooky_atom_funcs);
  return self;
}

const spooky_atom * spooky_atom_acquire() {
  return spooky_atom_init((spooky_atom *)(uintptr_t)spooky_atom_alloc());
}

const spooky_atom * spooky_atom_ctor(const spooky_atom * self, const char * str) {
  const spooky_ex * ex = NULL;
  
  spooky_atom_impl * impl = calloc(1, sizeof * impl);
  if(!impl) { goto err0; }
 
  impl->id = 0;
  impl->hash = 0;
  impl->ref_count = 0;
  impl->str = NULL;
  
  if(str) {
    const spooky_str * p;
    if(spooky_str_alloc(str, strnlen(str, SPOOKY_MAX_STRING_LEN), &p, &ex) != SP_SUCCESS) { goto err1; }

    ++spooky_atom_next_id;
    impl->id = spooky_atom_next_id;
    impl->hash = p->hash;
    impl->str = p;
  }

  ((spooky_atom *)(uintptr_t)self)->impl = impl;

  return self;

err1:
  free(impl), impl = NULL;

err0:
  if(ex) { fprintf(stderr, "%s\n", ex->msg); }
  abort();
}

const spooky_atom * spooky_atom_dtor(const spooky_atom * self) {
  if(self->impl->str) {
    spooky_str_free((spooky_str *)(uintptr_t)self->impl->str);
  }

  free(self->impl), ((spooky_atom *)(uintptr_t)self)->impl = NULL;
  return self;
}

void spooky_atom_free(const spooky_atom * self) {
  free((void *)(uintptr_t)self), self = NULL;
}

void spooky_atom_release(const spooky_atom * self) {
  self->free(self->dtor(self));
}

unsigned long spooky_atom_get_id(const spooky_atom * self) {
  return self->impl->id;
}

unsigned long spooky_atom_get_hash(const spooky_atom * self) {
  return self->impl->hash;
}

const spooky_str * spooky_atom_get_str(const spooky_atom * self) {
  return self->impl->str;
}

size_t spooky_atom_get_ref_count(const spooky_atom * self) {
  return self->impl->ref_count;
}

void spooky_atom_inc_ref_count(const spooky_atom * self) {
  self->impl->ref_count++;
}

void spooky_atom_dec_ref_count(const spooky_atom * self) {
  self->impl->ref_count--;
}

