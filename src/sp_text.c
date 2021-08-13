#include <assert.h>
#include <stdlib.h>

#include "sp_text.h"

typedef struct spooky_text_data {
  int foo;
} spooky_text_data;

const spooky_base * spooky_text_as_base(const spooky_text * self) {
  return &(self->super);
}

const spooky_text * spooky_text_init(spooky_text * self) {
  assert(self);
  if(!self) { abort(); }

  self->ctor = &spooky_text_ctor;
  self->dtor = &spooky_text_dtor;
  self->free = &spooky_text_free;
  self->release = &spooky_text_release;
  /*
  self->handle_event = &spooky_text_handle_event;
  self->handle_delta = &spooky_text_handle_delta;
  self->render = &spooky_text_render;
  */

  return self;
}

const spooky_text * spooky_text_alloc() {
  spooky_text * self = calloc(1, sizeof * self);
  if(!self) { abort(); }
  return self;
}

const spooky_text * spooky_text_acquire() {
  return spooky_text_init((spooky_text * )(uintptr_t)spooky_text_alloc());
}

const spooky_text * spooky_text_ctor(const spooky_text * self, const spooky_context * context, SDL_Renderer * renderer) {
  spooky_text_data * data = calloc(1, sizeof * data);
  if(!data) abort();

  (void)context;
  (void)renderer;

  ((spooky_text *)(uintptr_t)self)->data = data;

  return self;
}

const spooky_text * spooky_text_dtor(const spooky_text * self) {
  if(self) {
    free(self->data), ((spooky_text *)(uintptr_t)self)->data = NULL;
  }
  return self;
}

void spooky_text_free(const spooky_text * self) {
  free((spooky_text *)(uintptr_t)self), self = NULL;
}

void spooky_text_release(const spooky_text * self) {
  self->free(self->dtor(self));
}
