#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>

#include "sp_context.h"
#include "sp_font.h"
#include "sp_text.h"

static const size_t spooky_text_max_input_len = 128;

static bool spooky_text_handle_event(const spooky_base * self, SDL_Event * event);
static void spooky_text_render(const spooky_base * self, SDL_Renderer * renderer);

static void spooky_text_copy_text_to_buffer(const spooky_text * self, const char * text, size_t text_len);

typedef struct spooky_text_data {
  const spooky_context * context;

  char * text;
  size_t text_len;
  size_t text_capacity;

  char * current_command;
  char padding[7]; /* not portable */
  bool capture_input;
} spooky_text_data;

static const spooky_text spooky_text_funcs = {
  .as_base = &spooky_text_as_base,
  .ctor = &spooky_text_ctor,
  .dtor = &spooky_text_dtor,
  .free = &spooky_text_free,
  .release = &spooky_text_release,

  .super.handle_event = &spooky_text_handle_event,
  .super.render = &spooky_text_render,
  .super.handle_delta = NULL
};

const spooky_base * spooky_text_as_base(const spooky_text * self) {
  return ((const spooky_base *)self);
}

const spooky_text * spooky_text_init(spooky_text * self) {
  assert(self != NULL);
  if(!self) { abort(); }

  self = (spooky_text *)(uintptr_t)spooky_base_init((spooky_base *)(uintptr_t)self);

  self->as_base = spooky_text_funcs.as_base;
  self->ctor = spooky_text_funcs.ctor;
  self->dtor = spooky_text_funcs.dtor;
  self->free = spooky_text_funcs.free;
  self->release = spooky_text_funcs.release;

  self->super.handle_event = spooky_text_funcs.super.handle_event;
  self->super.render = spooky_text_funcs.super.render;
  self->super.handle_delta = spooky_text_funcs.super.handle_delta;

  return self;
}

const spooky_text * spooky_text_alloc() {
  spooky_text * self = calloc(1, sizeof * self);
  if(!self) { abort(); }
  return self;
}

const spooky_text * spooky_text_acquire() {
  return spooky_text_init((spooky_text * )(uintptr_t)spooky_text_alloc());
}

const spooky_text * spooky_text_ctor(const spooky_text * self, const spooky_context * context, SDL_Renderer * renderer) {
  spooky_text_data * data = calloc(1, sizeof * data);
  if(!data) abort();

  SDL_Rect origin = {
    .x = 10,
    .y = 10,
    .w = 10,
    .h = 10
  };
  self->super.ctor((const spooky_base *)self, origin);

  data->context = context;
  (void)renderer;

  data->capture_input = false;
  data->text = NULL;
  data->text_capacity = 0;
  data->text_len = 0;
  data->current_command = NULL;

  ((spooky_text *)(uintptr_t)self)->data = data;

  return self;
}

const spooky_text * spooky_text_dtor(const spooky_text * self) {
  if(self) {
    self->data->capture_input = false;
    self->data->text_len = 0;
    self->data->text_capacity = 0;
    free(self->data->current_command), self->data->current_command = NULL;
    free(self->data->text), self->data->text = NULL;
    free(self->data), ((spooky_text *)(uintptr_t)self)->data = NULL;
  }
  return self;
}

void spooky_text_free(const spooky_text * self) {
  free((spooky_text *)(uintptr_t)self), self = NULL;
}

void spooky_text_release(const spooky_text * self) {
  self->free(self->dtor(self));
}

static void spooky_text_copy_text_to_buffer(const spooky_text * self, const char * text, size_t text_len) {
  spooky_text_data * data = self->data;
  if(!data->text) {
    data->text = calloc(spooky_text_max_input_len, sizeof *(data->text));
    data->text_capacity = spooky_text_max_input_len;
    data->text_len = 0;
  }
  if(data->text_len + text_len < spooky_text_max_input_len) {
    char * dest = data->text + data->text_len;
    const char * src = text;
    const char * src_end = src + text_len;
    while (src < src_end && *src != '\0')
    {
      *dest = *src;
      dest++;
      src++;
      data->text_len++;
    }
  }
}

static bool spooky_text_handle_event(const spooky_base * self, SDL_Event * event) {
  spooky_text_data * data = ((const spooky_text *)self)->data;
  if(data->capture_input && event->type == SDL_TEXTINPUT) {
    /* accumulate command text */
    size_t input_len = strnlen(event->text.text, spooky_text_max_input_len);
    spooky_text_copy_text_to_buffer((const spooky_text *)self, event->text.text, input_len);
    return true;
  }
  else if(data->capture_input && event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_BACKSPACE) {
    /* delete command text */
    if(data->text_len > 0) {
      if(data->text_len - 1 < data->text_len) {
        data->text_len -= 1;
        data->text[data->text_len] = '\0';
      }
    }
    return true;
  }
  else if(data->capture_input && event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_RETURN) {
    /* push a new command */
    if(data->current_command) { free(data->current_command), data->current_command = NULL; }
    data->current_command = strndup(data->text, spooky_text_max_input_len);
    free(data->text), data->text = NULL;
    data->text_len = 0;
    data->text_capacity = spooky_text_max_input_len;
    data->text = calloc(data->text_capacity, sizeof * data->text);
    data->capture_input = false;
    return true;
  }
  else if(data->capture_input && event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_v && (SDL_GetModState() & KMOD_CTRL || SDL_GetModState() & KMOD_GUI)) {
    if(SDL_HasClipboardText()) {
      /* handle paste */
      const char * clipboard = SDL_GetClipboardText();
      if(clipboard) {
        size_t clipboard_len = strnlen(clipboard, spooky_text_max_input_len);
        spooky_text_copy_text_to_buffer((const spooky_text *)self, clipboard, clipboard_len);
      }
    }
    return true;
  } else if(event->type == SDL_KEYUP) {
    SDL_Keycode sym = event->key.keysym.sym;
    switch(sym) {
      case SDLK_SPACE: /* show text capture */
        data->capture_input = true;
        return true;
        break;
      case SDLK_ESCAPE: /* hide text capture */
        data->capture_input = false;
        return true;
        break;
      default:
        break;
    }
  }

  return false;
}

static void spooky_text_render(const spooky_base * self, SDL_Renderer * renderer) {
  spooky_text_data * data = ((const spooky_text *)self)->data;

  if(!data->capture_input) { return; }

  int w = 0, h = 0;

  SDL_GetRendererOutputSize(renderer, &w, &h);

  static const SDL_Color white = { 255, 255, 255, 255 };
  SDL_Rect rect = { .x = (w / 2) - (200 * 2), .y = h - 175, .w = w - 200, .h = 100 };
  const spooky_gui_rgba_context * rgba_context = spooky_gui_push_draw_color(renderer, &white);
  {

    SDL_RenderFillRect(renderer, &rect);
    if(data->capture_input) {
      const spooky_font * font = data->context->get_font(data->context);
      static const SDL_Color black = { 0, 0, 0, 255 };
      SDL_Point base_point = { .x = rect.x + 20, .y = rect.y + 50 };
      SDL_Rect text_rect = { .x = base_point.x, .y = base_point.y, .w = rect.w - 40, .h = font->get_height(font) + 6 };
      const spooky_gui_rgba_context * text_context = spooky_gui_push_draw_color(renderer, &black);
      {
        SDL_Point instructions_point = { .x = rect.x + 20, .y = rect.y + 10 };
        font->write_to_renderer(font, renderer, &instructions_point, &black, "WHAT SHALL I DO?", strlen("WHAT SHALL I DO?"), NULL, NULL);
        SDL_RenderFillRect(renderer, &text_rect);
        spooky_gui_pop_draw_color(text_context);
      }
      SDL_Point input_point = { .x = base_point.x + 7, .y = base_point.y };
      font->write_to_renderer(font, renderer, &input_point, &white, data->text, data->text_len, NULL, NULL);
    }
    spooky_gui_pop_draw_color(rgba_context);
  }

  return;
}
