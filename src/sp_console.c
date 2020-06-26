#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include "sp_gui.h"
#include "sp_base.h"
#include "sp_console.h"

static const int console_direction = 73;

typedef struct spooky_console_data {
  char * buf;
  char * input;
  SDL_Rect rect;
  int direction;
  bool show_console;
  bool is_animating;
  char padding[2]; /* not portable */
} spooky_console_data;

static void spooky_console_handle_event(const spooky_base * self, SDL_Event * event);
static void spooky_console_handle_delta(const spooky_base * self, double interpolation);
static void spooky_console_render(const spooky_base * self, SDL_Renderer * renderer);

const spooky_console * spooky_console_alloc() {
  spooky_console * self = calloc(1, sizeof * self);
  if(self == NULL) { 
    fprintf(stderr, "Unable to allocate memory.");
    abort();
  }
  return self;
}

const spooky_console * spooky_console_init(spooky_console * self) {
  assert(self != NULL);
  if(!self) { abort(); }

  self = (spooky_console *)(uintptr_t)spooky_base_init((spooky_base *)(uintptr_t)self);

  self->ctor = &spooky_console_ctor;
  self->dtor = &spooky_console_dtor;
  self->free = &spooky_console_free;
  self->release = &spooky_console_release;

  self->super.handle_event = &spooky_console_handle_event;
  self->super.handle_delta = &spooky_console_handle_delta;
  self->super.render = &spooky_console_render;

  return self;
}

const spooky_console * spooky_console_acquire() {
  return spooky_console_init((spooky_console *)(uintptr_t)spooky_console_alloc());
}

const spooky_console * spooky_console_ctor(const spooky_console * self, SDL_Renderer * renderer) {
  assert(self != NULL);
  spooky_console_data * data = calloc(1, sizeof * data);
  if(!data) { abort(); }

  
  data->direction = -console_direction;
  data->rect = (SDL_Rect){.x = 100, .y = -300, .w = 0, .h = 300 };
  data->show_console = false;
  data->is_animating = false;
 
  int w, h;
  SDL_GetRendererOutputSize(renderer, &w, &h);
  data->rect.w = w - 200;

  ((spooky_console *)(uintptr_t)self)->data = data;

  return self;
}

const spooky_console * spooky_console_dtor(const spooky_console * self) {
  if(self) {
    spooky_console_data * data = self->data;
    free(data), data = NULL;
  }

  return self;
}

void spooky_console_free(const spooky_console * self) {
  if(self) {
    free((void *)(uintptr_t)self), self = NULL;
  }
}

void spooky_console_release(const spooky_console * self) {
  self->free(self->dtor(self));
}

void spooky_console_handle_event(const spooky_base * self, SDL_Event * event) {
  spooky_console_data * data = ((const spooky_console *)self)->data;
 
  switch(event->type) {
    case SDL_KEYUP:
      {
        SDL_Keycode sym = event->key.keysym.sym;
        switch(sym) {
          case SDLK_BACKQUOTE: /* show console window */
            {
              bool old_show_console = data->show_console;
              data->show_console = !data->show_console;
              if(data->is_animating) { data->show_console = old_show_console; }
              data->is_animating = data->rect.y + data->rect.h > 0 || data->rect.y + data->rect.h < data->rect.h;
              data->direction = data->direction < 0 ? console_direction : -console_direction;
            }
            break; 
        }
      }
      break;
  }
}

void spooky_console_handle_delta(const spooky_base * self, double interpolation) {
  spooky_console_data * data = ((const spooky_console *)self)->data;

  if(data->show_console) {
    data->rect.y += (int)floor((double)data->direction * interpolation); 

    if(data->rect.y < 0 - data->rect.h) { data->rect.y = 0 - data->rect.h; }
    if(data->rect.y > 0) { data->rect.y = 0; }
  }
}

void spooky_console_render(const spooky_base * self, SDL_Renderer * renderer) {
  spooky_console_data * data = ((const spooky_console *)self)->data;

  if(data->show_console) {
    uint8_t r, g, b, a;
    SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);

    SDL_BlendMode blend_mode;
    SDL_GetRenderDrawBlendMode(renderer, &blend_mode);
   
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 173);
    SDL_RenderFillRect(renderer, &(data->rect));
   
    SDL_SetRenderDrawBlendMode(renderer, blend_mode);
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
  }
}

