#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include "sp_font.h"
#include "sp_help.h"

typedef struct spooky_help_impl {
  const spooky_context * context;
  bool show_help;
  char padding[7]; /* not portable */
} spooky_help_impl;

static const spooky_help spooky_help_funcs = {
  .as_base = &spooky_help_as_base,
  .ctor = &spooky_help_ctor,
  .dtor = &spooky_help_dtor,
  .free = &spooky_help_free,
  .release = &spooky_help_release,

  .super.handle_event = &spooky_help_handle_event,
  .super.render = &spooky_help_render,
  .super.handle_delta = NULL
};

const spooky_base * spooky_help_as_base(const spooky_help * self) {
  return (const spooky_base *)self;
}

const spooky_help * spooky_help_init(spooky_help * self) {
  assert(self != NULL);
  if(!self) { abort(); }

  self = (spooky_help *)(uintptr_t)spooky_base_init((spooky_base *)(uintptr_t)self);

  self->as_base = spooky_help_funcs.as_base;
  self->ctor = spooky_help_funcs.ctor;
  self->dtor = spooky_help_funcs.dtor;
  self->free = spooky_help_funcs.free;
  self->release = spooky_help_funcs.release;

  self->super.handle_event = spooky_help_funcs.super.handle_event;
  self->super.render = spooky_help_funcs.super.render;
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

  int help_text_w, help_text_h;
  const spooky_font * font = context->get_font(context);
  font->measure_text(font, "mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm", strlen("mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm"), &help_text_w, &help_text_h);
  int help_rect_w, help_rect_h;
  SDL_GetRendererOutputSize(context->get_renderer(context), &help_rect_w, &help_rect_h);
  SDL_Rect origin = {
    .x = (help_rect_w / 2) - (help_text_w / 2) - 500,
    .y = (help_rect_h / 2) - ((help_text_h * 24) / 2) - 350,
    .w = help_text_w,
    .h = help_text_h * 24
  };

  self->super.ctor((const spooky_base *)self, origin);

  spooky_help_impl * impl = calloc(1, sizeof * impl);
  if(!impl) { abort(); }

  impl->context = context;
  impl->show_help = false;

  ((spooky_help *)(uintptr_t)self)->impl = impl;

  const spooky_ex * ex = NULL;
  self->super.set_rect((const spooky_base *)self, &origin, &ex);
  return self;
}

const spooky_help * spooky_help_dtor(const spooky_help * self) {
  if(self != NULL) {
    free(self->impl), ((spooky_help *)(uintptr_t)self)->impl = NULL;
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

bool spooky_help_handle_event(const spooky_base * self, SDL_Event * event) {
  spooky_help_impl * impl = ((const spooky_help *)self)->impl;
  switch(event->type) {
    case SDL_KEYUP:
      {
        SDL_Keycode sym = event->key.keysym.sym;
        switch(sym) {
          case SDLK_F1: /* show help */
          case SDLK_SLASH:
          case SDLK_QUESTION:
            {
              impl->show_help = true;
            }
            break;
          case SDLK_ESCAPE: /* hide help */
            {
              impl->show_help = false;
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
  return impl->show_help;
}

void spooky_help_render(const spooky_base * self, SDL_Renderer * renderer) {
  spooky_help_impl * impl = ((const spooky_help *)self)->impl;

  if(!impl->show_help) { return; }

  static char help[1920] = { 0 };
  static_assert(sizeof(help) == 1920, "Help buffer must be 1920 bytes.");

  const spooky_font * font = impl->context->get_font(impl->context);

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

  assert(help_out > 0 && (size_t)help_out < sizeof(help));

  const SDL_Color help_fore_color = { .r = 255, .g = 255, .b = 255, .a = 255};
  const SDL_Rect * rect = self->get_rect(self);

  int help_w = 0, help_h = 0;
  int line_skip = font->get_line_skip(font);
  font->measure_text(font, help, (size_t)help_out, &help_w, &help_h);
  const SDL_Point help_point = { .x = rect->x + (rect->w / 2), .y = rect->y + (line_skip * 3) };
  const spooky_gui_rgba_context * rgba = spooky_gui_push_draw_color(renderer);
  {
    SDL_BlendMode blend_mode;
    SDL_GetRenderDrawBlendMode(renderer, &blend_mode);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 199, 78, 157, 150);

    SDL_Rect rr = { .x = help_point.x - 10, .y = help_point.y - 10, .w = help_w + 20, .h = help_h + 20 };
    SDL_RenderFillRect(renderer, &rr);
    SDL_SetRenderDrawBlendMode(renderer, blend_mode);

    spooky_gui_pop_draw_color(rgba);
  }

  int out_w = 0, out_h = 0;
  font->write_to_renderer(font, renderer, &help_point, &help_fore_color, help, (size_t)help_out, &out_w, &out_h);
}
