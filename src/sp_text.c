#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>

#include "../include/sp_context.h"
#include "../include/sp_font.h"
#include "../include/sp_text.h"

static const size_t sp_text_max_input_len = 37;

static void sp_text_handle_delta(const sp_base * self, const SDL_Event * event, uint64_t last_update_time, double interpolation);
static bool sp_text_handle_event(const sp_base * self, SDL_Event * event);
static void sp_text_render(const sp_base * self, SDL_Renderer * renderer);

static void sp_text_copy_text_to_buffer(const sp_text * self, const char * text, size_t text_len);

typedef struct sp_text_data {
  const sp_context * context;

  char * text;
  size_t text_len;
  size_t text_capacity;

  char * current_command;
  char padding[6]; /* not portable */
  bool capture_input;
  bool hide_cursor;
} sp_text_data;

static const sp_text sp_text_funcs = {
  .as_base = &sp_text_as_base,
  .ctor = &sp_text_ctor,
  .dtor = &sp_text_dtor,
  .free = &sp_text_free,
  .release = &sp_text_release,

  .super.handle_event = &sp_text_handle_event,
  .super.render = &sp_text_render,
  .super.handle_delta = &sp_text_handle_delta
};

const sp_base * sp_text_as_base(const sp_text * self) {
  return ((const sp_base *)self);
}

const sp_text * sp_text_init(sp_text * self) {
  assert(self != NULL);
  if(!self) { abort(); }

  self = (sp_text *)(uintptr_t)sp_base_init((sp_base *)(uintptr_t)self);

  self->as_base = sp_text_funcs.as_base;
  self->ctor = sp_text_funcs.ctor;
  self->dtor = sp_text_funcs.dtor;
  self->free = sp_text_funcs.free;
  self->release = sp_text_funcs.release;

  self->super.handle_event = sp_text_funcs.super.handle_event;
  self->super.render = sp_text_funcs.super.render;
  self->super.handle_delta = sp_text_funcs.super.handle_delta;

  return self;
}

const sp_text * sp_text_alloc(void) {
  sp_text * self = calloc(1, sizeof * self);
  if(!self) { abort(); }
  return self;
}

const sp_text * sp_text_acquire(void) {
  return sp_text_init((sp_text * )(uintptr_t)sp_text_alloc());
}

const sp_text * sp_text_ctor(const sp_text * self, const char * name, const sp_context * context, SDL_Renderer * renderer) {
  sp_text_data * data = calloc(1, sizeof * data);
  if(!data) abort();

  SDL_Rect origin = {
    .x = 10,
    .y = 10,
    .w = 10,
    .h = 10
  };
  self->super.ctor((const sp_base *)self, name, origin);

  data->context = context;
  (void)renderer;

  data->capture_input = false;
  data->text = NULL;
  data->text_capacity = 0;
  data->text_len = 0;
  data->current_command = NULL;
  data->hide_cursor = true;

  ((sp_text *)(uintptr_t)self)->data = data;

  return self;
}

const sp_text * sp_text_dtor(const sp_text * self) {
  if(self) {
    self->data->capture_input = false;
    self->data->text_len = 0;
    self->data->text_capacity = 0;
    free(self->data->current_command), self->data->current_command = NULL;
    free(self->data->text), self->data->text = NULL;
    free(self->data), ((sp_text *)(uintptr_t)self)->data = NULL;
  }
  return self;
}

void sp_text_free(const sp_text * self) {
  free((sp_text *)(uintptr_t)self), self = NULL;
}

void sp_text_release(const sp_text * self) {
  self->free(self->dtor(self));
}

static void sp_text_copy_text_to_buffer(const sp_text * self, const char * text, size_t text_len) {
  if(text_len < 1) { return; }
  sp_text_data * data = self->data;
  if(!data->text) {
    data->text = calloc(sp_text_max_input_len, sizeof *(data->text));
    data->text_capacity = sp_text_max_input_len;
    data->text_len = 0;
  }
  if(data->text_len + text_len < sp_text_max_input_len) {
    char * dest = data->text + data->text_len;
    const char * src = text;
    while (*src != '\0')
    {
      *dest = *src;
      dest++;
      src++;
      data->text_len++;
    }
  }
}

static bool sp_text_handle_event(const sp_base * self, SDL_Event * event) {
  sp_text_data * data = ((const sp_text *)self)->data;
  if(data->capture_input && event->type == SDL_TEXTINPUT) {
    /* accumulate command text */
    size_t input_len = strnlen(event->text.text, sp_text_max_input_len);
    sp_text_copy_text_to_buffer((const sp_text *)self, event->text.text, input_len);
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
    if(!data->text) { return true; }
    data->current_command = strndup(data->text, sp_text_max_input_len);
    free(data->text), data->text = NULL;
    data->text_len = 0;
    data->text_capacity = sp_text_max_input_len;
    data->text = calloc(data->text_capacity, sizeof * data->text);
    data->capture_input = false;
    return true;
  }
  else if(data->capture_input && event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_v && (SDL_GetModState() & KMOD_CTRL || SDL_GetModState() & KMOD_GUI)) {
    if(SDL_HasClipboardText()) {
      /* handle paste */
      const char * clipboard = SDL_GetClipboardText();
      if(clipboard) {
        size_t clipboard_len = strnlen(clipboard, sp_text_max_input_len);
        sp_text_copy_text_to_buffer((const sp_text *)self, clipboard, clipboard_len);
      }
    }
    return true;
  } else if(event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_ESCAPE) {
    data->capture_input = false;
    return true;
  } else if(event->type == SDL_KEYUP) {
    SDL_Keycode sym = event->key.keysym.sym;
    switch(sym) {
      case SDLK_SPACE: /* show text capture */
        data->capture_input = true;
        return true;
        break;
      default:
        break;
    }
  }

  return false;
}

static void sp_text_render(const sp_base * self, SDL_Renderer * renderer) {
  sp_text_data * data = ((const sp_text *)self)->data;

  if(!data->capture_input) { return; }

  int w = 0, h = 0;

  SDL_GetRendererOutputSize(renderer, &w, &h);

  int x_center = w / 2;
  int y_center = h / 2;
  SDL_Rect rect = {
    .x = x_center - (800 / 2),
    .y = y_center - (100 / 2),
    .w = 800,
    .h = 100
  };

  static const SDL_Color white = { 255, 255, 255, 255 };
  // SDL_Rect rect = { .x = (w / 2) - (200 * 2), .y = h - 175, .w = w - 200, .h = 100 };
  const sp_gui_rgba_context * rgba_context = sp_gui_push_draw_color(renderer, &white);
  {
    SDL_RenderFillRect(renderer, &rect);
    const sp_font * font = data->context->get_font(data->context);
    bool orthographic_ligatures = font->get_enable_orthographic_ligatures(font);
    font->set_enable_orthographic_ligatures(font, false);

    static const SDL_Color black = { 0, 0, 0, 255 };
    SDL_Point base_point = { .x = rect.x + 20, .y = rect.y + 50 };
    SDL_Rect text_rect = { .x = base_point.x, .y = base_point.y, .w = rect.w - 40, .h = font->get_height(font) + 6 };
    const sp_gui_rgba_context * text_context = sp_gui_push_draw_color(renderer, &black);
    {
      SDL_Point instructions_point = { .x = rect.x + 20, .y = rect.y + 10 };
      font->write_to_renderer(font, renderer, &instructions_point, &black, "WHAT SHALL I DO?", strlen("WHAT SHALL I DO?"), NULL, NULL);
      SDL_RenderFillRect(renderer, &text_rect);
      sp_gui_pop_draw_color(text_context);
    }

    SDL_Point input_point = { .x = base_point.x + 7, .y = base_point.y + 2 };
    int cursor_x = 0;
    font->write_to_renderer(font, renderer, &input_point, &white, data->text, data->text_len, &cursor_x, NULL);

    /* Draw blinking cursor */
    if(!data->hide_cursor) {
      input_point.x = cursor_x;
      font->write_to_renderer(font, renderer, &input_point, &white, "_", strlen("_"), NULL, NULL);
    }

    font->set_enable_orthographic_ligatures(font, orthographic_ligatures);
    sp_gui_pop_draw_color(rgba_context);
  }
}

static void sp_text_handle_delta(const sp_base * self, const SDL_Event * event, uint64_t last_update_time, double interpolation) {
  (void)event;
  (void)interpolation;
  sp_text_data * data = ((const sp_text *)self)->data;
  data->hide_cursor = (last_update_time / 375) % 2 == 0;
}

