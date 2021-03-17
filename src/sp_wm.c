#include <stdlib.h>
#include <assert.h>
#include <memory.h>
#include <stdbool.h>
#include <limits.h>
#include <stdint.h>

#include "sp_font.h"
#include "sp_wm.h"

static const size_t spooky_wm_objects_max = 1048576;

typedef struct spooky_wm_data {
  size_t objects_index;
  const spooky_context * context;
  const spooky_iter * it;
  const spooky_base * active_object;
  const spooky_base ** objects;
} spooky_wm_data;

static bool spooky_wm_handle_event(const spooky_base * self, SDL_Event * event);
static void spooky_wm_handle_delta(const spooky_base * self, int64_t last_update_time, double interpolation);
static void spooky_wm_render(const spooky_base * self, SDL_Renderer * renderer);
static void spooky_wm_register_window(spooky_wm const * self, const spooky_base * object);
static void spooky_wm_activate_window(spooky_wm const * self, const spooky_base * object);
static int spooky_wm_get_max_z_order(const spooky_wm * self);
static const spooky_base * spooky_wm_get_active_object(const spooky_wm * self);
static void spooky_wm_set_active_object(const spooky_wm * self, const spooky_base * active_object);
static const spooky_iter * spooky_wm_window_iter(spooky_wm const * self);

static const spooky_wm spooky_wm_funcs = {
  .ctor = &spooky_wm_ctor,
  .dtor = &spooky_wm_dtor,
  .free = &spooky_wm_free,
  .release = &spooky_wm_release,

  .super.handle_event = &spooky_wm_handle_event,
  .super.handle_delta = &spooky_wm_handle_delta,
  .super.render = &spooky_wm_render,

  .get_active_object = &spooky_wm_get_active_object,
  .set_active_object = &spooky_wm_set_active_object,

  .register_window = &spooky_wm_register_window,
  .activate_window = &spooky_wm_activate_window,
  .window_iter = &spooky_wm_window_iter,

  .get_max_z_order = &spooky_wm_get_max_z_order
};

const spooky_wm * spooky_wm_alloc() {
  const spooky_wm * self = calloc(1, sizeof * self);
  if(!self) { abort(); }
  return self;
}

const spooky_wm * spooky_wm_init(spooky_wm * self) {
  if(!self) { abort(); }
  self = (spooky_wm *)(uintptr_t)spooky_base_init((spooky_base *)self);

  memcpy(self, &spooky_wm_funcs, sizeof spooky_wm_funcs);
  return self;
}

const spooky_wm * spooky_wm_acquire() {
  return spooky_wm_init((spooky_wm *)(uintptr_t)spooky_wm_alloc());
}

const spooky_wm * spooky_wm_ctor(const spooky_wm * self, const spooky_context * context) {
  spooky_wm_data * data = calloc(1, sizeof * data);
  if(!data) { abort(); }

  data->objects = calloc(spooky_wm_objects_max, sizeof * data->objects);
  data->objects_index = 0;
  data->context = context;

  ((spooky_wm *)(uintptr_t)self)->data = data;

  /* iter depends on self->data */
  data->it = self->window_iter(self);

  return self;
}

const spooky_wm * spooky_wm_dtor(const spooky_wm * self) {
  if(self) {
    self->data->it->free(self->data->it);
    /* objects are not owned by this class and must be freed separately */
    /* ...but we need to free our array of pointers-to-objects */
    free(self->data->objects), ((spooky_wm *)(uintptr_t)self)->data->objects = NULL;
    free(self->data), ((spooky_wm *)(uintptr_t)self)->data = NULL;
  }

  return self;
}

void spooky_wm_free(const spooky_wm * self) {
  if(self) {
    spooky_wm * me = (spooky_wm *)(uintptr_t)self;
    free(me), me = NULL;
  }
}

void spooky_wm_release(const spooky_wm * self) {
  self->free(self->dtor(self));
}

bool spooky_wm_handle_event(const spooky_base * self, SDL_Event * event) {
  spooky_wm_data * data = ((const spooky_wm *)(uintptr_t)self)->data;
  const spooky_iter * it = data->it;

  bool handled = false;
  it->reset(it);
  while(it->next(it)) {
    const spooky_base * object = it->current(it);
    if(object != NULL) {
      handled = self == NULL;
      handled = object->handle_event(object, event);
      if(handled) { break; }
    }
  }

  return handled;
}

void spooky_wm_handle_delta(const spooky_base * self, int64_t last_update_time, double interpolation) {
  spooky_wm_data * data = ((const spooky_wm *)(uintptr_t)self)->data;
  const spooky_iter * it = data->it;

  it->reverse(it);
  while(it->next(it)) {
    const spooky_base * object = it->current(it);
    if(object != NULL) {
      object->handle_delta(object, last_update_time, interpolation);
    }
  }
}

void spooky_wm_render(const spooky_base * self, SDL_Renderer * renderer) {
  spooky_wm_data * data = ((const spooky_wm *)(uintptr_t)self)->data;
  const spooky_iter * it = data->it;
  it->reverse(it);
  while(it->next(it)) {
    const spooky_base * object = it->current(it);
    object->render(object, renderer);
  }
}

static const spooky_base * spooky_wm_get_active_object(const spooky_wm * self) {
  return self->data->active_object;
}

static void spooky_wm_set_active_object(const spooky_wm * self, const spooky_base * active_object) {
  self->data->active_object = active_object;
}

static void spooky_wm_register_window(const spooky_wm * self, const spooky_base * active_object) {
  spooky_wm_data * data = self->data;

  if(data->objects_index + 1 > spooky_wm_objects_max - 1) {
    fprintf(stderr, "Attempt to register window object over maximum available allocation.\n");
    abort();
  }

  data->objects[data->objects_index] = active_object;
  /* Setup initial z-order based on object registration */
  const spooky_base * object = active_object;
  object->set_z_order(object, data->objects_index);
  data->objects_index++;
}

static void spooky_wm_activate_window(const spooky_wm * self, const spooky_base * active_object) {
  spooky_wm_data * data = self->data;

  if(data->objects[data->objects_index] == active_object) { return; }

  size_t offset = 0;
  bool found = false;
  for(size_t i = 0; i <= data->objects_index; i++) {
    if(active_object == data->objects[i]) {
      offset = i;
      found = true;
      break;
    }
  }

  if(!found) { return; }

  for(size_t i = offset; i <= data->objects_index; i++) {
    data->objects[i] = data->objects[i + 1];
    const spooky_base * object = data->objects[i];
    if(object != NULL) {
      object->set_z_order(object, i);
    }
  }

  /* Active window becomes first in series with highest z-order */
  data->objects[data->objects_index] = active_object;
  active_object->set_z_order(active_object, data->objects_index);
}

static int spooky_wm_get_max_z_order(const spooky_wm * self) {
  spooky_wm_data * data = self->data;
  assert(data->objects_index < INT_MAX);
  return (int)data->objects_index; 
}

typedef struct spooky_window_iter {
  spooky_iter super;
  const spooky_wm * wm;
  size_t index;
  bool reverse;
  char padding[7];
} spooky_window_iter;

static bool spooky_iter_next(const spooky_iter * it) {
  spooky_window_iter * wit = (spooky_window_iter *)(uintptr_t)it;
  spooky_wm_data * data = wit->wm->data;
  if(wit->reverse) {
    ++wit->index;
    return wit->index <= data->objects_index;
  } else {
     --wit->index;
    return wit->index == 0;
  }
}

static const void * spooky_iter_current(const spooky_iter * it) {
  spooky_window_iter * wit = (spooky_window_iter *)(uintptr_t)it;
  spooky_wm_data * data = wit->wm->data;

  return data->objects[wit->index];
}

static void spooky_iter_reset(const spooky_iter * it) {
  spooky_window_iter * wit = (spooky_window_iter *)(uintptr_t)it;
  wit->index = 0;
  wit->reverse = false;
}

static void spooky_iter_free(const spooky_iter * it) {
  spooky_window_iter * wit = (spooky_window_iter *)(uintptr_t)it;
  if(wit != NULL) { free(wit), wit = NULL; }
}

static void spooky_iter_reverse(const spooky_iter * it) {
  spooky_window_iter * wit = (spooky_window_iter *)(uintptr_t)it;
  wit->index = 0;
  wit->reverse = true;
}

static const spooky_iter * spooky_wm_window_iter(const spooky_wm * self) {
  spooky_window_iter * wit = calloc(1, sizeof * wit);
  spooky_iter * it = (spooky_iter *)wit;

  wit->wm = self;
  wit->index = 0;
  wit->reverse = false;

  it->next = &spooky_iter_next;
  it->current = &spooky_iter_current;
  it->reset = &spooky_iter_reset;
  it->reverse = &spooky_iter_reverse;
  it->free = &spooky_iter_free;

  return it;
}
