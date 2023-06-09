#define _POSIX_C_SOURCE 200809L

#include <limits.h>
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include "../config.h"
#include "../include/sp_limits.h"
#include "../include/sp_gui.h"
#include "../include/sp_base.h"
#include "../include/sp_context.h"
#include "../include/sp_console.h"

static const int sp_console_max_display_lines = 1024;
static const int sp_console_animation_speed = 128;
static const int sp_console_max_input_len = 80;

typedef struct sp_console_line sp_console_line;
typedef struct sp_console_line {
  sp_console_line * next;
  sp_console_line * prev;
  size_t line_len;
  char * line;
  bool is_command;
  char padding[7]; /* not portable */
} sp_console_line;

typedef struct sp_console_lines {
  size_t count;
  sp_console_line * head;
} sp_console_lines;

typedef struct sp_console_impl {
  const sp_context * context;
  sp_console_lines * lines;
  int direction;
  bool show_console;
  bool is_animating_up;
  bool is_animating_down;
  bool hide_cursor;
  //char padding[2]; /* not portable */

  char * text;
  size_t text_len;
  size_t text_capacity;

  char * current_command;
} sp_console_impl;

static bool sp_console_handle_event(const sp_base * self, SDL_Event * event);
static void sp_console_handle_delta(const sp_base * self, const SDL_Event * event, uint64_t last_update_time, double interpolation);
static void sp_console_render(const sp_base * self, SDL_Renderer * renderer);
static void sp_console_push_str(const sp_console * self, const char * str);
static void sp_console_list_prepend(sp_console_line * X, sp_console_line * P);
static void sp_console_push_str_impl(const sp_console * self, const char * str, bool is_command);
static const char * sp_console_get_current_command(const sp_console * self);
static void sp_console_clear_current_command(const sp_console * self);
static void sp_console_clear_console(const sp_console * self);
static void sp_console_copy_text_to_buffer(const sp_console * self, const char * text, size_t text_len);

static const sp_console sp_console_funcs = {
  .as_base = &sp_console_as_base,
  .ctor = &sp_console_ctor,
  .dtor = &sp_console_dtor,
  .free = &sp_console_free,
  .release = &sp_console_release,

  .super.handle_event = &sp_console_handle_event,
  .super.handle_delta = &sp_console_handle_delta,
  .super.render = &sp_console_render,

  .push_str = &sp_console_push_str,
  .get_current_command = &sp_console_get_current_command,
  .clear_current_command = &sp_console_clear_current_command,
  .clear_console = &sp_console_clear_console
};

const sp_base * sp_console_as_base(const sp_console * self) {
  return (const sp_base *)self;
}

const sp_console * sp_console_alloc(void) {
  sp_console * self = calloc(1, sizeof * self);
  if(self == NULL) {
    fprintf(stderr, "Unable to allocate memory.");
    abort();
  }
  return self;
}

const sp_console * sp_console_init(sp_console * self) {
  assert(self != NULL);
  if(!self) { abort(); }

  self = (sp_console *)(uintptr_t)sp_base_init((sp_base *)(uintptr_t)self);

  self->as_base = sp_console_funcs.as_base;
  self->ctor = sp_console_funcs.ctor;
  self->dtor = sp_console_funcs.dtor;
  self->free = sp_console_funcs.free;
  self->release = sp_console_funcs.release;
  self->super.handle_event = sp_console_funcs.super.handle_event;
  self->super.handle_delta = sp_console_funcs.super.handle_delta;
  self->super.render = sp_console_funcs.super.render;
  self->push_str = sp_console_funcs.push_str;
  self->get_current_command = sp_console_funcs.get_current_command;
  self->clear_current_command = sp_console_funcs.clear_current_command;
  self->clear_console = sp_console_funcs.clear_console;
  return self;
}

const sp_console * sp_console_acquire(void) {
  return sp_console_init((sp_console *)(uintptr_t)sp_console_alloc());
}

const sp_console * sp_console_ctor(const sp_console * self, const char * name, const sp_context * context, SDL_Renderer * renderer) {
  assert(self != NULL);
  assert(context != NULL && renderer != NULL);

  SDL_Rect origin = { .x = 0, .y = 0, .w = 0, .h = 0 };
  self = (sp_console *)(uintptr_t)sp_base_ctor((sp_base *)(uintptr_t)self, name, origin);

  sp_console_impl * impl = calloc(1, sizeof * impl);
  if(!impl) { abort(); }

  impl->context = context;
  impl->direction = -sp_console_animation_speed;
  impl->show_console = false;
  impl->is_animating_up = false;
  impl->is_animating_down = false;
  impl->hide_cursor = true;

  impl->lines = calloc(1, sizeof * impl->lines);
  impl->lines->head = NULL;
  impl->lines->count = 0;

  impl->current_command = NULL;
  impl->text_len = 0;
  impl->text_capacity = 0; //SP_MAX_STRING_LEN;
  impl->text = NULL;// calloc(impl->text_capacity, sizeof * impl->text);;

  int w = 0, h = 0;
  if(SDL_GetRendererOutputSize(impl->context->get_renderer(impl->context), &w, &h) == 0) {
    const sp_base * base = (const sp_base *)self;
    base->set_x(base, (w / 2) - (base->get_w(base) / 2));
    base->set_w(base, (int)(floor((float)w * .75)));
    base->set_h(base, (int)(floor((float)h * .75)));
    base->set_y(base, 0-(base->get_h(base)));
  }
  ((sp_console *)(uintptr_t)self)->impl = impl;
  return self;
}

const sp_console * sp_console_dtor(const sp_console * self) {
  if(self) {
    sp_console_impl * impl = self->impl;
    sp_console_clear_console(self);
    free(impl->current_command), impl->current_command = NULL;
    free(impl), impl = NULL;
  }

  return self;
}

void sp_console_free(const sp_console * self) {
  if(self) {
    free((void *)(uintptr_t)self), self = NULL;
  }
}

void sp_console_release(const sp_console * self) {
  self->free(self->dtor(self));
}

static void sp_console_copy_text_to_buffer(const sp_console * self, const char * text, size_t text_len) {
  sp_console_impl * impl = self->impl;
  if(!impl->text) {
    impl->text = calloc(sp_console_max_input_len, sizeof *impl->text);
    impl->text_capacity = sp_console_max_input_len;
    impl->text_len = 0;
  }
  if(impl->text_len + text_len < sp_console_max_input_len) {
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

bool sp_console_handle_event(const sp_base * self, SDL_Event * event) {
  static bool ctrl = false;

  sp_console_impl * impl = ((const sp_console *)self)->impl;

  switch(event->type) {
    case SDL_KEYDOWN:
      if(event->key.keysym.sym == SDLK_LCTRL || event->key.keysym.sym == SDLK_RCTRL) {
        ctrl = true;
      }
      break;
    case SDL_KEYUP:
      if(event->key.keysym.sym == SDLK_LCTRL || event->key.keysym.sym == SDLK_RCTRL) {
        ctrl = false;
      }
      break;
    default:
      break;
  }

  switch(event->type) {
    case SDL_TEXTINPUT:
      {
        size_t input_len = strnlen(event->text.text, sp_console_max_input_len);
        sp_console_copy_text_to_buffer((const sp_console *)self, event->text.text, input_len);
        return true;
      }
      break;
    case SDL_KEYDOWN:
      {
        switch(event->key.keysym.sym) {
          case SDLK_BACKQUOTE:
            {
              if(ctrl) {
                if(!impl->is_animating_up && !impl->is_animating_down && !impl->show_console) {
                  if(!impl->show_console) {
                    impl->is_animating_down = true;
                  }
                } else if(impl->show_console && !impl->is_animating_down) {
                  impl->is_animating_up = true;
                }
                return true;
              }
            }
            break;
          case SDLK_BACKSPACE:
            {
              if(impl->text_len > 0) {
                if(impl->text && impl->text_len > 0) {
                  impl->text[impl->text_len - 1] = '\0';
                  impl->text_len--;
                }
              }
              return true;
            }
            break;
          default:
            break;
        }
      }
      break;
    case SDL_KEYUP:
      switch(event->key.keysym.sym) {
        case SDLK_RETURN:
          {
            if(impl->current_command) {
              free(impl->current_command), impl->current_command = NULL;
            }
            impl->current_command = strndup(impl->text, sp_console_max_input_len);
            sp_console_push_str_impl((const sp_console *)self, impl->text, true);
            free(impl->text), impl->text = NULL;
            impl->text_len = 0;
            impl->text_capacity = sp_console_max_input_len;
            impl->text = calloc(impl->text_capacity, sizeof * impl->text);
            if(!impl->text) { abort(); }
            return true;
          }
          break;
        case SDLK_v:
          {
            if(ctrl && (event->key.keysym.sym == SDLK_v && (SDL_GetModState() & KMOD_GUI))) {
              if(SDL_HasClipboardText()) {
                /* handle paste */
                const char * clipboard = SDL_GetClipboardText();
                if(clipboard) {
                  size_t clipboard_len = strnlen(clipboard, sp_console_max_input_len);
                  sp_console_copy_text_to_buffer((const sp_console *)self, clipboard, clipboard_len);
                  return true;
                }
              }
            }
          }
          break;
        default:
          break;
      }
    default:
      break;
  }

  return false;
}

void sp_console_handle_delta(const sp_base * self, const SDL_Event * event, uint64_t last_update_time, double interpolation) {
  (void)event; (void)interpolation;
  sp_console_impl * impl = ((const sp_console *)self)->impl;
  if(impl->is_animating_up || impl->is_animating_down || impl->show_console) {
    int w = 0, h = 0;
    if(SDL_GetRendererOutputSize(impl->context->get_renderer(impl->context), &w, &h) == 0) {
      self->set_x(self, (w / 2) - (self->get_w(self) / 2));
      self->set_w(self, (int)(floor((float)w * .75)));
      self->set_h(self, (int)(floor((float)h * .75)));
    }
    impl->hide_cursor = (last_update_time / 375) % 2 == 0;

    const sp_ex * ex = NULL;
    const SDL_Rect * rect = self->get_rect(self);
    SDL_Rect r = { .x = rect->x, .y = rect->y, .w = rect->w, .h = rect->h };

    if(impl->is_animating_up) {
      r.y -= (int)floor((double)sp_console_animation_speed * interpolation);
      if(r.y <= -r.h) {
        r.y = -r.h;
        impl->is_animating_up = false;
        impl->is_animating_down = false;
        impl->show_console = false;
      }
    } else if(impl->is_animating_down) {
      impl->show_console = true;
      r.y += (int)floor((double)sp_console_animation_speed * interpolation);
      if(r.y >= 0) {
        r.y = 0;
        impl->is_animating_up = false;
        impl->is_animating_down = false;
      }
    } else if(!impl->is_animating_down && !impl->is_animating_up && impl->show_console) {
      r.y = 0;
    }

    if(impl->is_animating_down || impl->show_console) {
      self->set_focus(self, true);
      impl->context->set_modal(impl->context, self);
    } else {
      self->set_focus(self, false);
      impl->context->set_modal(impl->context, NULL);
    }
    self->set_rect(self, &r, &ex);
  }
}

void sp_console_render(const sp_base * self, SDL_Renderer * renderer) {
  sp_console_impl * impl = ((const sp_console *)self)->impl;

  if(impl->show_console) {
    const SDL_Rect * rect = self->get_rect(self);
    SDL_Color transparent_black = { .r = 0, .g = 0, .b = 0, .a = 173 };
    const sp_gui_rgba_context * rgba = sp_gui_push_draw_color(renderer, &transparent_black);
    {
      SDL_BlendMode blend_mode;
      SDL_GetRenderDrawBlendMode(renderer, &blend_mode);

      SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
      SDL_RenderFillRect(renderer, rect);

      SDL_SetRenderDrawBlendMode(renderer, blend_mode);
      sp_gui_pop_draw_color(rgba);
    }

    const sp_font * font = impl->context->get_font(impl->context);
    int line_skip = font->get_line_skip(font);
    SDL_Point dest = { .x = rect->x + 10, .y = rect->y + rect->h - line_skip - 10};
    assert(impl->lines->count < INT_MAX);

    static const SDL_Color color = { .r = 0, .g = 255, .b = 0, .a = 255};

    int line_next = 2;

    sp_console_line * t = impl->lines->head;
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
            text = calloc(sp_console_max_input_len, sizeof * text);
          } else {
            text[impl->text_len - 1] = '\0';
          }
          if(!text) { abort(); }
          int text_len = snprintf(text, sp_console_max_input_len, "%s", impl->text + offset);
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

void sp_console_list_prepend(sp_console_line * head, sp_console_line * line) {
  assert(line != NULL && head != NULL);
  line->next = head;
  line->prev = head->prev;
  head->prev->next = line;
  head->prev = line;
}

void sp_console_push_str(const sp_console * self, const char * str) {
  sp_console_push_str_impl(self, str, false);
}

void sp_console_push_str_impl(const sp_console * self, const char * s, bool is_command) {
  size_t strings_capacity = 128;
  char ** strings = calloc(strings_capacity, sizeof * strings);
  if(strings == NULL) { abort(); }
  size_t split_count = 0;

  size_t s_len = strnlen(s, SP_MAX_STRING_LEN);
  char * str = NULL;
  size_t str_len = 0;
  if(sp_str_trim(s, s_len, SP_MAX_STRING_LEN, &str, &str_len) != SP_SUCCESS) { abort(); }

  char * working_str = strndup(str, SP_MAX_STRING_LEN);
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
    strings[split_count] = strndup(token, SP_MAX_STRING_LEN);
    split_count++;
    ws = NULL;
  }

  free(working_str), working_str = NULL;
  for(size_t split = 0; split < split_count; ++split) {
    sp_console_line * line = calloc(1, sizeof * line);
    char * string = strings[split];
    if(string == NULL) { continue; }
    line->line = string;
    line->line_len = strnlen(string, SP_MAX_STRING_LEN);
    line->is_command = is_command;
    if(self->impl->lines->count > sp_console_max_display_lines && self->impl->lines->head->next != NULL) {
      sp_console_line * deleted = self->impl->lines->head->next;
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
      sp_console_list_prepend(self->impl->lines->head, line);
    }
    self->impl->lines->count++;
  }
  free(strings), strings = NULL;
}

const char * sp_console_get_current_command(const sp_console * self) {
  return self->impl->current_command;
}

void sp_console_clear_current_command(const sp_console * self) {
  free(self->impl->current_command), self->impl->current_command = NULL;
}

void sp_console_clear_console(const sp_console * self) {
  sp_console_impl * impl = self->impl;
  if(impl && impl->lines && impl->lines->head) {
    sp_console_line * t = impl->lines->head->next;
    while(t != impl->lines->head) {
      if(t->line != NULL) {
        free(t->line), t->line = NULL;
      }
      sp_console_line * old = t;
      t = t->next;
      free(old), old = NULL;
    }
    free(impl->lines->head), impl->lines->head = NULL;
  }
  free(impl->text), impl->text = NULL;
  impl->text_capacity = sp_console_max_input_len;
  impl->text_len = 0;
}

