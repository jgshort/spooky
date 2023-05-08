#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>

#include "../include/sp_error.h"
#include "../include/sp_iter.h"
#include "../include/sp_base.h"

typedef struct sp_base_data {
  const sp_iter * it;
  const sp_base ** children;
  const sp_base * parent;
  const sp_base * prev;
  const sp_base * next;

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
} sp_base_data;

static void sp_base_set_z_order(const sp_base * self, size_t z_order);
static size_t sp_base_get_z_order(const sp_base * self);

static const SDL_Rect * sp_base_get_rect(const sp_base * self);
static errno_t sp_base_set_rect(const sp_base * self, const SDL_Rect * rect, const sp_ex ** ex);

static int sp_base_get_x(const sp_base * self);
static void sp_base_set_x(const sp_base * self, int x);
static int sp_base_get_y(const sp_base * self);
static void sp_base_set_y(const sp_base * self, int y);
static int sp_base_get_w(const sp_base * self);
static void sp_base_set_w(const sp_base * self, int w);
static int sp_base_get_h(const sp_base * self);
static void sp_base_set_h(const sp_base * self, int h);

static size_t sp_base_get_children_count(const sp_base * self);
static size_t sp_base_get_children_capacity(const sp_base * self);
static const sp_iter * sp_base_get_iterator(const sp_base * self);

static errno_t sp_base_children_iter(const sp_base * self, const sp_iter ** out_it, const sp_ex ** ex);
static errno_t sp_base_add_child(const sp_base * self, const sp_base * child, const sp_ex ** ex);
static errno_t sp_base_set_rect_relative(const sp_base * self, const SDL_Rect * from_rect, const sp_ex ** ex);
static errno_t sp_base_get_rect_relative(const sp_base * self, const SDL_Rect * rect, SDL_Rect * out_rect, const sp_ex ** ex);
static errno_t sp_base_get_bounds(const sp_base * self, SDL_Rect * out_bounds, const sp_ex ** ex);

static bool sp_base_get_focus(const sp_base * self);
static void sp_base_set_focus(const sp_base * self, bool is_focus);
static bool sp_base_get_is_modal(const sp_base * self);
static void sp_base_set_is_modal(const sp_base * self, bool is_modal);

static const char * sp_base_get_name(const sp_base * self);

static const sp_base sp_base_funcs = {
  .ctor = &sp_base_ctor,
  .dtor = &sp_base_dtor,
  .free = &sp_base_free,
  .release = &sp_base_release,

  .get_name = &sp_base_get_name,

  .handle_event = NULL,
  .handle_delta = NULL,
  .render = NULL,

  .get_rect = &sp_base_get_rect,
  .set_rect = &sp_base_set_rect,

  .get_x = &sp_base_get_x,
  .set_x = &sp_base_set_x,
  .get_y = &sp_base_get_y,
  .set_y = &sp_base_set_y,
  .get_w = &sp_base_get_w,
  .set_w = &sp_base_set_w,
  .get_h = &sp_base_get_h,
  .set_h = &sp_base_set_h,

  .set_z_order = &sp_base_set_z_order,
  .get_z_order = &sp_base_get_z_order,

  .get_children_capacity = &sp_base_get_children_capacity,
  .get_children_count = &sp_base_get_children_count,
  .get_iterator = &sp_base_get_iterator,

  .children_iter = &sp_base_children_iter,
  .add_child = &sp_base_add_child,
  .set_rect_relative = &sp_base_set_rect_relative ,
  .get_rect_relative = &sp_base_get_rect_relative,

  .get_bounds = &sp_base_get_bounds,

  .get_focus = &sp_base_get_focus,
  .set_focus = &sp_base_set_focus,

  .get_is_modal = &sp_base_get_is_modal,
  .set_is_modal = &sp_base_set_is_modal
};

const sp_base * sp_base_alloc(void) {
  sp_base * self = calloc(1, sizeof * self);
  if(!self) { goto err0; }

  return self;

err0:
  fprintf(stderr, "Unable to allocate memory.");
  abort();
}

const sp_base * sp_base_init(sp_base * self) {
  assert(self);
  *self = sp_base_funcs;
  return self;
}

const sp_base * sp_base_acquire(void) {
  return sp_base_init((sp_base *)(uintptr_t)sp_base_alloc());
}

const sp_base * sp_base_ctor(const sp_base * self, const char * name, SDL_Rect origin) {
  errno_t res = SP_FAILURE;
  const sp_ex * ex = NULL;

  assert(name);

  sp_base_data * data = calloc(1, sizeof * data);
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

  ((sp_base *)(uintptr_t)self)->data = data;

  if((res = self->children_iter(self, &(data->it), &ex)) != SP_SUCCESS) { goto err1; };
  assert(data->it);

  return self;

err1:
  free(data), data = NULL;
  sp_ex_new(__LINE__, __FILE__, -1, "Out-of-memory exception.", NULL, &ex);
  goto err0;

err0:
  fprintf(stderr, "%s\n", ex->msg);
  abort();
}

const sp_base * sp_base_dtor(const sp_base * self) {
  sp_base_data * my = self->data;
  self->data->it->free(self->data->it);
  /* children pointers are owned elsewhere. Likewise for parent, prev, and next */
  free(my->children), my->children = NULL;
  free(self->data), ((sp_base *)(uintptr_t)self)->data = NULL;
  return self;
}

static const char * sp_base_get_name(const sp_base * self) {
  return self->data->name;
}

errno_t sp_base_add_child(const sp_base * self, const sp_base * child, const sp_ex ** ex) {
  errno_t res = SP_FAILURE;

  sp_base_data * data = self->data;
  if(!data->children) {
    data->children_count = 0;
    data->children_capacity = 16;
    data->children = calloc(data->children_capacity, sizeof ** data->children);
    if(!data->children) { goto err0; }
  }

  if(data->children_count + 1 > data->children_capacity) {
    data->children_capacity += 16;
    const sp_base ** temp = realloc(data->children, data->children_capacity * sizeof ** data->children);
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
  sp_ex_new(__LINE__, __FILE__, -1, "Out-of-memory exception.", NULL, ex);
  return SP_FAILURE;
}

void sp_base_free(const sp_base * self) {
  free((void *)(uintptr_t)self), self = NULL;
}

void sp_base_release(const sp_base * self) {
  self->free(self->dtor(self));
}

errno_t sp_base_get_rect_relative(const sp_base * self, const SDL_Rect * from_rect, SDL_Rect * out_rect, const sp_ex ** ex) {
  assert(self && from_rect && out_rect);

  if(!self || !from_rect || !out_rect) { goto err0; }

  sp_base_data * data = self->data;
  int from_x = data->origin.x + from_rect->x;
  int from_y = data->origin.y + from_rect->y;

  out_rect->x = from_x;
  out_rect->y = from_y;
  out_rect->w = data->origin.w;
  out_rect->h = data->origin.h;

  return SP_SUCCESS;

err0:
  sp_ex_new(__LINE__, __FILE__, -1, "Out-of-memory exception.", NULL, ex);
  return SP_FAILURE;
}

errno_t sp_base_set_rect_relative(const sp_base * self, const SDL_Rect * from_rect, const sp_ex ** ex) {
  assert(self && from_rect);

  if(!self || !from_rect) { goto err0; }

  sp_base_data * data = self->data;
  int from_x = data->origin.x + from_rect->x;
  int from_y = data->origin.y + from_rect->y;

  data->rect.x = from_x;
  data->rect.y = from_y;

  return SP_SUCCESS;

err0:
  sp_ex_new(__LINE__, __FILE__, -1, "Out-of-memory exception.", NULL, ex);
  return SP_FAILURE;
}

errno_t sp_base_get_bounds(const sp_base * self, SDL_Rect * out_bounds, const sp_ex ** ex) {
  assert(self && out_bounds);
  if(!self || !out_bounds) { goto err0; }

  sp_base_data * data = self->data;
  out_bounds->x = data->rect.x;
  out_bounds->y = data->rect.y;
  out_bounds->w = data->origin.w;
  out_bounds->h = data->origin.h;

  if(data->children_count > 0) {
    const sp_iter * it = self->data->it;
    it->reset(it);
    while(it->next(it)) {
      const sp_base * object = it->current(it);
      if(object) {
        SDL_Rect object_origin = object->data->origin;
        out_bounds->w += object_origin.x + object_origin.w;
        out_bounds->h += object_origin.y + object_origin.h;
      }
    }
  }

  return SP_SUCCESS;

err0:
  sp_ex_new(__LINE__, __FILE__, -1, "Out-of-memory exception.", NULL, ex);
  return SP_FAILURE;
}

void sp_base_set_z_order(const sp_base * self, size_t z_order) {
  self->data->z_order = z_order;
}

size_t sp_base_get_z_order(const sp_base * self) {
  return self->data->z_order;
}

static int sp_base_z_compare(const void * a, const void * b) {
  const sp_base_data * l = (*(const sp_base * const *)a)->data;
  const sp_base_data * r = (*(const sp_base * const *)b)->data;
  if(l->z_order < r->z_order) { return -1; }
  if(l->z_order > r->z_order) { return 1; }
  else { return 0; }
}

void sp_base_z_sort(const sp_base ** items, size_t items_count) {
  qsort(items, items_count, sizeof * items, &sp_base_z_compare);
}

const SDL_Rect * sp_base_get_rect(const sp_base * self) {
  return &(self->data->rect);
}

errno_t sp_base_set_rect(const sp_base * self, const SDL_Rect * new_rect, const sp_ex ** ex) {
  assert(self && new_rect);

  if(!self || !new_rect) { goto err0; }

  errno_t res = SP_FAILURE;
  assert(self->data);
  sp_base_data * data = self->data;
  SDL_Rect * data_rect = &(data->rect);
  data_rect->x = new_rect->x;
  data_rect->y = new_rect->y;
  data_rect->w = new_rect->w;
  data_rect->h = new_rect->h;

  if(data->children_count > 0) {
    assert(data->it);
    const sp_iter * it = data->it;
    it->reset(it);
    while(it->next(it)) {
      const sp_base * child = it->current(it);
      if(child) {
        if((res = child->set_rect_relative(child, data_rect, ex)) != SP_SUCCESS) { goto err1; }
      }
    }
  }

  return SP_SUCCESS;

err1:
  return res;

err0:
  sp_ex_new(__LINE__, __FILE__, -1, "Out-of-memory exception.", NULL, ex);
  return SP_FAILURE;
}

int sp_base_get_x(const sp_base * self) {
  return self->data->rect.x;
}

void sp_base_set_x(const sp_base * self, int x) {
  self->data->rect.x = x;
}

int sp_base_get_y(const sp_base * self) {
  return self->data->rect.y;
}

void sp_base_set_y(const sp_base * self, int y) {
  self->data->rect.y = y;
}

int sp_base_get_w(const sp_base * self) {
  return self->data->rect.w;
}

void sp_base_set_w(const sp_base * self, int w) {
  self->data->rect.w = w;
}

int sp_base_get_h(const sp_base * self) {
  return self->data->rect.h;
}

void sp_base_set_h(const sp_base * self, int h) {
  self->data->rect.h = h;
}

bool sp_base_get_focus(const sp_base * self) {
  return self->data->is_focus;
}

void sp_base_set_focus(const sp_base * self, bool is_focus) {
  self->data->is_focus = is_focus;
}

static bool sp_base_get_is_modal(const sp_base * self) {
  return self->data->is_modal;
}

static void sp_base_set_is_modal(const sp_base * self, bool is_modal) {
  self->data->is_modal = is_modal;
}

static size_t sp_base_get_children_count(const sp_base * self) {
  return self->data->children_count;
}

static size_t sp_base_get_children_capacity(const sp_base * self) {
  return self->data->children_capacity;
}

static const sp_iter * sp_base_get_iterator(const sp_base * self) {
  return self->data->it;
}

typedef struct sp_children_iter {
  sp_iter super;
  const sp_base * base;
  int64_t index;
  bool reverse;
  char padding[7];
} sp_children_iter;

static bool sp_iter_next(const sp_iter * it) {
  sp_children_iter * this_it = (sp_children_iter *)(uintptr_t)it;
  const sp_base_data * data = this_it->base->data;

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

static const void * sp_iter_current(const sp_iter * it) {
  sp_children_iter * this_it = (sp_children_iter *)(uintptr_t)it;
  sp_base_data * data = this_it->base->data;
  if(data->children_count == 0 || this_it->index < 0) {
    return NULL;
  }
  return data->children[this_it->index];
}

static void sp_iter_reset(const sp_iter * it) {
  sp_children_iter * this_it = (sp_children_iter *)(uintptr_t)it;
  this_it->reverse = false;

  size_t * children_count = &(this_it->base->data->children_count);
  if(*children_count == 0) {
    this_it->index = -1;
  } else {
    this_it->index = 0;
  }
}

static void sp_iter_reverse(const sp_iter * it) {
  sp_children_iter * this_it = (sp_children_iter *)(uintptr_t)it;
  this_it->reverse = true;

  size_t * children_count = &(this_it->base->data->children_count);
  if(*children_count == 0) {
    this_it->index = -1;
  } else {
    this_it->index = (int64_t)*children_count;
    assert(this_it->index >= 0);
  }
}

static void sp_iter_free(const sp_iter * it) {
  sp_children_iter * this_it = (sp_children_iter *)(uintptr_t)it;
  free(this_it), this_it = NULL;
}

static errno_t sp_base_children_iter(const sp_base * self, const sp_iter ** out_it, const sp_ex ** ex) {
  sp_children_iter * this_it = calloc(1, sizeof * this_it);
  if(!this_it) { goto err0; }

  sp_iter * it = (sp_iter *)this_it;

  this_it->base = self;
  this_it->index = self->data->children_count > 0 ? 0 : -1;
  this_it->reverse = false;

  it->next = &sp_iter_next;
  it->current = &sp_iter_current;
  it->reset = &sp_iter_reset;
  it->reverse = &sp_iter_reverse;
  it->free = &sp_iter_free;

  *out_it = it;

  return SP_SUCCESS;

err0:
  sp_ex_new(__LINE__, __FILE__, -1, "Out-of-memory exception.", NULL, ex);
  return SP_FAILURE;
}

