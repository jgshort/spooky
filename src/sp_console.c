#define _POSIX_C_SOURCE 200809L

#include <limits.h>
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include "config.h"
#include "sp_limits.h"
#include "sp_gui.h"
#include "sp_base.h"
#include "sp_context.h"
#include "sp_console.h"

static const int spooky_console_max_display_lines = 1024;
static const int spooky_console_animation_speed = 128;
static const int spooky_console_max_input_len = 80;

typedef struct spooky_console_line spooky_console_line;
typedef struct spooky_console_line {
  spooky_console_line * next;
  spooky_console_line * prev;
  size_t line_len;
  char * line;
  bool is_command;
  char padding[7]; /* not portable */
} spooky_console_line;

typedef struct spooky_console_lines {
  size_t count;
  spooky_console_line * head;
} spooky_console_lines;

typedef struct spooky_console_impl {
  const spooky_context * context;
  spooky_console_lines * lines;
  int direction;
  bool show_console;
  bool is_animating;
  bool hide_cursor;
  char padding[1]; /* not portable */

  char * text;
  size_t text_len;
  size_t text_capacity;

  char * current_command;
} spooky_console_impl;

static bool spooky_console_handle_event(const spooky_base * self, SDL_Event * event);
static void spooky_console_handle_delta(const spooky_base * self, int64_t last_update_time, double interpolation);
static void spooky_console_render(const spooky_base * self, SDL_Renderer * renderer);
static void spooky_console_push_str(const spooky_console * self, const char * str);
static void spooky_console_list_prepend(spooky_console_line * X, spooky_console_line * P);
static void spooky_console_push_str_impl(const spooky_console * self, const char * str, bool is_command);
static const char * spooky_console_get_current_command(const spooky_console * self);
static void spooky_console_clear_current_command(const spooky_console * self);
static void spooky_console_clear_console(const spooky_console * self);
static void spooky_console_copy_text_to_buffer(const spooky_console * self, const char * text, size_t text_len);

static const spooky_console spooky_console_funcs = {
  .as_base = &spooky_console_as_base,
  .ctor = &spooky_console_ctor,
  .dtor = &spooky_console_dtor,
  .free = &spooky_console_free,
  .release = &spooky_console_release,

  .super.handle_event = &spooky_console_handle_event,
  .super.handle_delta = &spooky_console_handle_delta,
  .super.render = &spooky_console_render,

  .push_str = &spooky_console_push_str,
  .get_current_command = &spooky_console_get_current_command,
  .clear_current_command = &spooky_console_clear_current_command,
  .clear_console = &spooky_console_clear_console
};

const spooky_base * spooky_console_as_base(const spooky_console * self) {
  return SP_AS_BASE(self);
}

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

  self->as_base = spooky_console_funcs.as_base;
  self->ctor = spooky_console_funcs.ctor;
  self->dtor = spooky_console_funcs.dtor;
  self->free = spooky_console_funcs.free;
  self->release = spooky_console_funcs.release;
  self->super.handle_event = spooky_console_funcs.super.handle_event;
  self->super.handle_delta = spooky_console_funcs.super.handle_delta;
  self->super.render = spooky_console_funcs.super.render;
  self->push_str = spooky_console_funcs.push_str;
  self->get_current_command = spooky_console_funcs.get_current_command;
  self->clear_current_command = spooky_console_funcs.clear_current_command;
  self->clear_console = spooky_console_funcs.clear_console;
  return self;
}

const spooky_console * spooky_console_acquire() {
  return spooky_console_init((spooky_console *)(uintptr_t)spooky_console_alloc());
}

const spooky_console * spooky_console_ctor(const spooky_console * self, const spooky_context * context, SDL_Renderer * renderer) {
  assert(self != NULL);
  assert(context != NULL && renderer != NULL);

  SDL_Rect origin = { .x = 0, .y = 0, .w = 0, .h = 0 };
  int w, h;
  if(SDL_GetRendererOutputSize(context->get_renderer(context), &w, &h) == 0) {
    /* max width = renderer_width - some_margin */
    const spooky_font * font = context->get_font(context);
    int margin = (font->get_m_dash(font) - 1) * 5;
    origin.w = w - (margin * 2);
    origin.h = h - (margin / 2);
    origin.y = -origin.h;
    origin.x = margin;
  }

  self = (spooky_console *)(uintptr_t)spooky_base_ctor((spooky_base *)(uintptr_t)self, origin);

  spooky_console_impl * impl = calloc(1, sizeof * impl);
  if(!impl) { abort(); }

  impl->context = context;
  impl->direction = -spooky_console_animation_speed;
  impl->show_console = false;
  impl->is_animating = false;
  impl->hide_cursor = true;

  impl->lines = calloc(1, sizeof * impl->lines);
  impl->lines->head = NULL;
  impl->lines->count = 0;

  impl->current_command = NULL;
  impl->text_len = 0;
  impl->text_capacity = 0; //SPOOKY_MAX_STRING_LEN;
  impl->text = NULL;// calloc(impl->text_capacity, sizeof * impl->text);;

  const spooky_ex * ex = NULL;
  ((const spooky_base *)self)->set_rect((const spooky_base *)self, &origin, &ex);
  ((spooky_console *)(uintptr_t)self)->impl = impl;

  return self;
}

const spooky_console * spooky_console_dtor(const spooky_console * self) {
  if(self) {
    spooky_console_impl * impl = self->impl;
    spooky_console_clear_console(self);
    free(impl->current_command), impl->current_command = NULL;
    free(impl), impl = NULL;
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

static void spooky_console_copy_text_to_buffer(const spooky_console * self, const char * text, size_t text_len) {
  spooky_console_impl * impl = self->impl;
  if(!impl->text) {
    impl->text = calloc(spooky_console_max_input_len, sizeof *impl->text);
    impl->text_capacity = spooky_console_max_input_len;
    impl->text_len = 0;
  }
  if(impl->text_len + text_len < spooky_console_max_input_len) {
    char * dest = impl->text + impl->text_len;
    const char * src = text;
    const char * src_end = src + text_len;
    while (src < src_end && *src != '\0')
    {
      *dest = *src;
      dest++;
      src++;
      impl->text_len++;
    }
  }
}

bool spooky_console_handle_event(const spooky_base * self, SDL_Event * event) {
  spooky_console_impl * impl = ((const spooky_console *)self)->impl;

  bool toggled = false;
  if(event->type == SDL_KEYDOWN && SDL_GetModState() & KMOD_CTRL && event->key.keysym.sym == SDLK_BACKQUOTE) {
    impl->show_console = !impl->show_console;
    toggled = true;
  }
  else if(impl->show_console && !impl->is_animating) {
    if(event->type == SDL_TEXTINPUT) {
      /* accumulate command text */
      size_t input_len = strnlen(event->text.text, spooky_console_max_input_len);
      spooky_console_copy_text_to_buffer((const spooky_console *)self, event->text.text, input_len);
    }
    else if(event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_BACKSPACE) {
      /* delete command text */
      if(impl->text_len > 0) {
        impl->text[impl->text_len - 1] = '\0';
        impl->text_len--;
        if(impl->text_len < 0) { impl->text_len = 0; }
      }
    }
    else if(event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_RETURN) {
      /* push a new command */
      if(impl->current_command) { free(impl->current_command), impl->current_command = NULL; }
      impl->current_command = strndup(impl->text, spooky_console_max_input_len);
      spooky_console_push_str_impl((const spooky_console *)self, impl->text, true);
      free(impl->text), impl->text = NULL;
      impl->text_len = 0;
      impl->text_capacity = spooky_console_max_input_len;
      impl->text = calloc(impl->text_capacity, sizeof * impl->text);
    }
    else if(event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_v && (SDL_GetModState() & KMOD_CTRL || SDL_GetModState() & KMOD_GUI)) {
      if(SDL_HasClipboardText()) {
        /* handle paste */
        const char * clipboard = SDL_GetClipboardText();
        if(clipboard) {
          size_t clipboard_len = strnlen(clipboard, spooky_console_max_input_len);
          spooky_console_copy_text_to_buffer((const spooky_console *)self, clipboard, clipboard_len);
        }
      }
    }

    return true;
  }

  if(toggled) {
    impl->direction = impl->direction < 0 ? spooky_console_animation_speed : -spooky_console_animation_speed;
    impl->is_animating = true;
    self->set_focus(self, !impl->is_animating && impl->show_console);
    return true;
  }

  return false;
}

void spooky_console_handle_delta(const spooky_base * self, int64_t last_update_time, double interpolation) {
  spooky_console_impl * impl = ((const spooky_console *)self)->impl;

  if(impl->is_animating) {
    const SDL_Rect * rect = self->get_rect(self);
    SDL_Rect r = { .x = rect->x, .y = rect->y, .w = rect->w, .h = rect->h };
    r.y += (int)floor((double)impl->direction * interpolation);

    if(r.y < -r.h) {
      r.y = -r.h;
      impl->is_animating = false;
    } else if(r.y > 0) {
      r.y = 0;
      impl->is_animating = false;
    }

    const spooky_ex * ex = NULL;
    self->set_rect(self, &r, &ex);
  }

  int w, h;
  if(SDL_GetRendererOutputSize(impl->context->get_renderer(impl->context), &w, &h) == 0) {
    self->set_w(self, (int)(floor((float)w * .75)));
    self->set_x(self, (w / 2) - (self->get_w(self) / 2));
    self->set_h(self, (int)(floor((float)h * .75)));
  }

  impl->hide_cursor = (last_update_time / 650000000) % 2 == 0;
}

void spooky_console_render(const spooky_base * self, SDL_Renderer * renderer) {
  spooky_console_impl * impl = ((const spooky_console *)self)->impl;

  if(impl->show_console || impl->is_animating) {
    uint8_t r, g, b, a;
    SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);

    SDL_BlendMode blend_mode;
    SDL_GetRenderDrawBlendMode(renderer, &blend_mode);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 173);
    const SDL_Rect * rect = self->get_rect(self);
    SDL_RenderFillRect(renderer, rect);

    SDL_SetRenderDrawBlendMode(renderer, blend_mode);
    SDL_SetRenderDrawColor(renderer, r, g, b, a);

    const spooky_font * font = impl->context->get_font(impl->context);
    int line_skip = font->get_line_skip(font);
    SDL_Point dest = { .x = rect->x + 10, .y = rect->y + rect->h - line_skip - 10};
    assert(impl->lines->count < INT_MAX);

    static const SDL_Color color = { .r = 0, .g = 255, .b = 0, .a = 255};

    int line_next = 2;

    spooky_console_line * t = impl->lines->head;
    /* disable the presentation of orthographic ligatures as it negatively impacts the way text entry is performed */
    bool enable_orthographic_ligatures = font->get_enable_orthographic_ligatures(font);
    font->set_enable_orthographic_ligatures(font, false);
    int dot_w, dot_h;
    font->measure_text(font, "...", strlen("..."), &dot_w, &dot_h);

    while(t != NULL) {
      t = t->prev;
      if(t->line_len > 0 && t->line != NULL) {
        SDL_Point text_dest = { .x = rect->x + 10, .y = rect->y + rect->h - (line_skip * (line_next)) - 10};

        int line_w = 0, line_h = 0;
        font->measure_text(font, t->line, t->line_len, &line_w, &line_h);
        size_t max_rect_width = (size_t)rect->w - (size_t)dot_w;
        size_t max_chars = (size_t)((float)max_rect_width / (float)((float)line_w / (float)t->line_len)/* font->get_m_dash(font))*/);

        if(t->is_command) {
          /* draw the executed command */
          static const SDL_Color command_color = { .r = 255, .g = 0x55, .b = 255, .a = 255 };
          int command_width = 0;
          font->write_to_renderer(font, renderer, &text_dest, &command_color, "> ", strlen("> "), &command_width, NULL);
          text_dest.x = command_width;

          static char * temp = NULL;
          if(!temp) {
            temp = calloc(1024, sizeof * temp);
            assert(temp);
          }
          int temp_len = snprintf(temp, max_chars, "%s...", t->line);
          assert(temp_len > 0);
          font->write_to_renderer(font, renderer, &text_dest, &command_color, temp, (size_t)temp_len, NULL, NULL);
        } else {
          if(t->line_len <= max_chars) {
            font->write_to_renderer(font, renderer, &text_dest, &color, t->line, t->line_len, NULL, NULL);
          } else {
            static char temp[1024] = { 0 };
            int temp_len = snprintf(temp, max_chars, "%s...", t->line);
            assert(temp_len > 0);
            font->write_to_renderer(font, renderer, &text_dest, &color, temp, (size_t)temp_len, NULL, NULL);
          }
        }
        line_next++;
      }
      if(t == impl->lines->head) { break; }
    }

    // Draw prompt
    font->write_to_renderer(font, renderer, &dest, &color, "$", strlen("$"), NULL, NULL);

    /* Draw command accumulator */
    static const SDL_Color command_color = { .r = 255, .g = 255, .b = 0, .a = 255 };

    int text_w, text_h;
    font->measure_text(font, impl->text, impl->text_len, &text_w, &text_h);

    dest.x += font->get_m_dash(font) * 2;
    if(impl->text != NULL) {
      int chevron_w, chevron_h, chevron_advance;
      font->measure_text(font, "$ <<<_", strlen("$ <<<_"), &chevron_w, &chevron_h);
      size_t max_rect_width = (size_t)rect->w - (size_t)chevron_w;
      size_t max_chars = (size_t)((float)((float)max_rect_width) / (float)((float)text_w / (float)impl->text_len));

      if((size_t)text_w > max_rect_width) {
        /* only draw text that will fit within the console window */
        static const SDL_Color chevron_color = { .r = 255, .g = 0, .b = 255, .a = 255 };
        font->write_to_renderer(font, renderer, &dest, &chevron_color, "<<<", strlen("<<<"), &chevron_advance, NULL);
        size_t total_len = impl->text_len;
        size_t offset = total_len - max_chars;

        dest.x = chevron_advance;

        {
          static char * text = NULL;
          if(!text) {
            text = calloc(spooky_console_max_input_len, sizeof * text);
          } else {
            text[impl->text_len - 1] = '\0';
          }
          if(!text) { abort(); }
          int text_len = snprintf(text, spooky_console_max_input_len, "%s", impl->text + offset);
          assert(text_len > 0);
          int text_width;
          font->write_to_renderer(font, renderer, &dest, &command_color, text, (size_t)text_len, &text_width, NULL);
          free(text), text = NULL;
          dest.x = text_width;
        }
      } else {
        int text_width = 0;
        font->write_to_renderer(font, renderer, &dest, &command_color, impl->text, impl->text_len, &text_width, NULL);
        dest.x = text_width;
      }
    }

    /* Draw blinking cursor */
    if(!impl->hide_cursor) {
      font->write_to_renderer(font, renderer, &dest, &command_color, "_", strlen("_"), NULL, NULL);
    }

    font->set_enable_orthographic_ligatures(font, enable_orthographic_ligatures);
  }
}

void spooky_console_list_prepend(spooky_console_line * head, spooky_console_line * line) {
  assert(line != NULL && head != NULL);
  line->next = head;
  line->prev = head->prev;
  head->prev->next = line;
  head->prev = line;
}

void spooky_console_push_str(const spooky_console * self, const char * str) {
  spooky_console_push_str_impl(self, str, false);
}

void spooky_console_push_str_impl(const spooky_console * self, const char * s, bool is_command) {
  size_t strings_capacity = 128;
  char ** strings = calloc(strings_capacity, sizeof * strings);
  if(strings == NULL) { abort(); }
  size_t split_count = 0;

  size_t s_len = strnlen(s, SPOOKY_MAX_STRING_LEN);
  char * str = NULL;
  size_t str_len = 0;
  if(spooky_str_trim(s, s_len, SPOOKY_MAX_STRING_LEN, &str, &str_len) != SP_SUCCESS) { abort(); }

  char * working_str = strndup(str, SPOOKY_MAX_STRING_LEN);
  char * save_ptr = NULL, * token = NULL;
  char * ws = working_str;
  while((token = strtok_r(ws, "\n", &save_ptr)) != NULL) {
    assert(token != NULL);
    if(split_count >= strings_capacity) {
      strings_capacity *= 2;
      char ** temp = realloc(strings, strings_capacity * sizeof * strings);
      if(temp != NULL) {
        strings = temp;
      } else { abort(); }
    }
    assert(split_count < strings_capacity);
    /* duped string[sc] freed in dtor; copied below on line->line = string: */
    strings[split_count] = strndup(token, SPOOKY_MAX_STRING_LEN);
    split_count++;
    ws = NULL;
  }

  free(working_str), working_str = NULL;
  for(size_t split = 0; split < split_count; ++split) {
    spooky_console_line * line = calloc(1, sizeof * line);
    char * string = strings[split];
    if(string == NULL) { continue; }
    line->line = string;
    line->line_len = strnlen(string, SPOOKY_MAX_STRING_LEN);
    line->is_command = is_command;
    if(self->impl->lines->count > spooky_console_max_display_lines && self->impl->lines->head->next != NULL) {
      spooky_console_line * deleted = self->impl->lines->head->next;
      assert(deleted != NULL && deleted->next != NULL);
      if(deleted->next != NULL) {
        deleted->next->prev = self->impl->lines->head;
        self->impl->lines->head->next = deleted->next;
      }
      free(deleted), deleted = NULL;
    }
    if(self->impl->lines->head == NULL) {
      line->next = line;
      line->prev = line;
      self->impl->lines->head = line;
    } else {
      spooky_console_list_prepend(self->impl->lines->head, line);
    }
    self->impl->lines->count++;
  }
  free(strings), strings = NULL;
}

const char * spooky_console_get_current_command(const spooky_console * self) {
  return self->impl->current_command;
}

void spooky_console_clear_current_command(const spooky_console * self) {
  free(self->impl->current_command), self->impl->current_command = NULL;
}

void spooky_console_clear_console(const spooky_console * self) {
  spooky_console_impl * impl = self->impl;
  if(impl && impl->lines && impl->lines->head) {
    spooky_console_line * t = impl->lines->head->next;
    while(t != impl->lines->head) {
      if(t->line != NULL) {
        free(t->line), t->line = NULL;
      }
      spooky_console_line * old = t;
      t = t->next;
      free(old), old = NULL;
    }
    free(impl->lines->head), impl->lines->head = NULL;
  }
  free(impl->text), impl->text = NULL;
  impl->text_capacity = spooky_console_max_input_len;
  impl->text_len = 0;
}
