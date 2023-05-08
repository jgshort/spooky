#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>

#include "../include/sp_gui.h"
#include "../include/sp_base.h"
#include "../include/sp_font.h"
#include "../include/sp_context.h"
#include "../include/sp_debug.h"

typedef struct spooky_debug_data {
  const spooky_context * context;
  int64_t fps;
  int64_t seconds_since_start;
  double interpolation;
  bool show_debug;
  char padding[7]; /* not portable */
} spooky_debug_data;

static const spooky_debug spooky_debug_funcs = {
  .as_base = &spooky_debug_as_base,
  .ctor = &spooky_debug_ctor,
  .dtor = &spooky_debug_dtor,
  .free = &spooky_debug_free,
  .release = &spooky_debug_release,
  .super.handle_event = &spooky_debug_handle_event,
  .super.handle_delta = NULL,
  .super.render = &spooky_debug_render
};

const spooky_base * spooky_debug_as_base(const spooky_debug * self) {
  return (const spooky_base *)self;
}

const spooky_debug * spooky_debug_init(spooky_debug * self) {
  assert(self != NULL);
  if(!self) { abort(); }

  self = (spooky_debug *)(uintptr_t)spooky_base_init((spooky_base *)(uintptr_t)self);
  self->as_base = spooky_debug_funcs.as_base;
  self->ctor = spooky_debug_funcs.ctor;
  self->dtor = spooky_debug_funcs.dtor;
  self->free = spooky_debug_funcs.free;
  self->release = spooky_debug_funcs.release;
  self->super.handle_event = spooky_debug_funcs.super.handle_event;
  self->super.render = spooky_debug_funcs.super.render;
  return self;
}

const spooky_debug * spooky_debug_alloc(void) {
  spooky_debug * self = calloc(1, sizeof * self);
  if(self == NULL) {
    fprintf(stderr, "Unable to allocate memory.");
    abort();
  }
  return self;
}

const spooky_debug * spooky_debug_acquire(void) {
  return spooky_debug_init((spooky_debug *)(uintptr_t)spooky_debug_alloc());
}

const spooky_debug * spooky_debug_ctor(const spooky_debug * self, const char * name, const spooky_context * context) {
  assert(self != NULL);

  SDL_Rect rect = { .x = 5, .y = 5, .w = 0, .h = 0 };
  self = (spooky_debug *)(uintptr_t)spooky_base_ctor((spooky_base *)(uintptr_t)self, name, rect);

  spooky_debug_data * data = calloc(1, sizeof * data);
  if(!data) { abort(); }

  data->context = context;
  data->show_debug = false;

  ((spooky_debug *)(uintptr_t)self)->data = data;

  /* initial position */
  const spooky_ex * ex = NULL;
  self->super.set_rect((const spooky_base *)self, &rect, &ex);

  return self;
}

const spooky_debug * spooky_debug_dtor(const spooky_debug * self) {
  if(self != NULL) {
    /* self->data->font is not owned; do not release/free data->font */
    free(self->data), ((spooky_debug *)(uintptr_t)self)->data = NULL;
  }
  return self;
}

void spooky_debug_free(const spooky_debug * self) {
  if(self != NULL) {
    free((void *)(uintptr_t)self), self = NULL;
  }
}

void spooky_debug_release(const spooky_debug * self) {
  self->free(self->dtor(self));
}

void spooky_debug_update(const spooky_debug * self, int64_t fps, int64_t seconds_since_start, double interpolation) {
  spooky_debug_data * data = ((const spooky_debug *)self)->data;
  data->fps = fps;
  data->seconds_since_start = seconds_since_start;
  data->interpolation = interpolation;
}

bool spooky_debug_handle_event(const spooky_base * self, SDL_Event * event) {
  switch(event->type) {
    case SDL_KEYUP:
      {
        SDL_Keycode sym = event->key.keysym.sym;
        switch(sym) {
          case SDLK_F3: /* show HUD */
            {
              spooky_debug_data * data = ((const spooky_debug *)self)->data;
              data->show_debug = !data->show_debug;
              return true;
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
  return false;
}

void spooky_debug_render(const spooky_base * self, SDL_Renderer * renderer) {
  static char debug[1920] = { 0 };

  spooky_debug_data * data = ((const spooky_debug *)self)->data;

  if(!data->show_debug) { return; }

  static_assert(sizeof(debug) == 1920, "HUD buffer must be 1920 bytes.");

  int mouse_x = 0, mouse_y = 0;
  SDL_GetMouseState(&mouse_x, &mouse_y);

  const spooky_font * font = data->context->get_font(data->context);

  int64_t FPS = 60;
  int debug_out = snprintf(debug, sizeof(debug),
      " TIME: %" PRId64 "\n"
      "  FPS: %" PRId64 "\n"
      "DELTA: %1.5f\n"
      "    X: %i,\n"
      "    Y: %i"
      "\n"
      /*
         " FONT: Name   : '%s'\n"
         "       Shadow : %i\n"
         "       Height : %i\n"
         "       Ascent : %i\n"
         "       Descent: %i\n"
         "       M-Dash : %i\n" */
      , data->seconds_since_start, FPS, data->interpolation, mouse_x, mouse_y
      /*, font->get_name(font)
        , font->get_is_drop_shadow(font)
        , font->get_height(font)
        , font->get_ascent(font)
        , font->get_descent(font)
        , font->get_m_dash(font)
        */
      );

  assert(debug_out > 0 && (size_t)debug_out < sizeof(debug));

  const SDL_Point debug_point = { .x = self->get_rect(self)->x, .y = self->get_rect(self)->y };
  const SDL_Color debug_fore_color = { .r = 255, .g = 255, .b = 255, .a = 255};

  assert(debug_out > 0);
  font->write_to_renderer(font, renderer, &debug_point, &debug_fore_color, debug, (size_t)debug_out, NULL, NULL);
}
