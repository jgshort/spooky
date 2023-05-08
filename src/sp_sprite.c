#include <stdlib.h>
#include <assert.h>
#include <memory.h>
#include <stdbool.h>
#include <stdint.h>

#include "../include/sp_gui.h"
#include "../include/sp_sprite.h"

typedef struct sp_sprite_data {
  SDL_Texture * texture;

  SDL_Rect src;
  SDL_Rect dest;

  int current_sheet;
  int total_sheets;

  int current_frame;
  int total_frames;

  bool is_visible;
  char padding[7];
} sp_sprite_data;

// Not utilized yet: static bool sp_sprite_handle_event(const sp_base * self, SDL_Event * event);
static void sp_sprite_handle_delta(const sp_sprite * self, const SDL_Event * event, uint64_t last_update_time, double interpolation);
static void sp_sprite_render(const sp_sprite * self, SDL_Renderer * renderer, const SDL_Rect * src, const SDL_Rect * dest);

static bool sp_sprite_get_is_visible(const sp_sprite * self);
static void sp_sprite_set_is_visible(const sp_sprite * self, bool is_visible);

static void sp_sprite_set_sheet(const sp_sprite * self, int sheet);
static void sp_sprite_next_sheet(const sp_sprite * self);
static void sp_sprite_prev_sheet(const sp_sprite * self);
static void sp_sprite_validate_current_sheet(const sp_sprite * self);

static void sp_sprite_set_texture(const sp_sprite * self, SDL_Texture * texture);

const sp_sprite * sp_sprite_alloc() {
  sp_sprite * self = calloc(1, sizeof * self);
  if(!self) { abort(); }
  return self;
}

const sp_sprite * sp_sprite_init(sp_sprite * self) {
  assert(self);
  if(!self) { abort(); }

  self->ctor = &sp_sprite_ctor;
  self->dtor = &sp_sprite_dtor;
  self->free = &sp_sprite_free;
  self->release = &sp_sprite_release;

  // Not utilized yet: self->super.handle_event = &sp_sprite_handle_event;
  self->handle_delta = &sp_sprite_handle_delta;
  self->render = &sp_sprite_render;

  self->set_sheet = &sp_sprite_set_sheet;
  self->next_sheet = &sp_sprite_next_sheet;
  self->prev_sheet = &sp_sprite_prev_sheet;

  self->set_is_visible = &sp_sprite_set_is_visible;
  self->get_is_visible = &sp_sprite_get_is_visible;
  self->set_texture = &sp_sprite_set_texture;

  return self;
}

const sp_sprite * sp_sprite_acquire() {
  return sp_sprite_init((sp_sprite * )(uintptr_t)sp_sprite_alloc());
}

const sp_sprite * sp_sprite_ctor(const sp_sprite * self, SDL_Texture * texture) {
  sp_sprite_data * data = calloc(1, sizeof * data);
  if(!data) abort();

  data->texture = texture;
  data->current_frame = 0;
  data->total_frames = 0;
  data->is_visible = true;

  ((sp_sprite *)(uintptr_t)self)->data = data;

  return self;
}

const sp_sprite * sp_sprite_dtor(const sp_sprite * self) {
  if(self) {
    if(self->data->texture) {
      SDL_DestroyTexture(self->data->texture);
      self->data->texture = NULL;
    }
    free(((sp_sprite *)(uintptr_t)self)->data), ((sp_sprite *)(uintptr_t)self)->data = NULL;
  }

  return self;
}

void sp_sprite_free(const sp_sprite * self) {
  if(self) {
    free((sp_sprite *)(uintptr_t)self), self = NULL;
  }
}

void sp_sprite_release(const sp_sprite * self) {
  self->free(self->dtor(self));
}

/* Not utilized yet:
   static bool sp_sprite_handle_event(const sp_sprite * self, SDL_Event * event) {
   (void)self;
   (void)event;
   return false;
   }
   */

static void sp_sprite_handle_delta(const sp_sprite * self, const SDL_Event * event, uint64_t last_update_time, double interpolation) {
  (void)event;
  sp_sprite_data * data = self->data;

  (void)last_update_time;
  (void)interpolation;

  data->current_frame++;
  if(data->current_frame >= data->total_frames) {
    data->current_frame = 0;
  }

  (void)interpolation;
}

static void sp_sprite_render(const sp_sprite * self, SDL_Renderer * renderer, const SDL_Rect * src, const SDL_Rect * dest) {
  sp_sprite_data * data = self->data;

  if(!data->is_visible) { return; }

  //SDL_Rect src = data->src;
  //SDL_Rect dest = data->dest;

  //src.x = src.x + (data->current_frame * src.w);

  SDL_RenderCopy(renderer, data->texture, src, dest);
}

static bool sp_sprite_get_is_visible(const sp_sprite * self) {
  return self->data->is_visible;
}

static void sp_sprite_set_is_visible(const sp_sprite * self, bool is_visible) {
  self->data->is_visible = is_visible;
}

static void sp_sprite_set_sheet(const sp_sprite * self, int sheet) {
  self->data->current_sheet = sheet;
  self->data->current_frame = 0;
}

static void sp_sprite_validate_current_sheet(const sp_sprite * self) {
  if(self->data->current_sheet > self->data->total_sheets - 1) { self->data->current_sheet = self->data->total_sheets - 1; }
  if(self->data->current_sheet < 0) { self->data->current_sheet = 0; }
}

static void sp_sprite_next_sheet(const sp_sprite * self) {
  self->data->current_sheet++;
  sp_sprite_validate_current_sheet(self);
}

static void sp_sprite_prev_sheet(const sp_sprite * self) {
  self->data->current_sheet--;
  sp_sprite_validate_current_sheet(self);
}

static void sp_sprite_set_texture(const sp_sprite * self, SDL_Texture * texture) {
  self->data->texture = texture;
}
