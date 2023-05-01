#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>

#include "../include/sp_error.h"
#include "../include/sp_iter.h"
#include "../include/sp_base.h"

typedef struct spooky_base_data {
  const spooky_iter * it;
  const spooky_base ** children;
  const spooky_base * parent;
  const spooky_base * prev;
  const spooky_base * next;

  const char * name;

  size_t children_index;
  size_t children_count;
  size_t children_capacity;

  /* the original start location */
  SDL_Rect origin;
  /* the current (displayed) location, which may be based on a parent */
  SDL_Rect rect;

  size_t z_order;
  bool is_focus;
  bool is_modal;

  char padding[6];
} spooky_base_data;

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

static size_t spooky_base_get_children_count(const spooky_base * self);
static size_t spooky_base_get_children_capacity(const spooky_base * self);
static const spooky_iter * spooky_base_get_iterator(const spooky_base * self);

static errno_t spooky_base_children_iter(const spooky_base * self, const spooky_iter ** out_it, const spooky_ex ** ex);
static errno_t spooky_base_add_child(const spooky_base * self, const spooky_base * child, const spooky_ex ** ex);
static errno_t spooky_base_set_rect_relative(const spooky_base * self, const SDL_Rect * from_rect, const spooky_ex ** ex);
static errno_t spooky_base_get_rect_relative(const spooky_base * self, const SDL_Rect * rect, SDL_Rect * out_rect, const spooky_ex ** ex);
static errno_t spooky_base_get_bounds(const spooky_base * self, SDL_Rect * out_bounds, const spooky_ex ** ex);

static bool spooky_base_get_focus(const spooky_base * self);
static void spooky_base_set_focus(const spooky_base * self, bool is_focus);
static bool spooky_base_get_is_modal(const spooky_base * self);
static void spooky_base_set_is_modal(const spooky_base * self, bool is_modal);

static const char * spooky_base_get_name(const spooky_base * self);

static const spooky_base spooky_base_funcs = {
  .ctor = &spooky_base_ctor,
  .dtor = &spooky_base_dtor,
  .free = &spooky_base_free,
  .release = &spooky_base_release,

  .get_name = &spooky_base_get_name,

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

  .get_children_capacity = &spooky_base_get_children_capacity,
  .get_children_count = &spooky_base_get_children_count,
  .get_iterator = &spooky_base_get_iterator,

  .children_iter = &spooky_base_children_iter,
  .add_child = &spooky_base_add_child,
  .set_rect_relative = &spooky_base_set_rect_relative ,
  .get_rect_relative = &spooky_base_get_rect_relative,

  .get_bounds = &spooky_base_get_bounds,

  .get_focus = &spooky_base_get_focus,
  .set_focus = &spooky_base_set_focus,

  .get_is_modal = &spooky_base_get_is_modal,
  .set_is_modal = &spooky_base_set_is_modal
};

const spooky_base * spooky_base_alloc(void) {
  spooky_base * self = calloc(1, sizeof * self);
  if(!self) { goto err0; }

  return self;

err0:
  fprintf(stderr, "Unable to allocate memory.");
  abort();
}

const spooky_base * spooky_base_init(spooky_base * self) {
  assert(self);
  *self = spooky_base_funcs;
  return self;
}

const spooky_base * spooky_base_acquire(void) {
  return spooky_base_init((spooky_base *)(uintptr_t)spooky_base_alloc());
}

const spooky_base * spooky_base_ctor(const spooky_base * self, const char * name, SDL_Rect origin) {
  errno_t res = SP_FAILURE;
  const spooky_ex * ex = NULL;

  assert(name);

  spooky_base_data * data = calloc(1, sizeof * data);
  if(!data) { goto err0; }

  data->name = name;
  data->z_order = 0;
  data->children = NULL;
  data->parent = NULL;
  data->prev = NULL;
  data->next = NULL;
  data->rect = origin;
  data->origin = origin;
  data->is_focus = false;
  data->is_modal = false;

  ((spooky_base *)(uintptr_t)self)->data = data;

  if((res = self->children_iter(self, &(data->it), &ex)) != SP_SUCCESS) { goto err1; };
  assert(data->it);

  return self;

err1:
  free(data), data = NULL;
  spooky_ex_new(__LINE__, __FILE__, -1, "Out-of-memory exception.", NULL, &ex);
  goto err0;

err0:
  fprintf(stderr, "%s\n", ex->msg);
  abort();
}

const spooky_base * spooky_base_dtor(const spooky_base * self) {
  spooky_base_data * my = self->data;
  self->data->it->free(self->data->it);
  /* children pointers are owned elsewhere. Likewise for parent, prev, and next */
  free(my->children), my->children = NULL;
  free(self->data), ((spooky_base *)(uintptr_t)self)->data = NULL;
  return self;
}

static const char * spooky_base_get_name(const spooky_base * self) {
  return self->data->name;
}

errno_t spooky_base_add_child(const spooky_base * self, const spooky_base * child, const spooky_ex ** ex) {
  errno_t res = SP_FAILURE;

  spooky_base_data * data = self->data;
  if(!data->children) {
    data->children_count = 0;
    data->children_capacity = 16;
    data->children = calloc(data->children_capacity, sizeof ** data->children);
    if(!data->children) { goto err0; }
  }

  if(data->children_count + 1 > data->children_capacity) {
    data->children_capacity += 16;
    const spooky_base ** temp = realloc(data->children, data->children_capacity * sizeof ** data->children);
    if(!temp) { goto err1; }
    data->children = temp;
  }

  data->children[data->children_count] = child;
  data->children_count++;

  child->data->parent = self;
  if((res = child->set_rect_relative(child, &(data->rect), ex)) != SP_SUCCESS) { goto err2; };

  return SP_SUCCESS;

err2: /* set_rect_relative failure  */
  return res;

err1: /* realloc failure */
err0: /* calloc failure */
  spooky_ex_new(__LINE__, __FILE__, -1, "Out-of-memory exception.", NULL, ex);
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

  spooky_base_data * data = self->data;
  int from_x = data->origin.x + from_rect->x;
  int from_y = data->origin.y + from_rect->y;

  out_rect->x = from_x;
  out_rect->y = from_y;
  out_rect->w = data->origin.w;
  out_rect->h = data->origin.h;

  return SP_SUCCESS;

err0:
  spooky_ex_new(__LINE__, __FILE__, -1, "Out-of-memory exception.", NULL, ex);
  return SP_FAILURE;
}

errno_t spooky_base_set_rect_relative(const spooky_base * self, const SDL_Rect * from_rect, const spooky_ex ** ex) {
  assert(self && from_rect);

  if(!self || !from_rect) { goto err0; }

  spooky_base_data * data = self->data;
  int from_x = data->origin.x + from_rect->x;
  int from_y = data->origin.y + from_rect->y;

  data->rect.x = from_x;
  data->rect.y = from_y;

  return SP_SUCCESS;

err0:
  spooky_ex_new(__LINE__, __FILE__, -1, "Out-of-memory exception.", NULL, ex);
  return SP_FAILURE;
}

errno_t spooky_base_get_bounds(const spooky_base * self, SDL_Rect * out_bounds, const spooky_ex ** ex) {
  assert(self && out_bounds);
  if(!self || !out_bounds) { goto err0; }

  spooky_base_data * data = self->data;
  out_bounds->x = data->rect.x;
  out_bounds->y = data->rect.y;
  out_bounds->w = data->origin.w;
  out_bounds->h = data->origin.h;

  if(data->children_count > 0) {
    const spooky_iter * it = self->data->it;
    it->reset(it);
    while(it->next(it)) {
      const spooky_base * object = it->current(it);
      if(object) {
        SDL_Rect object_origin = object->data->origin;
        out_bounds->w += object_origin.x + object_origin.w;
        out_bounds->h += object_origin.y + object_origin.h;
      }
    }
  }

  return SP_SUCCESS;

err0:
  spooky_ex_new(__LINE__, __FILE__, -1, "Out-of-memory exception.", NULL, ex);
  return SP_FAILURE;
}

void spooky_base_set_z_order(const spooky_base * self, size_t z_order) {
  self->data->z_order = z_order;
}

size_t spooky_base_get_z_order(const spooky_base * self) {
  return self->data->z_order;
}

static int spooky_base_z_compare(const void * a, const void * b) {
  const spooky_base_data * l = (*(const spooky_base * const *)a)->data;
  const spooky_base_data * r = (*(const spooky_base * const *)b)->data;
  if(l->z_order < r->z_order) { return -1; }
  if(l->z_order > r->z_order) { return 1; }
  else { return 0; }
}

void spooky_base_z_sort(const spooky_base ** items, size_t items_count) {
  qsort(items, items_count, sizeof * items, &spooky_base_z_compare);
}

const SDL_Rect * spooky_base_get_rect(const spooky_base * self) {
  return &(self->data->rect);
}

errno_t spooky_base_set_rect(const spooky_base * self, const SDL_Rect * new_rect, const spooky_ex ** ex) {
  assert(self && new_rect);

  if(!self || !new_rect) { goto err0; }

  errno_t res = SP_FAILURE;
  assert(self->data);
  spooky_base_data * data = self->data;
  SDL_Rect * data_rect = &(data->rect);
  data_rect->x = new_rect->x;
  data_rect->y = new_rect->y;
  data_rect->w = new_rect->w;
  data_rect->h = new_rect->h;

  if(data->children_count > 0) {
    assert(data->it);
    const spooky_iter * it = data->it;
    it->reset(it);
    while(it->next(it)) {
      const spooky_base * child = it->current(it);
      if(child) {
        if((res = child->set_rect_relative(child, data_rect, ex)) != SP_SUCCESS) { goto err1; }
      }
    }
  }

  return SP_SUCCESS;

err1:
  return res;

err0:
  spooky_ex_new(__LINE__, __FILE__, -1, "Out-of-memory exception.", NULL, ex);
  return SP_FAILURE;
}

int spooky_base_get_x(const spooky_base * self) {
  return self->data->rect.x;
}

void spooky_base_set_x(const spooky_base * self, int x) {
  self->data->rect.x = x;
}

int spooky_base_get_y(const spooky_base * self) {
  return self->data->rect.y;
}

void spooky_base_set_y(const spooky_base * self, int y) {
  self->data->rect.y = y;
}

int spooky_base_get_w(const spooky_base * self) {
  return self->data->rect.w;
}

void spooky_base_set_w(const spooky_base * self, int w) {
  self->data->rect.w = w;
}

int spooky_base_get_h(const spooky_base * self) {
  return self->data->rect.h;
}

void spooky_base_set_h(const spooky_base * self, int h) {
  self->data->rect.h = h;
}

bool spooky_base_get_focus(const spooky_base * self) {
  return self->data->is_focus;
}

void spooky_base_set_focus(const spooky_base * self, bool is_focus) {
    self->data->is_focus = is_focus;
}

static bool spooky_base_get_is_modal(const spooky_base * self) {
  return self->data->is_modal;
}

static void spooky_base_set_is_modal(const spooky_base * self, bool is_modal) {
  self->data->is_modal = is_modal;
}

static size_t spooky_base_get_children_count(const spooky_base * self) {
  return self->data->children_count;
}

static size_t spooky_base_get_children_capacity(const spooky_base * self) {
  return self->data->children_capacity;
}

static const spooky_iter * spooky_base_get_iterator(const spooky_base * self) {
  return self->data->it;
}

typedef struct spooky_children_iter {
  spooky_iter super;
  const spooky_base * base;
  int64_t index;
  bool reverse;
  char padding[7];
} spooky_children_iter;

static bool spooky_iter_next(const spooky_iter * it) {
  spooky_children_iter * this_it = (spooky_children_iter *)(uintptr_t)it;
  const spooky_base_data * data = this_it->base->data;

  if(this_it->index < 0 || data->children_count <= 0) {
    return false;
  }

  if(this_it->reverse) {
    --this_it->index;
    if(this_it->index < 0) { this_it->index = -1; }
    return this_it->index >= 0;
  }

  assert(data->children_count <= INT64_MAX);
  ++this_it->index;
  return this_it->index <= (int64_t)data->children_count;
}

static const void * spooky_iter_current(const spooky_iter * it) {
  spooky_children_iter * this_it = (spooky_children_iter *)(uintptr_t)it;
  spooky_base_data * data = this_it->base->data;
  if(data->children_count == 0 || this_it->index < 0) {
    return NULL;
  }
  return data->children[this_it->index];
}

static void spooky_iter_reset(const spooky_iter * it) {
  spooky_children_iter * this_it = (spooky_children_iter *)(uintptr_t)it;
  this_it->reverse = false;

  size_t * children_count = &(this_it->base->data->children_count);
  if(*children_count == 0) {
    this_it->index = -1;
  } else {
    assert(*children_count >= 0);
    this_it->index = 0;
  }
}

static void spooky_iter_reverse(const spooky_iter * it) {
  spooky_children_iter * this_it = (spooky_children_iter *)(uintptr_t)it;
  this_it->reverse = true;

  size_t * children_count = &(this_it->base->data->children_count);
  if(*children_count == 0) {
    this_it->index = -1;
  } else {
    assert(*children_count >= 0);
    this_it->index = (int64_t)*children_count;
    assert(this_it->index >= 0);
  }
}

static void spooky_iter_free(const spooky_iter * it) {
  spooky_children_iter * this_it = (spooky_children_iter *)(uintptr_t)it;
  free(this_it), this_it = NULL;
}

static errno_t spooky_base_children_iter(const spooky_base * self, const spooky_iter ** out_it, const spooky_ex ** ex) {
  spooky_children_iter * this_it = calloc(1, sizeof * this_it);
  if(!this_it) { goto err0; }

  spooky_iter * it = (spooky_iter *)this_it;

  this_it->base = self;
  this_it->index = self->data->children_count > 0 ? 0 : -1;
  this_it->reverse = false;

  it->next = &spooky_iter_next;
  it->current = &spooky_iter_current;
  it->reset = &spooky_iter_reset;
  it->reverse = &spooky_iter_reverse;
  it->free = &spooky_iter_free;

  *out_it = it;

  return SP_SUCCESS;

err0:
  spooky_ex_new(__LINE__, __FILE__, -1, "Out-of-memory exception.", NULL, ex);
  return SP_FAILURE;
}

