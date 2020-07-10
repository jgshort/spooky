#define _POSIX_C_SOURCE 200809L

#include <limits.h>
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include "config.h"
#include "sp_gui.h"
#include "sp_base.h"
#include "sp_context.h"
#include "sp_console.h"

static const size_t spooky_console_max_line_capacity = 1024;
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
  SDL_Rect rect;
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

  self->ctor = &spooky_console_ctor;
  self->dtor = &spooky_console_dtor;
  self->free = &spooky_console_free;
  self->release = &spooky_console_release;

  self->super.handle_event = &spooky_console_handle_event;
  self->super.handle_delta = &spooky_console_handle_delta;
  self->super.render = &spooky_console_render;

  self->push_str = &spooky_console_push_str;
  self->get_current_command = &spooky_console_get_current_command;
  self->clear_current_command = &spooky_console_clear_current_command;
  self->clear_console = &spooky_console_clear_console;
  return self;
}

const spooky_console * spooky_console_acquire() {
  return spooky_console_init((spooky_console *)(uintptr_t)spooky_console_alloc());
}

const spooky_console * spooky_console_ctor(const spooky_console * self, const spooky_context * context, SDL_Renderer * renderer) {
  assert(self != NULL);
  assert(context != NULL && renderer != NULL);

  self = (spooky_console *)(uintptr_t)spooky_base_ctor((spooky_base *)(uintptr_t)self);

  spooky_console_impl * impl = calloc(1, sizeof * impl);
  if(!impl) { abort(); }
 
  impl->context = context;
  impl->direction = -spooky_console_animation_speed;
  impl->rect = (SDL_Rect){ .x = 0, .y = 0, .w = 0, .h = 0 };
  impl->show_console = false;
  impl->is_animating = false;
  impl->hide_cursor = true;
  
  impl->lines = calloc(1, sizeof * impl->lines);
  impl->lines->head = NULL;
  impl->lines->count = 0;

  impl->current_command = NULL;
  impl->text_len = 0;
  impl->text_capacity = spooky_console_max_line_capacity;
  impl->text = calloc(impl->text_capacity, sizeof * impl->text);;

  int w, h;
  if(SDL_GetRendererOutputSize(impl->context->get_renderer(impl->context), &w, &h) == 0) {
    /* max width = renderer_width - some_margin */
    const spooky_font * font = context->get_font(context);
    int margin = (font->get_m_dash(font) - 1) * 5;
    impl->rect.w = w - (margin * 2);
    impl->rect.h = h - (margin / 2);
    impl->rect.y = -impl->rect.h;
    impl->rect.x = margin;
  }

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

bool spooky_console_handle_event(const spooky_base * self, SDL_Event * event) {
  spooky_console_impl * impl = ((const spooky_console *)self)->impl;

  bool close_console = false;
  if(impl->show_console) {
    if(event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_BACKSPACE) {
      if(impl->text_len > 0) {
        impl->text_len--;
        impl->text[impl->text_len] = '\0';
      }
    }
    else if(event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_RETURN) {
      impl->current_command = strndup(impl->text, spooky_console_max_input_len);
      spooky_console_push_str_impl((const spooky_console *)self, impl->text, true);
      free(impl->text), impl->text = NULL;
      impl->text_len = 0;
      impl->text_capacity = spooky_console_max_line_capacity;
      impl->text = calloc(impl->text_capacity, sizeof * impl->text);
    }
    else if(event->type == SDL_TEXTINPUT) {
      /* accumulate command text*/
      if(event->text.text[0] != '`') {
        size_t input_len = strnlen(event->text.text, spooky_console_max_input_len);
        if(input_len + impl->text_len <= spooky_console_max_input_len) {
          if(impl->text_len + input_len > impl->text_capacity) {
            impl->text_capacity += spooky_console_max_line_capacity;
            char * temp = realloc(impl->text, impl->text_capacity * sizeof * impl->text);
            if(temp == NULL) {
              abort();
            } 
            impl->text = temp;
          }
          
          strncat(impl->text, event->text.text, spooky_console_max_input_len);
          
          size_t new_len = input_len + impl->text_len;
          impl->text_len = new_len;
          assert(impl->text_len <= spooky_console_max_input_len);
          return true;
        }
      } else {
        close_console = true;
      }
    } 
  }
  if(close_console || (!impl->show_console && event->type == SDL_KEYUP && event->key.keysym.sym == SDLK_BACKQUOTE)) {
    if(!impl->is_animating) { impl->show_console = !impl->show_console; }
    impl->direction = impl->direction < 0 ? spooky_console_animation_speed : -spooky_console_animation_speed;
    impl->is_animating = impl->rect.y + impl->rect.h > 0 || impl->rect.y + impl->rect.h < impl->rect.h;
  }
  else if(event->type == SDL_WINDOWEVENT && (
               event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED 
            || event->window.event == SDL_WINDOWEVENT_MOVED 
            || event->window.event == SDL_WINDOWEVENT_RESIZED
            || event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED
            || event->window.event == SDL_WINDOWEVENT_EXPOSED
            /* Only happens when clicking About in OS X Mojave */
            || event->window.event == SDL_WINDOWEVENT_FOCUS_LOST
        ))
  {
    int w, h;
    if(SDL_GetRendererOutputSize(impl->context->get_renderer(impl->context), &w, &h) == 0) {
      const spooky_font * font = impl->context->get_font(impl->context);
      int margin = (font->get_m_dash(font) - 1) * 5;
      impl->rect.w = w - (margin * 2);
      impl->rect.x = margin;
      impl->rect.h = h - (margin / 2);
    }
  }
  return impl->show_console;
}

void spooky_console_handle_delta(const spooky_base * self, int64_t last_update_time, double interpolation) {
  spooky_console_impl * impl = ((const spooky_console *)self)->impl;

  if(impl->show_console) {
    impl->rect.y += (int)floor((double)impl->direction * interpolation); 

    if(impl->rect.y < 0 - impl->rect.h) { impl->rect.y = 0 - impl->rect.h; }
    if(impl->rect.y > 0) { impl->rect.y = 0; }

    impl->hide_cursor = (last_update_time / 650000000) % 2 == 0;
  }
}

void spooky_console_render(const spooky_base * self, SDL_Renderer * renderer) {
  spooky_console_impl * impl = ((const spooky_console *)self)->impl;

  if(impl->show_console) {
    uint8_t r, g, b, a;
    SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);

    SDL_BlendMode blend_mode;
    SDL_GetRenderDrawBlendMode(renderer, &blend_mode);
   
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 173);
    SDL_RenderFillRect(renderer, &(impl->rect));
   
    SDL_SetRenderDrawBlendMode(renderer, blend_mode);
    SDL_SetRenderDrawColor(renderer, r, g, b, a);

    const spooky_font * font = impl->context->get_font(impl->context);
    int line_skip = font->get_line_skip(font);
    SDL_Point dest = { .x = impl->rect.x + 10, .y =  impl->rect.y + impl->rect.h - line_skip - 10};
    assert(impl->lines->count < INT_MAX);

    static const SDL_Color color = { .r = 0, .g = 255, .b = 0, .a = 255};
    
    int line_next = 2;
    
    spooky_console_line * t = impl->lines->head;
    while(t != NULL) {
      t = t->prev;
      if(t->line_len > 0 && t->line != NULL) {
        SDL_Point text_dest = { .x = impl->rect.x + 10, .y =  impl->rect.y + impl->rect.h - (line_skip * (line_next)) - 10};
        if(t->is_command) {
          static const SDL_Color command_color = { .r = 255, .g = 0x55, .b = 255, .a = 255 };
          int command_width;
          font->write_to_renderer(font, renderer, &text_dest, &command_color, "> ", &command_width, NULL);
          text_dest.x += command_width;
          font->write_to_renderer(font, renderer, &text_dest, &command_color, t->line, NULL, NULL);
        } else {
          font->write_to_renderer(font, renderer, &text_dest, &color, t->line, NULL, NULL);
        }
        line_next++;
      }
      if(t == impl->lines->head) { break; }
    }

    // Draw prompt
    font->write_to_renderer(font, renderer, &dest, &color, "$", NULL, NULL);

    /* Draw command accumulator */
    static const SDL_Color command_color = { .r = 255, .g = 255, .b = 0, .a = 255 };
    
    int text_w, text_h;
    font->measure_text(font, impl->text, &text_w, &text_h);

    dest.x += font->get_m_dash(font) * 2;
    if(impl->text != NULL) {
      int max_rect_width = impl->rect.w - ((font->get_m_dash(font)) * 3);
      if(max_rect_width < text_w) {
        /* only draw text that will fit within the console window */
        int max_chars = (int)((float)max_rect_width / ((float)text_w / (float)impl->text_len)) - 3; /* -3 for '<<<' */
        int total_len = (int)impl->text_len;
        int offset = total_len - max_chars;
        static const SDL_Color chevron_color = { .r = 255, .g = 0, .b = 255, .a = 255 };
        int chevron_w;
        font->write_to_renderer(font, renderer, &dest, &chevron_color, "<<<", &chevron_w, NULL);
        dest.x += chevron_w;
       
        {
          char * text = calloc(spooky_console_max_line_capacity, sizeof * text);
          if(!text) { abort(); }
          snprintf(text, spooky_console_max_input_len, "%s", impl->text + offset);
          int text_width;
          font->write_to_renderer(font, renderer, &dest, &command_color, text, &text_width, NULL);
          free(text), text = NULL;
          dest.x += text_width;
        }
      } else {
        int text_width;
        font->write_to_renderer(font, renderer, &dest, &command_color, impl->text, &text_width, NULL);
        dest.x += text_width;
      }
    }

    /* Draw blinking cursor */
    if(impl->hide_cursor) { 
    } else {
      font->write_to_renderer(font, renderer, &dest, &command_color, "_", NULL, NULL);
    }
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

void spooky_console_push_str_impl(const spooky_console * self, const char * str, bool is_command) {
  size_t strings_capacity = 128;
  char ** strings = calloc(strings_capacity, sizeof * strings);
  if(strings == NULL) { abort(); }
  size_t split_count = 0;
  
  char * working_str = strndup(str, spooky_console_max_line_capacity);
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
    strings[split_count] = strndup(token, spooky_console_max_line_capacity); 
    split_count++;
    ws = NULL;
  }

  free(working_str), working_str = NULL;
  for(size_t split = 0; split < split_count; ++split) {
    spooky_console_line * line = calloc(1, sizeof * line);
    char * string = strings[split];
    if(string == NULL) { continue; }
    line->line = string;
    line->line_len = strnlen(string, spooky_console_max_line_capacity);
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

  free(impl->text), impl->text = NULL;
  impl->text_capacity =spooky_console_max_line_capacity;
  impl->text = calloc(impl->text_capacity, sizeof * impl->text);
  impl->text_len = 0;
}
