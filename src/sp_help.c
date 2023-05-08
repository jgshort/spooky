#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>

#include "../include/sp_font.h"
#include "../include/sp_help.h"

typedef struct sp_help_impl {
  const sp_context * context;
  bool show_help;
  bool is_active;
  char padding[6]; /* not portable */
} sp_help_impl;

static const sp_help sp_help_funcs = {
  .as_base = &sp_help_as_base,
  .ctor = &sp_help_ctor,
  .dtor = &sp_help_dtor,
  .free = &sp_help_free,
  .release = &sp_help_release,

  .super.handle_event = &sp_help_handle_event,
  .super.render = &sp_help_render,
  .super.handle_delta = NULL
};

const sp_base * sp_help_as_base(const sp_help * self) {
  return (const sp_base *)self;
}

const sp_help * sp_help_init(sp_help * self) {
  assert(self != NULL);
  if(!self) { abort(); }

  self = (sp_help *)(uintptr_t)sp_base_init((sp_base *)(uintptr_t)self);

  self->as_base = sp_help_funcs.as_base;
  self->ctor = sp_help_funcs.ctor;
  self->dtor = sp_help_funcs.dtor;
  self->free = sp_help_funcs.free;
  self->release = sp_help_funcs.release;

  self->super.handle_event = sp_help_funcs.super.handle_event;
  self->super.render = sp_help_funcs.super.render;
  self->super.handle_delta = NULL;

  return self;
}

const sp_help * sp_help_alloc(void) {
  sp_help * self = calloc(1, sizeof * self);
  if(self == NULL) {
    fprintf(stderr, "Unable to allocate memory.");
    abort();
  }
  return self;
}

const sp_help * sp_help_acquire(void) {
  return sp_help_init((sp_help *)(uintptr_t)sp_help_alloc());
}

const sp_help * sp_help_ctor(const sp_help * self, const char * name, const sp_context * context) {
  assert(self != NULL);

  int help_text_w, help_text_h;
  const sp_font * font = context->get_font(context);
  font->measure_text(font, "mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm", strlen("mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm"), &help_text_w, &help_text_h);
  int help_rect_w, help_rect_h;
  SDL_GetRendererOutputSize(context->get_renderer(context), &help_rect_w, &help_rect_h);
  SDL_Rect origin = {
    .x = (help_rect_w / 4) - (help_text_w / 2),
    .y = (help_rect_h / 4) - ((help_text_h * 24) / 2),
    .w = help_text_w,
    .h = help_text_h * 24
  };

  self->super.ctor((const sp_base *)self, name, origin);

  sp_help_impl * impl = calloc(1, sizeof * impl);
  if(!impl) { abort(); }

  impl->context = context;
  impl->show_help = false;
  impl->is_active = false;

  ((sp_help *)(uintptr_t)self)->impl = impl;

  //const sp_ex * ex = NULL;
  //self->super.set_rect((const sp_base *)self, &origin, &ex);
  return self;
}

const sp_help * sp_help_dtor(const sp_help * self) {
  if(self != NULL) {
    free(self->impl), ((sp_help *)(uintptr_t)self)->impl = NULL;
  }
  return self;
}

void sp_help_free(const sp_help * self) {
  if(self != NULL) {
    free((void *)(uintptr_t)self), self = NULL;
  }
}

void sp_help_release(const sp_help * self) {
  self->free(self->dtor(self));
}

bool sp_help_handle_event(const sp_base * self, SDL_Event * event) {
  sp_help_impl * impl = ((const sp_help *)self)->impl;
  switch(event->type) {
    case SDL_KEYDOWN:
      {
        if(!impl->show_help) { return false; }
        SDL_Keycode sym = event->key.keysym.sym;
        switch(sym) {
          case SDLK_ESCAPE: /* hide help */
            impl->show_help = false;
            impl->is_active = false;
            return true;
          default:
            break;
        }
      }
      break;
    case SDL_KEYUP:
      {
        SDL_Keycode sym = event->key.keysym.sym;
        switch(sym) {
          case SDLK_F1: /* show help */
          case SDLK_SLASH:
          case SDLK_QUESTION:
            impl->show_help = true;
            impl->is_active = true;
            return true;
          default:
            break;
        }
      }
      break;
    default:
      break;
  }
  return impl->is_active;
}

void sp_help_render(const sp_base * self, SDL_Renderer * renderer) {
  sp_help_impl * impl = ((const sp_help *)self)->impl;
  if(!impl->show_help) { return; }

  static char help[1920] = { 0 };
  static_assert(sizeof(help) == 1920, "Help buffer must be 1920 bytes.");

  const sp_font * font = impl->context->get_font(impl->context);

  int help_rect_w, help_rect_h;
  SDL_GetRendererOutputSize(renderer, &help_rect_w, &help_rect_h);

  int help_out = snprintf(help, sizeof(help),
      "               > HELP <                 \n"
      " ? or F1 : Help                         \n"
      "                                        \n"
      " Ctrl `  : Debug Console                \n"
      " Ctrl +  : Text size up                 \n"
      " Ctrl -  : Text size down               \n"
      "                                        \n"
      " F3: HUD            F12: Full screen    \n"
      "                                        \n"
      " Esc    : Cancel/Back Out               \n"
      "                                        \n"
      " Ctrl-Q : Quit                          \n"
      "                          [Esc to Close]\n"
      );

  int out_w = 0, out_h = 0;
  static const SDL_Color help_fore_color = { .r = 255, .g = 255, .b = 255, .a = 255};
  const SDL_Point help_point = { .x = help_rect_w / 4, .y = help_rect_h / 4 };
  font->write_to_renderer(font, renderer, &help_point, &help_fore_color, help, (size_t)help_out, &out_w, &out_h);

  /*
     assert(help_out > 0 && (size_t)help_out < sizeof(help));

     int help_text_w, help_text_h;
  //"               > HELP <                 \n"
  font->measure_text(font, "mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm", strlen("mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm"), &help_text_w, &help_text_h);


  int help_w = 0, help_h = 0;
  font->measure_text(font, help, (size_t)help_out, &help_w, &help_h);

  int x_center = help_rect_w / 2;
  int y_center = help_rect_h / 2;
  SDL_Rect origin = {
  .x = x_center - (help_text_w / 2),// - (help_text_w / 2),
  .y = y_center - (help_text_h / 2), //- (help_h / 2),
  .w = help_text_w,
  .h = help_h
  };

  const SDL_Rect * rect = &origin;
  const SDL_Point help_point = { .x = rect->x, .y = rect->y };
  const SDL_Color background_color = { .r = 199, .g = 78, .b = 157, .a = 150 };
  const sp_gui_rgba_context * rgba = sp_gui_push_draw_color(renderer, &background_color);
  {
  SDL_BlendMode blend_mode;
  SDL_GetRenderDrawBlendMode(renderer, &blend_mode);

  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

  SDL_Rect rr = { .x = help_point.x, .y = help_point.y, .w = help_w, .h = help_h };
  SDL_RenderFillRect(renderer, &rr);
  SDL_SetRenderDrawBlendMode(renderer, blend_mode);

  sp_gui_pop_draw_color(rgba);
  }
  */
}
