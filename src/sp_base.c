#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>

#include "sp_error.h"
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
  bool is_focus;
  char padding[7];
} spooky_base_impl;

static void spooky_base_set_z_order(const spooky_base * self, size_t z_order);
static size_t spooky_base_get_z_order(const spooky_base * self);

static const SDL_Rect * spooky_base_get_rect(const spooky_base * self);
static errno_t spooky_base_set_rect(const spooky_base * self, const SDL_Rect * rect, const spooky_ex ** ex);

static int spooky_base_get_x(const spooky_base * self);
static void spooky_base_set_x(const spooky_base * self, int x);
static int spooky_base_get_y(const spooky_base * self);
static void spooky_base_set_y(const spooky_base * self, int y);
static int spooky_base_get_w(const spooky_base * self);
static void spooky_base_set_w(const spooky_base * self, int w);
static int spooky_base_get_h(const spooky_base * self);
static void spooky_base_set_h(const spooky_base * self, int h);

static errno_t spooky_base_children_iter(const spooky_base * self, const spooky_iter ** out_it, const spooky_ex ** ex);
static errno_t spooky_base_add_child(const spooky_base * self, const spooky_base * child, const spooky_ex ** ex);
static errno_t spooky_base_set_rect_relative(const spooky_base * self, const SDL_Rect * from_rect, const spooky_ex ** ex);
static errno_t spooky_base_get_rect_relative(const spooky_base * self, const SDL_Rect * rect, SDL_Rect * out_rect, const spooky_ex ** ex);
static errno_t spooky_base_get_bounds(const spooky_base * self, SDL_Rect * out_bounds, const spooky_ex ** ex);

static bool spooky_base_get_focus(const spooky_base * self);
static void spooky_base_set_focus(const spooky_base * self, bool is_focus);

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

  .get_focus = &spooky_base_get_focus,
  .set_focus = &spooky_base_set_focus
};

const spooky_base * spooky_base_alloc() {
  spooky_base * self = calloc(1, sizeof * self);
  if(!self) { goto err0; } 
    
  return self;

err0:
  fprintf(stderr, "Unable to allocate memory.");
  abort();
}

const spooky_base * spooky_base_init(spooky_base * self) {
  assert(self);
  memcpy(self, &spooky_base_funcs, sizeof spooky_base_funcs);
  return self;
}

const spooky_base * spooky_base_acquire() {
  return spooky_base_init((spooky_base *)(uintptr_t)spooky_base_alloc());
}

const spooky_base * spooky_base_ctor(const spooky_base * self, SDL_Rect origin) {
  errno_t res = SP_FAILURE;
  const spooky_ex * ex = NULL;

  spooky_base_impl * impl = calloc(1, sizeof * impl);
  if(!impl) { goto err0; }
  
  impl->z_order = 0; 
  impl->children = NULL;
  impl->parent = NULL;
  impl->prev = NULL;
  impl->next = NULL;
  impl->rect = (SDL_Rect){ 0 }; 
  impl->origin = origin;

  ((spooky_base *)(uintptr_t)self)->impl = impl;

  if((res = self->children_iter(self, &(impl->it), &ex)) != SP_SUCCESS) { goto err1; };
  assert(impl->it);

  return self;

err1:
  free(impl), impl = NULL;
  ex = &spooky_null_ref_ex;

err0:
  fprintf(stderr, "%s\n", ex->msg);
  abort();
}

const spooky_base * spooky_base_dtor(const spooky_base * self) {
  spooky_base_impl * my = self->impl;
  self->impl->it->free(self->impl->it);
  /* children pointers are owned elsewhere. Likewise for parent, prev, and next */
  free(my->children), my->children = NULL; 
  free(self->impl), ((spooky_base *)(uintptr_t)self)->impl = NULL;
  return self;
}

errno_t spooky_base_add_child(const spooky_base * self, const spooky_base * child, const spooky_ex ** ex) {
  errno_t res = SP_FAILURE;

  spooky_base_impl * impl = self->impl;
  if(!impl->children) {
    impl->children_count = 0;
    impl->children_capacity = 16;
    impl->children = calloc(impl->children_capacity, sizeof * impl->children);
    if(!impl->children) { goto err0; }
  }

  if(impl->children_count + 1 > impl->children_capacity) {
    impl->children_capacity += 16;
    const spooky_base ** temp = realloc(impl->children, impl->children_capacity * sizeof * impl->children);
    if(!temp) { goto err1; }
    impl->children = temp;
  }
  impl->children[impl->children_count] = child;
  impl->children_count++;
  
  child->impl->parent = self;
  if((res = child->set_rect_relative(child, &(impl->rect), ex)) != SP_SUCCESS) { goto err2; };

  return SP_SUCCESS;

err2: /* set_rect_relative failure  */
  return res;

err1: /* realloc failure */
err0: /* calloc failure */
  *ex = &spooky_alloc_ex;
  return SP_FAILURE;
}

void spooky_base_free(const spooky_base * self) {
  free((void *)(uintptr_t)self), self = NULL;
}

void spooky_base_release(const spooky_base * self) {
  self->free(self->dtor(self));
}

errno_t spooky_base_get_rect_relative(const spooky_base * self, const SDL_Rect * from_rect, SDL_Rect * out_rect, const spooky_ex ** ex) {

  assert(self && from_rect && out_rect);

  if(!self || !from_rect || !out_rect) { goto err0; }
  
  spooky_base_impl * impl = self->impl;
  int from_x = impl->origin.x + from_rect->x;
  int from_y = impl->origin.y + from_rect->y;

  out_rect->x = from_x;
  out_rect->y = from_y;
  out_rect->w = impl->origin.w;
  out_rect->h = impl->origin.h;

  return SP_SUCCESS;

err0:
  if(ex) { *ex = &spooky_null_ref_ex; }
  return SP_FAILURE;
}

errno_t spooky_base_set_rect_relative(const spooky_base * self, const SDL_Rect * from_rect, const spooky_ex ** ex) {
  assert(self && from_rect);

  if(!self || !from_rect) { goto err0; }
  
  spooky_base_impl * impl = self->impl;
  int from_x = impl->origin.x + from_rect->x;
  int from_y = impl->origin.y + from_rect->y;
 
  impl->rect.x = from_x;
  impl->rect.y = from_y;

  return SP_SUCCESS;

err0:
  *ex = &spooky_null_ref_ex;
  return SP_FAILURE;
}

errno_t spooky_base_get_bounds(const spooky_base * self, SDL_Rect * out_bounds, const spooky_ex ** ex) {
  assert(self && out_bounds);
  if(!self || !out_bounds) { goto err0; }

  spooky_base_impl * impl = self->impl;
  out_bounds->x = impl->rect.x;
  out_bounds->y = impl->rect.y;
  out_bounds->w = impl->origin.w;
  out_bounds->h = impl->origin.h;

  if(impl->children_count > 0) {
    const spooky_iter * it = self->impl->it;
    it->reset(it);
    while(it->next(it)) {
      const spooky_base * object = it->current(it);
      if(object) {
        SDL_Rect object_origin = object->impl->origin;
        out_bounds->w += object_origin.x + object_origin.w;
        out_bounds->h += object_origin.y + object_origin.h;
      }
    }
  }

  return SP_SUCCESS;

err0:
  if(ex) { *ex = &spooky_null_ref_ex; }
  return SP_FAILURE;
}

void spooky_base_set_z_order(const spooky_base * self, size_t z_order) {
  self->impl->z_order = z_order;
}

size_t spooky_base_get_z_order(const spooky_base * self) {
  return self->impl->z_order;
}

static int spooky_base_z_compare(const void * a, const void * b) {
  const spooky_base_impl * l = (*(const spooky_base * const *)a)->impl;
  const spooky_base_impl * r = (*(const spooky_base * const *)b)->impl;
  if(l->z_order < r->z_order) { return -1; }
  if(l->z_order > r->z_order) { return 1; }
  else { return 0; }
}

void spooky_base_z_sort(const spooky_base ** items, size_t items_count) {
  qsort(items, items_count, sizeof * items, &spooky_base_z_compare);
}

const SDL_Rect * spooky_base_get_rect(const spooky_base * self) {
  return &(self->impl->rect);
}

errno_t spooky_base_set_rect(const spooky_base * self, const SDL_Rect * new_rect, const spooky_ex ** ex) {
  assert(self && new_rect);
  
  if(!self || !new_rect) { goto err0; }

  errno_t res = SP_FAILURE;
  assert(self->impl);
  spooky_base_impl * impl = self->impl;
  SDL_Rect * impl_rect = &(impl->rect);
  impl_rect->x = new_rect->x;
  impl_rect->y = new_rect->y;
  impl_rect->w = new_rect->w;
  impl_rect->h = new_rect->h;
  
  if(impl->children_count > 0) {
    assert(impl->it);
    const spooky_iter * it = impl->it;
    it->reset(it);
    while(it->next(it)) {
      const spooky_base * child = it->current(it);
      assert(child);
      if(child) {
        if((res = child->set_rect_relative(child, impl_rect, ex)) != SP_SUCCESS) { goto err1; }
      }
    }
  }

  return SP_SUCCESS;

err1:
  return res;

err0:
  if(ex) { *ex = &spooky_null_ref_ex; }
  return SP_FAILURE; 
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

bool spooky_base_get_focus(const spooky_base * self) {
  return self->impl->rect.h;
}

void spooky_base_set_focus(const spooky_base * self, bool is_focus) {
    self->impl->is_focus = is_focus;
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
  spooky_children_iter * this_it = (spooky_children_iter *)(uintptr_t)it;
  spooky_base_impl * impl = this_it->base->impl;
  if(!this_it->reverse) {
    ++(this_it->index);
    return this_it->index <= impl->children_count;
  }
  return false;
}

static const void * spooky_iter_current(const spooky_iter * it) {
  spooky_children_iter * this_it = (spooky_children_iter *)(uintptr_t)it;
  spooky_base_impl * impl = this_it->base->impl;
  return impl->children[this_it->index - 1];
}

static void spooky_iter_reset(const spooky_iter * it) {
  spooky_children_iter * this_it = (spooky_children_iter *)(uintptr_t)it;
  this_it->reset = true;
  this_it->index = 0;
  this_it->reverse = false;
}

static void spooky_iter_free(const spooky_iter * it) {
  spooky_children_iter * this_it = (spooky_children_iter *)(uintptr_t)it;
  if(this_it) { free(this_it), this_it = NULL; }
}

static void spooky_iter_reverse(const spooky_iter * it) {
  spooky_children_iter * this_it = (spooky_children_iter *)(uintptr_t)it;
  this_it->reset = true;
  this_it->index = 0;
  this_it->reverse = true;
}

static errno_t spooky_base_children_iter(const spooky_base * self, const spooky_iter ** out_it, const spooky_ex ** ex) {
  spooky_children_iter * this_it = calloc(1, sizeof * this_it);
  if(!this_it) { goto err0; }

  spooky_iter * it = (spooky_iter *)this_it;

  this_it->base = self;
  this_it->index = 0;
  this_it->reverse = false;
  this_it->reset = true;

  it->next = &spooky_iter_next;
  it->current = &spooky_iter_current;
  it->reset = &spooky_iter_reset;
  it->reverse = &spooky_iter_reverse;
  it->free = &spooky_iter_free;
 
  *out_it = it;
  return SP_SUCCESS;

err0:
  if(ex) { *ex = &spooky_null_ref_ex; }
  return SP_FAILURE;
}


