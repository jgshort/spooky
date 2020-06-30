#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include "sp_font.h"
#include "sp_help.h"

typedef struct spooky_help_data {
  const spooky_context * context;
  bool show_help;
  char padding[7]; /* not portable */
} spooky_help_data;

const spooky_help * spooky_help_init(spooky_help * self) {
  assert(self != NULL);
  if(!self) { abort(); }

  self = (spooky_help *)(uintptr_t)spooky_base_init((spooky_base *)(uintptr_t)self);

  self->ctor = &spooky_help_ctor;
  self->dtor = &spooky_help_dtor;
  self->free = &spooky_help_free;
  self->release = &spooky_help_release;

  self->super.handle_event = &spooky_help_handle_event;
  self->super.render = &spooky_help_render;
  self->super.handle_delta = NULL;

  return self;
}

const spooky_help * spooky_help_alloc() {
  spooky_help * self = calloc(1, sizeof * self);
  if(self == NULL) { 
    fprintf(stderr, "Unable to allocate memory.");
    abort();
  }
  return self;
}

const spooky_help * spooky_help_acquire() {
  return spooky_help_init((spooky_help *)(uintptr_t)spooky_help_alloc());
}

const spooky_help * spooky_help_ctor(const spooky_help * self, const spooky_context * context) {
  assert(self != NULL);
  spooky_help_data * data = calloc(1, sizeof * data);
  if(!data) { abort(); }
 
  data->context = context;
  data->show_help = false;

  ((spooky_help *)(uintptr_t)self)->data = data;

  return self;
}

const spooky_help * spooky_help_dtor(const spooky_help * self) {
  if(self != NULL) {
    /* self->data->font is not owned; do not release/free data->font */
    free(self->data), ((spooky_help *)(uintptr_t)self)->data = NULL;
  }
  return self;
}

void spooky_help_free(const spooky_help * self) {
  if(self != NULL) {
    free((void *)(uintptr_t)self), self = NULL;
  }
}

void spooky_help_release(const spooky_help * self) {
  self->free(self->dtor(self));
}

void spooky_help_handle_event(const spooky_base * self, SDL_Event * event) {
  switch(event->type) {
    case SDL_KEYUP:
      {
        SDL_Keycode sym = event->key.keysym.sym;
        switch(sym) {
          case SDLK_F1: /* show help */
          case SDLK_h:
          case SDLK_SLASH:
          case SDLK_QUESTION:
            {
              spooky_help_data * data = ((const spooky_help *)self)->data;
              data->show_help = true;
            }
            break;
          case SDLK_ESCAPE:
            {
              spooky_help_data * data = ((const spooky_help *)self)->data;
              data->show_help = false;
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

void spooky_help_render(const spooky_base * self, SDL_Renderer * renderer) {
  static char help[1920] = { 0 };

  spooky_help_data * data = ((const spooky_help *)self)->data;
  
  if(!data->show_help) { return; }

  static_assert(sizeof(help) == 1920, "HUD buffer must be 1920 bytes.");
  
  int mouse_x = 0, mouse_y = 0;
  SDL_GetMouseState(&mouse_x, &mouse_y);

  const spooky_font * font = data->context->get_font(data->context);

  int help_out = snprintf(help, sizeof(help),
      "PLEASE HELP!"
    );

  assert(help_out > 0 && (size_t)help_out < sizeof(help));
  
  const SDL_Point help_point = { .x = 5, .y = 5 };
  const SDL_Color help_fore_color = { .r = 255, .g = 255, .b = 255, .a = 255};

  font->write_to_renderer(font, renderer, &help_point, &help_fore_color, help, NULL, NULL);
}

