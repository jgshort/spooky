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
  self->super.handle_delta = &spooky_hud_handle_delta;
  self->super.render = &spooky_hud_render;

  return self;
}

const spooky_hud * spooky_hud_alloc() {
  return NULL;
}

const spooky_hud * spooky_hud_acquire() {
  return NULL;
}

const spooky_hud * spooky_hud_ctor(const spooky_hud * self, const spooky_font * font) {
  (void)self;
  (void)font;
  return self;
}

const spooky_hud * spooky_hud_dtor(const spooky_hud * self) {
  (void)self;
  return self;
}

void spooky_hud_free(const spooky_hud * self) {
  (void)self;
}

void spooky_hud_release(const spooky_hud * self) {
  (void)self;
}

void spooky_hud_update(const spooky_hud * self, int64_t fps, int64_t seconds_since_start, double interpolation) {
  spooky_hud_data * data = ((const spooky_hud *)self)->data;
  data->fps = fps;
  data->seconds_since_start = seconds_since_start;
  data->interpolation = interpolation;
}

void spooky_hud_handle_event(const spooky_base * self, SDL_Event * event) {
  (void)self;
  (void)event;
}

void spooky_hud_handle_delta(const spooky_base * self, double interpolation) {
  (void)self;
  (void)interpolation;
}

void spooky_hud_render(const spooky_base * self, SDL_Renderer * renderer) {
  spooky_hud_data * data = ((const spooky_hud *)self)->data;

  char hud[1920] = { 0 };

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

