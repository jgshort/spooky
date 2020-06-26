#include <assert.h>
#include <stdlib.h>

#include "sp_base.h"

const spooky_base * spooky_base_alloc() {
  return NULL;
}

const spooky_base * spooky_base_init(spooky_base * self) {
  assert(self != NULL);

  self->ctor = &spooky_base_ctor;
  self->dtor = &spooky_base_dtor;
  self->free = &spooky_base_free;
  self->release = &spooky_base_release;

  self->handle_event = &spooky_base_handle_event;
  self->handle_delta = &spooky_base_handle_delta;
  self->render = &spooky_base_render;

  return self;
}

const spooky_base * spooky_base_ctor(const spooky_base * self) {
  return self;
}

const spooky_base * spooky_base_dtor(const spooky_base * self) {
  return self;
}

void spooky_base_free(const spooky_base * self) {
  (void)self;
}

void spooky_base_release(const spooky_base * self) {
  (void)self;
}

void spooky_base_handle_event(const spooky_base * self, SDL_Event * event) {
  (void)self;
  (void)event;
}

void spooky_base_handle_delta(const spooky_base * self, double interpolation) {
  (void)self;
  (void)interpolation;
}

void spooky_base_render(const spooky_base * self, SDL_Renderer * renderer) {
  (void)self;
  (void)renderer;
}

