#include <assert.h>
#include <stdlib.h>

#include "sp_base.h"

static void spooky_base_set_z_order(const spooky_base * base, int z_order);
static int spooky_base_get_z_order(const spooky_base * base);

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

  self->set_z_order = &spooky_base_set_z_order;
  self->get_z_order = &spooky_base_get_z_order;

  return self;
}

const spooky_base * spooky_base_ctor(const spooky_base * self) {
  ((spooky_base *)(uintptr_t)self)->z_order = 0; 
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

void spooky_base_set_z_order(const spooky_base * base, int z_order) {
  ((spooky_base *)(uintptr_t)base)->z_order = z_order;
}

int spooky_base_get_z_order(const spooky_base * base) {
  return base->z_order;
}

static int spooky_base_z_compare(const void * a, const void * b) {
  const spooky_base * l = *(const spooky_base * const *)a;
  const spooky_base * r = *(const spooky_base * const *)b;
  if(l->z_order < r->z_order) return -1;
  if(l->z_order == r->z_order) return 0;
  else return 1;
}

void spooky_base_z_sort(const spooky_base ** items, size_t items_count) {
  qsort(items, items_count, sizeof * items, &spooky_base_z_compare);
}

