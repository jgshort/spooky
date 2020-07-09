#include <assert.h>
#include <stdlib.h>

#include "sp_base.h"

typedef struct spooky_base_impl {
  const spooky_base ** children;
  const spooky_base * parent;
  const spooky_base * prev;
  const spooky_base * next;
  SDL_Rect rect;
  float z_order;
  char padding[4]; /* not portable */
} spooky_base_impl;

static void spooky_base_set_z_order(const spooky_base * self, float z_order);
static float spooky_base_get_z_order(const spooky_base * self);

static const SDL_Rect * spooky_base_get_rect(const spooky_base * self);
static void spooky_base_set_x(const spooky_base * self, int x);
static void spooky_base_set_y(const spooky_base * self, int y);
static void spooky_base_set_w(const spooky_base * self, int w);
static void spooky_base_set_h(const spooky_base * self, int h);

static const spooky_base spooky_base_funcs = {
  .ctor = &spooky_base_ctor,
  .dtor = &spooky_base_dtor,
  .free = &spooky_base_free,
  .release = &spooky_base_release,

  .handle_event = &spooky_base_handle_event,
  .handle_delta = &spooky_base_handle_delta,
  .render = &spooky_base_render,

  .get_rect = &spooky_base_get_rect,
  .set_x = &spooky_base_set_x,
  .set_y = &spooky_base_set_y,
  .set_w = &spooky_base_set_w,
  .set_h = &spooky_base_set_h,

  .set_z_order = &spooky_base_set_z_order,
  .get_z_order = &spooky_base_get_z_order
};

const spooky_base * spooky_base_alloc() {
  spooky_base * self = calloc(1, sizeof * self);
  if(self == NULL) { 
    fprintf(stderr, "Unable to allocate memory.");
    abort();
  }
  return self;
}

const spooky_base * spooky_base_init(spooky_base * self) {
  assert(self != NULL);
  memmove(self, &spooky_base_funcs, sizeof spooky_base_funcs);
  return self;
}

const spooky_base * spooky_base_acquire() {
  return spooky_base_init((spooky_base *)(uintptr_t)spooky_base_alloc());
}

const spooky_base * spooky_base_ctor(const spooky_base * self) {
  spooky_base_impl * my = self->impl;
  my->z_order = 0; 
  my->children = NULL;
  my->parent = NULL;
  my->prev = NULL;
  my->next = NULL;

  return self;
}

const spooky_base * spooky_base_dtor(const spooky_base * self) {
  spooky_base_impl * my = self->impl;
  /* children pointers are owned elsewhere. Likewise for parent, prev, and next */
  free(my->children), my->children = NULL; 
  free(self->impl), ((spooky_base *)(uintptr_t)self)->impl = NULL;
  return self;
}

void spooky_base_free(const spooky_base * self) {
  free((void *)(uintptr_t)self), self = NULL;
}

void spooky_base_release(const spooky_base * self) {
  self->free(self->dtor(self));
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

void spooky_base_set_z_order(const spooky_base * self, float z_order) {
  self->impl->z_order = z_order;
}

float spooky_base_get_z_order(const spooky_base * self) {
  return self->impl->z_order;
}

static int spooky_base_z_compare(const void * a, const void * b) {
  const spooky_base * l = *(const spooky_base * const *)a;
  const spooky_base * r = *(const spooky_base * const *)b;
  if(l->impl->z_order < r->impl->z_order) return -1;
  if(l->impl->z_order > r->impl->z_order) return 1;
  else return 0;
}

void spooky_base_z_sort(const spooky_base ** items, size_t items_count) {
  qsort(items, items_count, sizeof * items, &spooky_base_z_compare);
}

const SDL_Rect * spooky_base_get_rect(const spooky_base * self) {
  return &(self->impl->rect);
}

void spooky_base_set_x(const spooky_base * self, int x) {
  self->impl->rect.x = x;
}

void spooky_base_set_y(const spooky_base * self, int y) {
  self->impl->rect.y = y;
}

void spooky_base_set_w(const spooky_base * self, int w) {
  self->impl->rect.w = w;
}

void spooky_base_set_h(const spooky_base * self, int h) {
  self->impl->rect.h = h;
}

