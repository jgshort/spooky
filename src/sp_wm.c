#include <stdlib.h>
#include <assert.h>
#include <memory.h>
#include <stdbool.h>
#include <stdint.h>

#include "sp_font.h"
#include "sp_wm.h"

#define WM_WINDOWS_MAX 65536 

typedef struct spooky_wm_data {
  const spooky_context * context;
  
  int windows_index;
  char padding[4];
  const spooky_iter * it;
  
  const spooky_box * active_box;
  const spooky_box * windows[WM_WINDOWS_MAX];
} spooky_wm_data;

static bool spooky_wm_handle_event(const spooky_base * self, SDL_Event * event);
//TODO: static void spooky_wm_handle_delta(const spooky_base * self, int64_t last_update_time, double interpolation);
static void spooky_wm_render(const spooky_base * self, SDL_Renderer * renderer);

static void spooky_wm_register_window(spooky_wm const * self, const spooky_box * active_box);
static void spooky_wm_activate_window(spooky_wm const * self, const spooky_box * active_box);
static const spooky_iter * spooky_wm_window_iter(spooky_wm const * self);

static int spooky_wm_get_max_z_order(const spooky_wm * self);

static const spooky_box * spooky_wm_get_active_box(const spooky_wm * self);
static void spooky_wm_set_active_box(const spooky_wm * self, const spooky_box * active_box);

const spooky_wm * spooky_wm_alloc() {
  const spooky_wm * self = calloc(1, sizeof * self);
  if(!self) { abort(); }
  return self;
}

const spooky_wm * spooky_wm_init(spooky_wm * self) {
  if(!self) { abort(); }

  self->ctor = &spooky_wm_ctor;
  self->dtor = &spooky_wm_dtor;
  self->free = &spooky_wm_free;
  self->release = &spooky_wm_release;

  self->super.handle_event = &spooky_wm_handle_event;
  self->super.handle_delta = NULL;
  self->super.render = &spooky_wm_render;

  self->get_active_box = &spooky_wm_get_active_box;
  self->set_active_box = &spooky_wm_set_active_box;

  self->register_window = &spooky_wm_register_window;
  self->activate_window = &spooky_wm_activate_window;
  self->window_iter = &spooky_wm_window_iter;

  self->get_max_z_order = &spooky_wm_get_max_z_order;

  return self;
}

const spooky_wm * spooky_wm_acquire() {
  return spooky_wm_init((spooky_wm *)(uintptr_t)spooky_wm_alloc());
}

const spooky_wm * spooky_wm_ctor(const spooky_wm * self, const spooky_context * context) {
  spooky_wm_data * data = calloc(1, sizeof * data);
  if(!data) { abort(); }
 
  memset(data->windows, 0, sizeof data->windows[0] * WM_WINDOWS_MAX);

  data->windows_index = -1;
  data->context = context;

  ((spooky_wm *)(uintptr_t)self)->data = data;

  /* iter depends on self->data */
  data->it = self->window_iter(self);

  return self;
}

const spooky_wm * spooky_wm_dtor(const spooky_wm * self) {
  if(self) {
    self->data->it->free(self->data->it);
    free(self->data), ((spooky_wm *)(uintptr_t)self)->data = NULL;
  }

  return self;
}

void spooky_wm_free(const spooky_wm * self) {
  if(self) {
    spooky_wm * this = (spooky_wm *)(uintptr_t)self;
    free(this), this = NULL;
  }
}

void spooky_wm_release(const spooky_wm * self) {
  self->free(self->dtor(self));
}

bool spooky_wm_handle_event(const spooky_base * self, SDL_Event * event) {
  spooky_wm_data * data = ((const spooky_wm *)(uintptr_t)self)->data;
  const spooky_iter * it = data->it;

  // TODO:
  (void)event;

  bool handled = false;
  it->reset(it);
  while(it->next(it)) {
    const spooky_box * box = it->current(it);
    if(box) {
      handled = self == NULL;
      // TODO: handled = b->handle_event(b, event);
      if(handled) { break; }
    }
  }

  return handled;
}

void spooky_wm_render(const spooky_base * self, SDL_Renderer * renderer) {
  spooky_wm_data * data = ((const spooky_wm *)(uintptr_t)self)->data;
  const spooky_iter * it = data->it;
  (void)renderer;
  it->reverse(it);
  while(it->next(it)) {
    //TODO: const spooky_box * box = it->current(it);
    //TODO: box->_class->render(box, renderer);
  }
}

static const spooky_box * spooky_wm_get_active_box(const spooky_wm * self) {
  return self->data->active_box;
}

static void spooky_wm_set_active_box(const spooky_wm * self, const spooky_box * active_box) {
  self->data->active_box = active_box;
}

static void spooky_wm_register_window(spooky_wm const * self, const spooky_box * active_box) {
  spooky_wm_data * data = self->data;
 
  if(data->windows_index + 1 > WM_WINDOWS_MAX - 1) {
    fprintf(stderr, "Attempt to register window over configured maximum.\n");
    abort();
  }

  data->windows_index++;
  data->windows[data->windows_index] = active_box;
  /* Setup initial z-order based on registration */
  //TODO: const spooky_box * box = active_box;
  //TODO: box->_class->set_z_order(box, data->windows_index);
}

static void spooky_wm_activate_window(spooky_wm const * self, const spooky_box * active_box) {
  spooky_wm_data * data = self->data;

  if(data->windows_index < 0) { return; }
  if(data->windows[data->windows_index] == active_box) { return; }

  int offset = -1;
  for(int i = 0; i <= data->windows_index; i++) {
    if(active_box == data->windows[i]) {
      offset = i;
      break;
    }
  }

  if(offset < 0) { return; }

  for(int i = offset; i <= data->windows_index; i++) {
    data->windows[i] = data->windows[i + 1];
    //TODO:const spooky_box * box = data->windows[i];
    //TODO:if(box) {
    //TODO:  box->_class->set_z_order(box, i);
    //TODO:}
  }
 
  /* Active window becomes first in series with highest z-order */
  data->windows[data->windows_index] = active_box;
  //TODO:active_box->_class->set_z_order(active_box, data->windows_index);
}

static int spooky_wm_get_max_z_order(const spooky_wm * self) {
  spooky_wm_data * data = self->data;
  return data->windows_index; 
}

typedef struct spooky_window_iter {
  spooky_iter _it;
  const spooky_wm * wm;
  int index;
  bool reverse;
  char padding[3];
} spooky_window_iter;

static bool spooky_iter_next(spooky_iter const * it) {
  spooky_window_iter * wit = (spooky_window_iter *)(uintptr_t)it;
  spooky_wm_data * data = wit->wm->data;
  if(wit->reverse) {
    ++wit->index;
    return wit->index <= data->windows_index;
  } else {
     --wit->index;
    return wit->index >= 0;
  }
}

static const void * spooky_iter_current(spooky_iter const * it) {
  spooky_window_iter * wit = (spooky_window_iter *)(uintptr_t)it; 
  spooky_wm_data * data = wit->wm->data;

  return data->windows[wit->index];
}

static void spooky_iter_reset(spooky_iter const * it) {
  spooky_window_iter * wit = (spooky_window_iter *)(uintptr_t)it; 
  spooky_wm_data * data = wit->wm->data;
  wit->reverse = false;
  wit->index = data->windows_index + 1;
}

static void spooky_iter_free(spooky_iter const * it) {
  spooky_window_iter * wit = (spooky_window_iter *)(uintptr_t)it; 

  if(wit) {
    free(wit), wit = NULL;
  }
}

static void spooky_iter_reverse(spooky_iter const * it) {
  spooky_window_iter * wit = (spooky_window_iter *)(uintptr_t)it;

  wit->index = -1;
  wit->reverse = true;
}

static const spooky_iter * spooky_wm_window_iter(spooky_wm const * self) {
  spooky_wm_data * data = self->data;
  spooky_window_iter * wit = calloc(1, sizeof * wit);

  spooky_iter * it = (spooky_iter *)wit;

  wit->wm = self;
  wit->index = data->windows_index + 1;
  wit->reverse = false;

  it->next = &spooky_iter_next;
  it->current = &spooky_iter_current;
  it->reset = &spooky_iter_reset;
  it->reverse = &spooky_iter_reverse;
  it->free = &spooky_iter_free;
  
  return it;
}

