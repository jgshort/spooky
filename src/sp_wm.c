#include <stdlib.h>
#include <assert.h>
#include <memory.h>
#include <stdbool.h>
#include <limits.h>
#include <stdint.h>

#include "../include/sp_error.h"
#include "../include/sp_log.h"
#include "../include/sp_font.h"
#include "../include/sp_wm.h"

//static const size_t spooky_wm_objects_max = 1048576;

typedef struct spooky_wm_data {
  // size_t objects_index;
  const spooky_context * context;
  // const spooky_iter * it;
  const spooky_base * active_object;
  // const spooky_base ** objects;
} spooky_wm_data;

inline static const spooky_base * spooky_wm_as_base(const spooky_wm * self);
static bool spooky_wm_handle_event(const spooky_base * self, SDL_Event * event);
static void spooky_wm_handle_delta(const spooky_base * self, const SDL_Event * event, uint64_t last_update_time, double interpolation);
static void spooky_wm_render(const spooky_base * self, SDL_Renderer * renderer);
static void spooky_wm_register_window(spooky_wm const * self, const spooky_base * object);
//static void spooky_wm_activate_window(spooky_wm const * self, const spooky_base * object);
static int spooky_wm_get_max_z_order(const spooky_wm * self);
static const spooky_base * spooky_wm_get_active_object(const spooky_wm * self);
static void spooky_wm_set_active_object(const spooky_wm * self, const spooky_base * active_object);
// static const spooky_iter * spooky_wm_window_iter(spooky_wm const * self);

inline static const spooky_base * spooky_wm_as_base(const spooky_wm * self) {
  return (const spooky_base *)self;
}

const spooky_wm * spooky_wm_alloc(void) {
  const spooky_wm * self = calloc(1, sizeof * self);
  if(!self) { abort(); }
  return self;
}

const spooky_wm * spooky_wm_init(spooky_wm * self) {
  if(!self) { abort(); }
  self = (spooky_wm *)(uintptr_t)spooky_base_init((spooky_base *)(uintptr_t)self);

  self->as_base = &spooky_wm_as_base;
  self->ctor = &spooky_wm_ctor;
  self->dtor = &spooky_wm_dtor;
  self->free = &spooky_wm_free;
  self->release = &spooky_wm_release;

  self->super.handle_event = &spooky_wm_handle_event;
  self->super.handle_delta = &spooky_wm_handle_delta;
  self->super.render = &spooky_wm_render;

  self->get_active_object = &spooky_wm_get_active_object;
  self->set_active_object = &spooky_wm_set_active_object;

  self->register_window = &spooky_wm_register_window;

  //.activate_window = &spooky_wm_activate_window,
  //.window_iter = &spooky_wm_window_iter,

  self->get_max_z_order = &spooky_wm_get_max_z_order;

  return self;
}

const spooky_wm * spooky_wm_acquire(void) {
  return spooky_wm_init((spooky_wm *)(uintptr_t)spooky_wm_alloc());
}

const spooky_wm * spooky_wm_ctor(const spooky_wm * self, const char * name, const spooky_context * context) {
  spooky_wm_data * data = calloc(1, sizeof * data);
  if(!data) { abort(); }

  SDL_Rect origin = { 0 };
  self->super.ctor((const spooky_base *)self, name, origin);
  ((spooky_wm *)(uintptr_t)self)->data = data;

  // data->objects = calloc(spooky_wm_objects_max, sizeof ** data->objects);
  // data->objects_index = 0;
  data->context = context;
  data->active_object = NULL;

  /* iter depends on self->data */
  //data->it = self->window_iter(self);

  return self;
}

const spooky_wm * spooky_wm_dtor(const spooky_wm * self) {
  if(self) {
    //self->data->it->free(self->data->it);
    /* objects are not owned by this class and must be freed separately */
    /* ...but we need to free our array of pointers-to-objects */
    // free(self->data->objects), ((spooky_wm *)(uintptr_t)self)->data->objects = NULL;
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
  const spooky_iter * it = NULL;
  const spooky_ex * ex = NULL;

  errno_t err = self->children_iter(self, &it, &ex);
  if(err != SP_SUCCESS) { goto err0; }

  bool handled = false;

  it->reset(it);
  while(it->next(it)) {
    const spooky_base * object = it->current(it);
    if(object) {
      /* assigned intentionally... */
      if((handled = object->handle_event(object, event)) == true) {
        break;
      }
    }
  }

  return handled;

err0:
  SP_LOG(SLS_ERROR, "Window Manager Event: Unable to get child iterator.\n");
  return false;
}

void spooky_wm_handle_delta(const spooky_base * self, const SDL_Event * event, uint64_t last_update_time, double interpolation) {
  const spooky_iter * it = NULL;
  const spooky_ex * ex = NULL;

  errno_t err = self->children_iter(self, &it, &ex);
  if(err != SP_SUCCESS) { goto err0; }

  it->reverse(it);
  while(it->next(it)) {
    const spooky_base * object = it->current(it);
    if(object != NULL) {
      object->handle_delta(object, event, last_update_time, interpolation);
    }
  }

  return;

err0:
  SP_LOG(SLS_ERROR, "Window Manager Delta: Unable to get child iterator.\n");
}

void spooky_wm_render(const spooky_base * self, SDL_Renderer * renderer) {
  const spooky_iter * it = NULL;
  const spooky_ex * ex = NULL;

  errno_t err = self->children_iter(self, &it, &ex);
  if(err != SP_SUCCESS) { goto err0; }

  it->reverse(it);
  while(it->next(it)) {
    const spooky_base * object = it->current(it);
    if(object && object->render) {
      object->render(object, renderer);
    }
  }

  return;

err0:
  SP_LOG(SLS_ERROR, "Window Manager Render: Unable to get child iterator.\n");
}

static const spooky_base * spooky_wm_get_active_object(const spooky_wm * self) {
  return self->data->active_object;
}

static void spooky_wm_set_active_object(const spooky_wm * self, const spooky_base * active_object) {
  self->data->active_object = active_object;
}

static void spooky_wm_register_window(const spooky_wm * self, const spooky_base * active_object) {
  spooky_wm_data * data = self->data;

  //errno_t (*add_child)(const spooky_base * /* self */, const spooky_base * /* child */, const spooky_ex ** /* ex */);

  const spooky_ex * ex = NULL;
  const spooky_base * wm_base = self->as_base(self);
  errno_t err = wm_base->add_child(wm_base, active_object, &ex);
  if(err != SP_SUCCESS) { goto err0; }
  /*if(data->objects_index + 1 >= spooky_wm_objects_max) {
    fprintf(stderr, "Attempt to register window object over maximum available allocation.\n");
    abort();
    } */

  data->active_object = active_object;
  //data->objects[data->objects_index] = active_object;

  /* Setup initial z-order based on object registration */
  const spooky_base * object = active_object;
  object->set_z_order(object, wm_base->get_children_count(wm_base));
  // data->objects_index++;

  SP_LOG(SLS_INFO, "Window Manager Register: Registered object '%s'.\n", wm_base->get_name(wm_base));
  return;

err0:
  SP_LOG(SLS_ERROR, "Window Manager Register: Unable to register active object '%s'\n", active_object->get_name(active_object));
}

/* TODO:
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

// Active window becomes first in series with highest z-order
data->objects[data->objects_index] = active_object;
active_object->set_z_order(active_object, data->objects_index);
}
*/


static int spooky_wm_get_max_z_order(const spooky_wm * self) {
  return (int)self->as_base(self)->get_children_count(self->as_base(self));
}

/*
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
   }

   if(wit->index - 1 > wit->index) {
   wit->index = 0;
   return false;
   } else {
   --wit->index;
   }

   return wit->index == 0;
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
   */

