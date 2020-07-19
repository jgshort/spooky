#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <memory.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "sp_error.h"
#include "sp_iter.h"
#include "sp_atom.h"
#include "sp_str.h"

static const size_t SPOOKY_ATOM_MAX_STR_LEN = 2048;
static unsigned long spooky_atom_next_id = 0;

typedef struct spooky_atom_impl {
  unsigned long id;
  unsigned long hash;
  const spooky_str * str;
} spooky_atom_impl;

static const spooky_atom spooky_atom_funcs = {
  .ctor = &spooky_atom_ctor,
  .dtor = &spooky_atom_dtor,
  .free = &spooky_atom_free,
  .release = &spooky_atom_release
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
  memcpy(self, &spooky_atom_funcs, sizeof spooky_atom_funcs);
  return self;
}

const spooky_atom * spooky_atom_acquire() {
  return spooky_atom_init((spooky_atom *)(uintptr_t)spooky_atom_alloc());
}

const spooky_atom * spooky_atom_ctor(const spooky_atom * self, const char * str) {
  const spooky_ex * ex = NULL;

  spooky_atom_impl * impl = calloc(1, sizeof * impl);
  if(!impl) { goto err0; }

  spooky_str * p;
  if(spooky_str_alloc(str, strnlen(str, SPOOKY_ATOM_MAX_STR_LEN), &p, NULL)) {
    impl->id = ++spooky_atom_next_id;
    impl->hash = p->hash;
    impl->str = p;
  } else {
    abort();
  }
  
  ((spooky_atom *)(uintptr_t)self)->impl = impl;

  return self;

err0:
  fprintf(stderr, "%s\n", ex->msg);
  abort();
}

const spooky_atom * spooky_atom_dtor(const spooky_atom * self) {
  free(self->impl), ((spooky_atom *)(uintptr_t)self)->impl = NULL;
  return self;
}

void spooky_atom_free(const spooky_atom * self) {
  free((void *)(uintptr_t)self), self = NULL;
}

void spooky_atom_release(const spooky_atom * self) {
  self->free(self->dtor(self));
}

