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

//static const size_t sp_wm_objects_max = 1048576;

typedef struct sp_wm_data {
  // size_t objects_index;
  const sp_context * context;
  // const sp_iter * it;
  const sp_base * active_object;
  // const sp_base ** objects;
} sp_wm_data;

inline static const sp_base * sp_wm_as_base(const sp_wm * self);
static bool sp_wm_handle_event(const sp_base * self, SDL_Event * event);
static void sp_wm_handle_delta(const sp_base * self, const SDL_Event * event, uint64_t last_update_time, double interpolation);
static void sp_wm_render(const sp_base * self, SDL_Renderer * renderer);
static void sp_wm_register_window(sp_wm const * self, const sp_base * object);
//static void sp_wm_activate_window(sp_wm const * self, const sp_base * object);
static int sp_wm_get_max_z_order(const sp_wm * self);
static const sp_base * sp_wm_get_active_object(const sp_wm * self);
static void sp_wm_set_active_object(const sp_wm * self, const sp_base * active_object);
// static const sp_iter * sp_wm_window_iter(sp_wm const * self);

inline static const sp_base * sp_wm_as_base(const sp_wm * self) {
  return (const sp_base *)self;
}

const sp_wm * sp_wm_alloc(void) {
  const sp_wm * self = calloc(1, sizeof * self);
  if(!self) { abort(); }
  return self;
}

const sp_wm * sp_wm_init(sp_wm * self) {
  if(!self) { abort(); }
  self = (sp_wm *)(uintptr_t)sp_base_init((sp_base *)(uintptr_t)self);

  self->as_base = &sp_wm_as_base;
  self->ctor = &sp_wm_ctor;
  self->dtor = &sp_wm_dtor;
  self->free = &sp_wm_free;
  self->release = &sp_wm_release;

  self->super.handle_event = &sp_wm_handle_event;
  self->super.handle_delta = &sp_wm_handle_delta;
  self->super.render = &sp_wm_render;

  self->get_active_object = &sp_wm_get_active_object;
  self->set_active_object = &sp_wm_set_active_object;

  self->register_window = &sp_wm_register_window;

  //.activate_window = &sp_wm_activate_window,
  //.window_iter = &sp_wm_window_iter,

  self->get_max_z_order = &sp_wm_get_max_z_order;

  return self;
}

const sp_wm * sp_wm_acquire(void) {
  return sp_wm_init((sp_wm *)(uintptr_t)sp_wm_alloc());
}

const sp_wm * sp_wm_ctor(const sp_wm * self, const char * name, const sp_context * context) {
  sp_wm_data * data = calloc(1, sizeof * data);
  if(!data) { abort(); }

  SDL_Rect origin = { 0 };
  self->super.ctor((const sp_base *)self, name, origin);
  ((sp_wm *)(uintptr_t)self)->data = data;

  // data->objects = calloc(sp_wm_objects_max, sizeof ** data->objects);
  // data->objects_index = 0;
  data->context = context;
  data->active_object = NULL;

  /* iter depends on self->data */
  //data->it = self->window_iter(self);

  return self;
}

const sp_wm * sp_wm_dtor(const sp_wm * self) {
  if(self) {
    //self->data->it->free(self->data->it);
    /* objects are not owned by this class and must be freed separately */
    /* ...but we need to free our array of pointers-to-objects */
    // free(self->data->objects), ((sp_wm *)(uintptr_t)self)->data->objects = NULL;
    free(self->data), ((sp_wm *)(uintptr_t)self)->data = NULL;
  }

  return self;
}

void sp_wm_free(const sp_wm * self) {
  if(self) {
    sp_wm * me = (sp_wm *)(uintptr_t)self;
    free(me), me = NULL;
  }
}

void sp_wm_release(const sp_wm * self) {
  self->free(self->dtor(self));
}

bool sp_wm_handle_event(const sp_base * self, SDL_Event * event) {
  const sp_iter * it = NULL;
  const sp_ex * ex = NULL;

  errno_t err = self->children_iter(self, &it, &ex);
  if(err != SP_SUCCESS) { goto err0; }

  bool handled = false;

  it->reset(it);
  while(it->next(it)) {
    const sp_base * object = it->current(it);
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

void sp_wm_handle_delta(const sp_base * self, const SDL_Event * event, uint64_t last_update_time, double interpolation) {
  const sp_iter * it = NULL;
  const sp_ex * ex = NULL;

  errno_t err = self->children_iter(self, &it, &ex);
  if(err != SP_SUCCESS) { goto err0; }

  it->reverse(it);
  while(it->next(it)) {
    const sp_base * object = it->current(it);
    if(object != NULL) {
      object->handle_delta(object, event, last_update_time, interpolation);
    }
  }

  return;

err0:
  SP_LOG(SLS_ERROR, "Window Manager Delta: Unable to get child iterator.\n");
}

void sp_wm_render(const sp_base * self, SDL_Renderer * renderer) {
  const sp_iter * it = NULL;
  const sp_ex * ex = NULL;

  errno_t err = self->children_iter(self, &it, &ex);
  if(err != SP_SUCCESS) { goto err0; }

  it->reverse(it);
  while(it->next(it)) {
    const sp_base * object = it->current(it);
    if(object && object->render) {
      object->render(object, renderer);
    }
  }

  return;

err0:
  SP_LOG(SLS_ERROR, "Window Manager Render: Unable to get child iterator.\n");
}

static const sp_base * sp_wm_get_active_object(const sp_wm * self) {
  return self->data->active_object;
}

static void sp_wm_set_active_object(const sp_wm * self, const sp_base * active_object) {
  self->data->active_object = active_object;
}

static void sp_wm_register_window(const sp_wm * self, const sp_base * active_object) {
  sp_wm_data * data = self->data;

  //errno_t (*add_child)(const sp_base * /* self */, const sp_base * /* child */, const sp_ex ** /* ex */);

  const sp_ex * ex = NULL;
  const sp_base * wm_base = self->as_base(self);
  errno_t err = wm_base->add_child(wm_base, active_object, &ex);
  if(err != SP_SUCCESS) { goto err0; }
  /*if(data->objects_index + 1 >= sp_wm_objects_max) {
    fprintf(stderr, "Attempt to register window object over maximum available allocation.\n");
    abort();
    } */

  data->active_object = active_object;
  //data->objects[data->objects_index] = active_object;

  /* Setup initial z-order based on object registration */
  const sp_base * object = active_object;
  object->set_z_order(object, wm_base->get_children_count(wm_base));
  // data->objects_index++;

  SP_LOG(SLS_INFO, "Window Manager Register: Registered object '%s'.\n", wm_base->get_name(wm_base));
  return;

err0:
  SP_LOG(SLS_ERROR, "Window Manager Register: Unable to register active object '%s'\n", active_object->get_name(active_object));
}

/* TODO:
   static void sp_wm_activate_window(const sp_wm * self, const sp_base * active_object) {
   sp_wm_data * data = self->data;

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
   const sp_base * object = data->objects[i];
   if(object != NULL) {
   object->set_z_order(object, i);
   }
   }

// Active window becomes first in series with highest z-order
data->objects[data->objects_index] = active_object;
active_object->set_z_order(active_object, data->objects_index);
}
*/


static int sp_wm_get_max_z_order(const sp_wm * self) {
  return (int)self->as_base(self)->get_children_count(self->as_base(self));
}

/*
   typedef struct sp_window_iter {
   sp_iter super;
   const sp_wm * wm;
   size_t index;
   bool reverse;
   char padding[7];
   } sp_window_iter;

   static bool sp_iter_next(const sp_iter * it) {
   sp_window_iter * wit = (sp_window_iter *)(uintptr_t)it;
   sp_wm_data * data = wit->wm->data;
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

   static const void * sp_iter_current(const sp_iter * it) {
   sp_window_iter * wit = (sp_window_iter *)(uintptr_t)it;
   sp_wm_data * data = wit->wm->data;
   return data->objects[wit->index];
   }

   static void sp_iter_reset(const sp_iter * it) {
   sp_window_iter * wit = (sp_window_iter *)(uintptr_t)it;
   wit->index = 0;
   wit->reverse = false;
   }

   static void sp_iter_free(const sp_iter * it) {
   sp_window_iter * wit = (sp_window_iter *)(uintptr_t)it;
   if(wit != NULL) { free(wit), wit = NULL; }
   }

   static void sp_iter_reverse(const sp_iter * it) {
   sp_window_iter * wit = (sp_window_iter *)(uintptr_t)it;
   wit->index = 0;
   wit->reverse = true;
   }

   static const sp_iter * sp_wm_window_iter(const sp_wm * self) {
   sp_window_iter * wit = calloc(1, sizeof * wit);
   sp_iter * it = (sp_iter *)wit;

   wit->wm = self;
   wit->index = 0;
   wit->reverse = false;

   it->next = &sp_iter_next;
   it->current = &sp_iter_current;
   it->reset = &sp_iter_reset;
   it->reverse = &sp_iter_reverse;
   it->free = &sp_iter_free;

   return it;
   }
   */

