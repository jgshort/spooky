#include <assert.h>
#include <stdlib.h>

#include "sp_base.h"

typedef struct spooky_base_impl {
  spooky_base interface;
  float z_order;
  char padding[4]; /* not portable */
} spooky_base_impl;

static void spooky_base_set_z_order(const spooky_base * base, float z_order);
static float spooky_base_get_z_order(const spooky_base * base);

const static spooky_base spooky_base_funcs = {
  .ctor = &spooky_base_ctor,
  .dtor = &spooky_base_dtor,
  .free = &spooky_base_free,
  .release = &spooky_base_release,

  .handle_event = &spooky_base_handle_event,
  .handle_delta = &spooky_base_handle_delta,
  .render = &spooky_base_render,

  .set_z_order = &spooky_base_set_z_order,
  .get_z_order = &spooky_base_get_z_order
};

const spooky_base * spooky_base_alloc() {
  return NULL;
}

const spooky_base * spooky_base_init(spooky_base * self) {
  assert(self != NULL);
  return memmove(self, &spooky_base_funcs, sizeof spooky_base_funcs);
}

const spooky_base * spooky_base_ctor(const spooky_base * self) {
  ((spooky_base_impl *)(uintptr_t)self)->z_order = 0; 
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

bool spooky_base_handle_event(const spooky_base * self, SDL_Event * event) {
  (void)self;
  (void)event;
  return false;
}

void spooky_base_handle_delta(const spooky_base * self, int64_t last_update_time, double interpolation) {
  (void)self;
  (void)interpolation;
  (void)last_update_time;
}

void spooky_base_render(const spooky_base * self, SDL_Renderer * renderer) {
  (void)self;
  (void)renderer;
}

void spooky_base_set_z_order(const spooky_base * base, float z_order) {
  ((spooky_base_impl *)(uintptr_t)base)->z_order = z_order;
}

float spooky_base_get_z_order(const spooky_base * base) {
  return ((const spooky_base_impl *)base)->z_order;
}

static int spooky_base_z_compare(const void * a, const void * b) {
  const spooky_base_impl * l = *(const spooky_base_impl * const *)a;
  const spooky_base_impl * r = *(const spooky_base_impl * const *)b;
  if(l->z_order < r->z_order) return -1;
  if(l->z_order > r->z_order) return 1;
  else return 0;
}

void spooky_base_z_sort(const spooky_base ** items, size_t items_count) {
  qsort(items, items_count, sizeof * items, &spooky_base_z_compare);
}

