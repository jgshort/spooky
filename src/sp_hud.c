#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>

#include "sp_gui.h"
#include "sp_base.h"
#include "sp_font.h"
#include "sp_hud.h"

typedef struct spooky_hud_data {
  const spooky_font * font;
  int64_t fps;
  int64_t seconds_since_start;
  double interpolation;
  bool show_hud;
  char padding[7]; /* not portable */
} spooky_hud_data;

const spooky_hud * spooky_hud_init(spooky_hud * self) {
  assert(self != NULL);
  if(!self) { abort(); }

  self = (spooky_hud *)(uintptr_t)spooky_base_init((spooky_base *)(uintptr_t)self);

  self->ctor = &spooky_hud_ctor;
  self->dtor = &spooky_hud_dtor;
  self->free = &spooky_hud_free;
  self->release = &spooky_hud_release;

  self->super.handle_event = &spooky_hud_handle_event;
  self->super.handle_delta = NULL;
  self->super.render = &spooky_hud_render;

  return self;
}

const spooky_hud * spooky_hud_alloc() {
  spooky_hud * self = calloc(1, sizeof * self);
  if(self == NULL) { 
    fprintf(stderr, "Unable to allocate memory.");
    abort();
  }
  return self;
}

const spooky_hud * spooky_hud_acquire() {
  return spooky_hud_init((spooky_hud *)(uintptr_t)spooky_hud_alloc());
}

const spooky_hud * spooky_hud_ctor(const spooky_hud * self, const spooky_font * font) {
  assert(self != NULL);
  spooky_hud_data * data = calloc(1, sizeof * data);
  if(!data) { abort(); }
 
  data->font = font;
  data->show_hud = false;

  ((spooky_hud *)(uintptr_t)self)->data = data;

  return self;
}

const spooky_hud * spooky_hud_dtor(const spooky_hud * self) {
  if(self != NULL) {
    /* self->data->font is not owned; do not release/free data->font */
    free(self->data), ((spooky_hud *)(uintptr_t)self)->data = NULL;
  }
  return self;
}

void spooky_hud_free(const spooky_hud * self) {
  if(self != NULL) {
    free((void *)(uintptr_t)self), self = NULL;
  }
}

void spooky_hud_release(const spooky_hud * self) {
  self->free(self->dtor(self));
}

void spooky_hud_update(const spooky_hud * self, int64_t fps, int64_t seconds_since_start, double interpolation) {
  spooky_hud_data * data = ((const spooky_hud *)self)->data;
  data->fps = fps;
  data->seconds_since_start = seconds_since_start;
  data->interpolation = interpolation;
}

void spooky_hud_handle_event(const spooky_base * self, SDL_Event * event) {
  switch(event->type) {
    case SDL_KEYUP:
      {
        SDL_Keycode sym = event->key.keysym.sym;
        switch(sym) {
          case SDLK_F3: /* show HUD */
            {
              spooky_hud_data * data = ((const spooky_hud *)self)->data;
              data->show_hud = !data->show_hud;
            }
            break;
          default:
            break;
        }
      }
      break;
    default:
      break;
  }

}

void spooky_hud_handle_delta(const spooky_base * self, double interpolation) {
  (void)self;
  (void)interpolation;
}

void spooky_hud_render(const spooky_base * self, SDL_Renderer * renderer) {
  static char hud[1920] = { 0 };

  spooky_hud_data * data = ((const spooky_hud *)self)->data;
  
  if(!data->show_hud) { return; }

  static_assert(sizeof(hud) == 1920, "HUD buffer must be 1920 bytes.");
  
  int mouse_x = 0, mouse_y = 0;
  SDL_GetMouseState(&mouse_x, &mouse_y);

  const spooky_font * font = data->font;

  int hud_out = snprintf(hud, sizeof(hud),
      " TIME: %" PRId64 "\n"
      "  FPS: %" PRId64 "\n"
      "DELTA: %1.5f\n"
      "    X: %i,\n"
      "    Y: %i"
      "\n"
      " FONT: Name   : '%s'\n"
      "       Shadow : %i\n"
      "       Height : %i\n"
      "       Ascent : %i\n"
      "       Descent: %i\n"
      "       M-Dash : %i\n"
      , data->seconds_since_start, data->fps, data->interpolation, mouse_x, mouse_y
      , font->get_name(font)
      , font->get_is_drop_shadow(font)
      , font->get_height(font)
      , font->get_ascent(font)
      , font->get_descent(font)
      , font->get_m_dash(font)
    );

  assert(hud_out > 0 && (size_t)hud_out < sizeof(hud));
  
  const SDL_Point hud_point = { .x = 5, .y = 5 };
  const SDL_Color hud_fore_color = { .r = 255, .g = 255, .b = 255, .a = 255};

  font->write_to_renderer(font, renderer, &hud_point, &hud_fore_color, hud, NULL, NULL);
}

