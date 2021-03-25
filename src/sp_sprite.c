#include <stdlib.h>
#include <assert.h>
#include <memory.h>
#include <stdbool.h>
#include <stdint.h>

#include "sp_gui.h"
#include "sp_sprite.h"

typedef struct spooky_sprite_data {
  SDL_Texture * texture;

  SDL_Rect src;
  SDL_Rect dest;

  int current_frame;
  int total_frames;

  bool is_visible;
  char padding[7];
} spooky_sprite_data;

// Not utilized yet: static bool spooky_sprite_handle_event(const spooky_base * self, SDL_Event * event);
static void spooky_sprite_handle_delta(const spooky_sprite * self, int64_t last_update_time, double interpolation);
static void spooky_sprite_render(const spooky_sprite * self, SDL_Renderer * renderer);

const spooky_sprite * spooky_sprite_alloc() {
  spooky_sprite * self = calloc(1, sizeof * self);
  if(!self) { abort(); }
  return self;
}

const spooky_sprite * spooky_sprite_init(spooky_sprite * self) {
  assert(self);
  if(!self) { abort(); }

  self = (spooky_sprite *)(uintptr_t)spooky_base_init((spooky_base *)(uintptr_t)self);

  self->ctor = &spooky_sprite_ctor;
  self->dtor = &spooky_sprite_dtor;
  self->free = &spooky_sprite_free;
  self->release = &spooky_sprite_release;

  // Not utilized yet: self->super.handle_event = &spooky_sprite_handle_event;
  self->handle_delta = &spooky_sprite_handle_delta;
  self->render = &spooky_sprite_render;

  return self;
}

const spooky_sprite * spooky_sprite_acquire() {
  return spooky_sprite_init((spooky_sprite * )(uintptr_t)spooky_sprite_alloc());
}

const spooky_sprite * spooky_sprite_ctor(const spooky_sprite * self, SDL_Texture * texture) {
  spooky_sprite_data * data = calloc(1, sizeof * data);
  if(!data) abort();

  SDL_Rect origin = { .x = 0, .y = 0, .w = 0, .h = 0 };
  spooky_base_ctor((spooky_base *)(uintptr_t)self, origin);

  data->texture = texture;
  data->current_frame = 0;
  data->total_frames = 0;
  data->is_visible = true;

  ((spooky_sprite *)(uintptr_t)self)->data = data;

  return self;
}

const spooky_sprite * spooky_sprite_dtor(const spooky_sprite * self) {
  if(self) {
    if(self->data->texture) {
      SDL_DestroyTexture(self->data->texture);
      self->data->texture = NULL;
    }
    free(((spooky_sprite *)(uintptr_t)self)->data), ((spooky_sprite *)(uintptr_t)self)->data = NULL;
  }

  return self;
}

void spooky_sprite_free(const spooky_sprite * self) {
  if(self) {
    free((spooky_sprite *)(uintptr_t)self), self = NULL;
  }
}

void spooky_sprite_release(const spooky_sprite * self) {
  self->free(self->dtor(self));
}

/* Not utilized yet:
static bool spooky_sprite_handle_event(const spooky_sprite * self, SDL_Event * event) {
  (void)self;
  (void)event;
  return false;
}
*/

static void spooky_sprite_handle_delta(const spooky_sprite * self, int64_t last_update_time, double interpolation) {
  spooky_sprite_data * data = self->data;

  (void)last_update_time;
  (void)interpolation;

  data->current_frame++;
  if(data->current_frame >= data->total_frames) {
    data->current_frame = 0;
  }

  (void)interpolation;
}

static void spooky_sprite_render(const spooky_sprite * self, SDL_Renderer * renderer) {
  spooky_sprite_data * data = self->data;

  if(!data->is_visible) { return; }

  SDL_Rect src = data->src;
  SDL_Rect dest = data->dest;

  src.x = src.x + (data->current_frame * src.w);

  SDL_RenderCopy(renderer, data->texture, &src, &dest);
}
