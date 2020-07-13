#include <assert.h>
#include <stdlib.h>

#include "sp_iter.h"
#include "sp_base.h"

typedef struct spooky_base_impl {
  const spooky_iter * it;
  const spooky_base ** children;
  const spooky_base * parent;
  const spooky_base * prev;
  const spooky_base * next;
  
  size_t children_index;
  size_t children_count;
  size_t children_capacity;

  /* the original start location */
  SDL_Rect origin;
  /* the current (displayed) location, which may be based on a parent */
  SDL_Rect rect;

  size_t z_order;
} spooky_base_impl;

static void spooky_base_set_z_order(const spooky_base * self, size_t z_order);
static size_t spooky_base_get_z_order(const spooky_base * self);

static const SDL_Rect * spooky_base_get_rect(const spooky_base * self);
static void spooky_base_set_rect(const spooky_base * self, const SDL_Rect * rect);

static int spooky_base_get_x(const spooky_base * self);
static void spooky_base_set_x(const spooky_base * self, int x);
static int spooky_base_get_y(const spooky_base * self);
static void spooky_base_set_y(const spooky_base * self, int y);
static int spooky_base_get_w(const spooky_base * self);
static void spooky_base_set_w(const spooky_base * self, int w);
static int spooky_base_get_h(const spooky_base * self);
static void spooky_base_set_h(const spooky_base * self, int h);

static const spooky_iter * spooky_base_children_iter(const spooky_base * self);
static void spooky_base_add_child(const spooky_base * self, const spooky_base * child);
static void spooky_base_set_rect_relative(const spooky_base * self, const SDL_Rect * rect);
static SDL_Rect spooky_base_get_rect_relative(const spooky_base * self, const SDL_Rect * rect);
static  SDL_Rect spooky_base_get_bounds(const spooky_base * self);

static const spooky_base spooky_base_funcs = {
  .ctor = &spooky_base_ctor,
  .dtor = &spooky_base_dtor,
  .free = &spooky_base_free,
  .release = &spooky_base_release,

  .handle_event = NULL,
  .handle_delta = NULL,
  .render = NULL,

  .get_rect = &spooky_base_get_rect,
  .set_rect = &spooky_base_set_rect,

  .get_x = &spooky_base_get_x,
  .set_x = &spooky_base_set_x,
  .get_y = &spooky_base_get_y,
  .set_y = &spooky_base_set_y,
  .get_w = &spooky_base_get_w,
  .set_w = &spooky_base_set_w,
  .get_h = &spooky_base_get_h,
  .set_h = &spooky_base_set_h,

  .set_z_order = &spooky_base_set_z_order,
  .get_z_order = &spooky_base_get_z_order,
  .children_iter = &spooky_base_children_iter,
  .add_child = &spooky_base_add_child,
  .set_rect_relative = &spooky_base_set_rect_relative ,
  .get_rect_relative = &spooky_base_get_rect_relative,

  .get_bounds = &spooky_base_get_bounds,
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
  memcpy(self, &spooky_base_funcs, sizeof spooky_base_funcs);
  return self;
}

const spooky_base * spooky_base_acquire() {
  return spooky_base_init((spooky_base *)(uintptr_t)spooky_base_alloc());
}

const spooky_base * spooky_base_ctor(const spooky_base * self, SDL_Rect origin) {
  spooky_base_impl * impl = calloc(1, sizeof * impl);
  impl->z_order = 0; 
  impl->children = NULL;
  impl->parent = NULL;
  impl->prev = NULL;
  impl->next = NULL;
  impl->rect = (SDL_Rect){ 0 }; 
  impl->origin = origin;

  ((spooky_base *)(uintptr_t)self)->impl = impl;

  impl->it = self->children_iter(self);
  assert(impl->it != NULL);

  return self;
}

const spooky_base * spooky_base_dtor(const spooky_base * self) {
  spooky_base_impl * my = self->impl;
  self->impl->it->free(self->impl->it);
  /* children pointers are owned elsewhere. Likewise for parent, prev, and next */
  free(my->children), my->children = NULL; 
  free(self->impl), ((spooky_base *)(uintptr_t)self)->impl = NULL;
  return self;
}

void spooky_base_add_child(const spooky_base * self, const spooky_base * child) {
  if(self->impl->children == NULL) {
    self->impl->children_count = 0;
    self->impl->children_capacity = 16;
    self->impl->children = calloc(self->impl->children_capacity, sizeof * self->impl->children);
  }

  self->impl->children[self->impl->children_count] = child;
  self->impl->children_count++;
  
  child->impl->parent = self;
  child->set_rect_relative(child, &(self->impl->rect));
}

void spooky_base_free(const spooky_base * self) {
  free((void *)(uintptr_t)self), self = NULL;
}

void spooky_base_release(const spooky_base * self) {
  self->free(self->dtor(self));
}

SDL_Rect spooky_base_get_rect_relative(const spooky_base * self, const SDL_Rect * from_rect) {
  int from_x = self->impl->origin.x + from_rect->x;
  int from_y = self->impl->origin.y + from_rect->y;

  return (SDL_Rect){ .x = from_x, .y = from_y, .w = self->impl->origin.w, .h = self->impl->origin.h };
}

void spooky_base_set_rect_relative(const spooky_base * self, const SDL_Rect * from_rect) {
  assert(self != NULL && from_rect != NULL);
  
  int from_x = self->impl->origin.x + from_rect->x;
  int from_y = self->impl->origin.y + from_rect->y;
 
  self->impl->rect.x = from_x;
  self->impl->rect.y = from_y;
}

SDL_Rect spooky_base_get_bounds(const spooky_base * self) {
  SDL_Rect bounds = { .x = self->impl->origin.x, .y = self->impl->origin.x, .w = self->impl->origin.w, .h = self->impl->origin.h };

  if(self->impl->children_count > 0) {
    const spooky_iter * it = self->impl->it;
    it->reset(it);
    while(it->next(it)) {
      const spooky_base * object = it->current(it);
      if(object != NULL) {
        bounds.w += object->get_w(object);
        bounds.h += object->get_h(object);
      }
    }
  }

  return bounds;
}

void spooky_base_set_z_order(const spooky_base * self, size_t z_order) {
  self->impl->z_order = z_order;
}

size_t spooky_base_get_z_order(const spooky_base * self) {
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

void spooky_base_set_rect(const spooky_base * self, const SDL_Rect * rect) {
  self->impl->rect.x = rect->x;
  self->impl->rect.y = rect->y;
  self->impl->rect.w = rect->w;
  self->impl->rect.h = rect->h;

  if(self->impl->children_count > 0) {
    const spooky_iter * it = self->impl->it;
    it->reset(it);
    while(it->next(it)) {
      const spooky_base * object = it->current(it);
      if(object != NULL) {
        object->set_rect_relative(object, &(self->impl->rect));
      }
    }
  }
}

int spooky_base_get_x(const spooky_base * self) {
  return self->impl->rect.x;
}

void spooky_base_set_x(const spooky_base * self, int x) {
  self->impl->rect.x = x;
}

int spooky_base_get_y(const spooky_base * self) {
  return self->impl->rect.y;
}

void spooky_base_set_y(const spooky_base * self, int y) {
  self->impl->rect.y = y;
}

int spooky_base_get_w(const spooky_base * self) {
  return self->impl->rect.w;
}

void spooky_base_set_w(const spooky_base * self, int w) {
  self->impl->rect.w = w;
}

int spooky_base_get_h(const spooky_base * self) {
  return self->impl->rect.h;
}

void spooky_base_set_h(const spooky_base * self, int h) {
  self->impl->rect.h = h;
}

typedef struct spooky_children_iter {
  spooky_iter super;
  const spooky_base * base;
  size_t index;
  bool reset;
  bool reverse;
  char padding[6];
} spooky_children_iter;

static bool spooky_iter_next(const spooky_iter * it) {
  spooky_children_iter * wit = (spooky_children_iter *)(uintptr_t)it;
  spooky_base_impl * impl = wit->base->impl;
  if(!wit->reverse) {
    ++(wit->index);
    return wit->index <= impl->children_count;
  }
  return false;
}


static const void * spooky_iter_current(const spooky_iter * it) {
  spooky_children_iter * wit = (spooky_children_iter *)(uintptr_t)it;
  spooky_base_impl * impl = wit->base->impl;
  return impl->children[wit->index - 1];
}

static void spooky_iter_reset(const spooky_iter * it) {
  spooky_children_iter * wit = (spooky_children_iter *)(uintptr_t)it;
  wit->reset = true;
  wit->index = 0;
  wit->reverse = false;
}

static void spooky_iter_free(const spooky_iter * it) {
  spooky_children_iter * wit = (spooky_children_iter *)(uintptr_t)it;
  if(wit != NULL) { free(wit), wit = NULL; }
}

static void spooky_iter_reverse(const spooky_iter * it) {
  spooky_children_iter * wit = (spooky_children_iter *)(uintptr_t)it;
  wit->reset = true;
  wit->index = 0;
  wit->reverse = true;
}

static const spooky_iter * spooky_base_children_iter(const spooky_base * self) {
  spooky_children_iter * wit = calloc(1, sizeof * wit);
  spooky_iter * it = (spooky_iter *)wit;

  wit->base = self;
  wit->index = 0;
  wit->reverse = false;
  wit->reset = true;

  it->next = &spooky_iter_next;
  it->current = &spooky_iter_current;
  it->reset = &spooky_iter_reset;
  it->reverse = &spooky_iter_reverse;
  it->free = &spooky_iter_free;
  
  return it;
}

