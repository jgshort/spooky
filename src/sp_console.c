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

typedef struct spooky_console_data {
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
} spooky_console_data;

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

  spooky_console_data * data = calloc(1, sizeof * data);
  if(!data) { abort(); }
 
  data->context = context;
  data->direction = -spooky_console_animation_speed;
  data->rect = (SDL_Rect){ .x = 0, .y = 0, .w = 0, .h = 0 };
  data->show_console = false;
  data->is_animating = false;
  data->hide_cursor = true;
  
  data->lines = calloc(1, sizeof * data->lines);
  data->lines->head = NULL;
  data->lines->count = 0;

  data->current_command = NULL;
  data->text_len = 0;
  data->text_capacity = spooky_console_max_line_capacity;
  data->text = calloc(data->text_capacity, sizeof * data->text);;

  int w, h;
  if(SDL_GetRendererOutputSize(data->context->get_renderer(data->context), &w, &h) == 0) {
    /* max width = renderer_width - some_margin */
    const spooky_font * font = context->get_font(context);
    int margin = (font->get_m_dash(font) - 1) * 5;
    data->rect.w = w - margin;
    data->rect.h = h - (margin / 2);
    data->rect.y = -data->rect.h;
    data->rect.x = margin;
  }

  ((spooky_console *)(uintptr_t)self)->data = data;

  return self;
}

const spooky_console * spooky_console_dtor(const spooky_console * self) {
  if(self) {
    spooky_console_data * data = self->data;
    spooky_console_clear_console(self);
    free(data->current_command), data->current_command = NULL;
    free(data), data = NULL;
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
  spooky_console_data * data = ((const spooky_console *)self)->data;

  bool close_console = false;
  if(data->show_console) {
    if(event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_BACKSPACE) {
      if(data->text_len > 0) {
        data->text_len--;
        data->text[data->text_len] = '\0';
      }
    }
    else if(event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_RETURN) {
      data->current_command = strndup(data->text, spooky_console_max_input_len);
      spooky_console_push_str_impl((const spooky_console *)self, data->text, true);
      free(data->text), data->text = NULL;
      data->text_len = 0;
      data->text_capacity = spooky_console_max_line_capacity;
      data->text = calloc(data->text_capacity, sizeof * data->text);
    }
    else if(event->type == SDL_TEXTINPUT) {
      /* accumulate command text*/
      if(event->text.text[0] != '`') {
        size_t input_len = strnlen(event->text.text, spooky_console_max_input_len);
        if(input_len + data->text_len <= spooky_console_max_input_len) {
          if(data->text_len + input_len > data->text_capacity) {
            data->text_capacity += spooky_console_max_line_capacity;
            char * temp = realloc(data->text, data->text_capacity * sizeof * data->text);
            if(temp == NULL) {
              abort();
            } 
            data->text = temp;
          }
          
          strncat(data->text, event->text.text, spooky_console_max_input_len);
          
          size_t new_len = input_len + data->text_len;
          data->text_len = new_len;
          assert(data->text_len <= spooky_console_max_input_len);
          return true;
        }
      } else {
        close_console = true;
      }
    } 
  }
  if(close_console || (!data->show_console && event->type == SDL_KEYUP && event->key.keysym.sym == SDLK_BACKQUOTE)) {
    if(!data->is_animating) { data->show_console = !data->show_console; }
    data->direction = data->direction < 0 ? spooky_console_animation_speed : -spooky_console_animation_speed;
    data->is_animating = data->rect.y + data->rect.h > 0 || data->rect.y + data->rect.h < data->rect.h;
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
    if(SDL_GetRendererOutputSize(data->context->get_renderer(data->context), &w, &h) == 0) {
      const spooky_font * font = data->context->get_font(data->context);
      int margin = (font->get_m_dash(font) - 1) * 10;
      data->rect.w = w - margin;
      data->rect.h = h - (margin / 2);
    }
  }
  return data->show_console;
}

void spooky_console_handle_delta(const spooky_base * self, int64_t last_update_time, double interpolation) {
  spooky_console_data * data = ((const spooky_console *)self)->data;

  if(data->show_console) {
    data->rect.y += (int)floor((double)data->direction * interpolation); 

    if(data->rect.y < 0 - data->rect.h) { data->rect.y = 0 - data->rect.h; }
    if(data->rect.y > 0) { data->rect.y = 0; }

    data->hide_cursor = (last_update_time / 650000000) % 2 == 0;
  }
}

void spooky_console_render(const spooky_base * self, SDL_Renderer * renderer) {
  spooky_console_data * data = ((const spooky_console *)self)->data;

  if(data->show_console) {
    uint8_t r, g, b, a;
    SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);

    SDL_BlendMode blend_mode;
    SDL_GetRenderDrawBlendMode(renderer, &blend_mode);
   
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 173);
    SDL_RenderFillRect(renderer, &(data->rect));
   
    SDL_SetRenderDrawBlendMode(renderer, blend_mode);
    SDL_SetRenderDrawColor(renderer, r, g, b, a);

    const spooky_font * font = data->context->get_font(data->context);
    int line_skip = font->get_line_skip(font);
    SDL_Point dest = { .x = data->rect.x + 10, .y =  data->rect.y + data->rect.h - line_skip - 10};
    assert(data->lines->count < INT_MAX);

    static const SDL_Color color = { .r = 0, .g = 255, .b = 0, .a = 255};
    
    spooky_console_line * t = NULL;
    if(data->lines->head != NULL) {
      t = data->lines->head; 
    }

    int line_next = 2;
    while(t != NULL) {
      if(t->line_len > 0 && t->line != NULL) {
        SDL_Point text_dest = { .x = data->rect.x + 10, .y =  data->rect.y + data->rect.h - (line_skip * (line_next)) - 10};
        if(t->is_command) {
          static const SDL_Color command_color = { .r = 255, .g = 0, .b = 0x55, .a = 255 };
          int command_width;
          font->write_to_renderer(font, renderer, &text_dest, &command_color, "> ", &command_width, NULL);
          text_dest.x += command_width;
          font->write_to_renderer(font, renderer, &text_dest, &command_color, t->line, NULL, NULL);
        } else {
          font->write_to_renderer(font, renderer, &text_dest, &color, t->line, NULL, NULL);
        }
        line_next++;
      }
      t = t->prev;
      if(t == data->lines->head) { break; }
    }

    // Draw prompt
    font->write_to_renderer(font, renderer, &dest, &color, "$", NULL, NULL);

    /* Draw command accumulator */
    static const SDL_Color command_color = { .r = 255, .g = 255, .b = 0, .a = 255 };
    
    int text_w, text_h;
    font->measure_text(font, data->text, &text_w, &text_h);

    dest.x += font->get_m_dash(font) * 2;
    if(data->text != NULL) {
      int max_rect_width = data->rect.w - ((font->get_m_dash(font)) * 3);
      if(max_rect_width < text_w) {
        /* only draw text that will fit within the console window */
        int max_chars = (int)((float)max_rect_width / ((float)text_w / (float)data->text_len)) - 3; /* -3 for '<<<' */
        int total_len = (int)data->text_len;
        int offset = total_len - max_chars;
        static const SDL_Color chevron_color = { .r = 255, .g = 0, .b = 255, .a = 255 };
        int chevron_w;
        font->write_to_renderer(font, renderer, &dest, &chevron_color, "<<<", &chevron_w, NULL);
        dest.x += chevron_w;
       
        {
          char * text = calloc(spooky_console_max_line_capacity, sizeof * text);
          if(!text) { abort(); }
          snprintf(text, spooky_console_max_input_len, "%s", data->text + offset);
          int text_width;
          font->write_to_renderer(font, renderer, &dest, &command_color, text, &text_width, NULL);
          free(text), text = NULL;
          dest.x += text_width;
        }
      } else {
        int text_width;
        font->write_to_renderer(font, renderer, &dest, &command_color, data->text, &text_width, NULL);
        dest.x += text_width;
      }
    }

    /* Draw blinking cursor */
    if(data->hide_cursor) { 
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
  spooky_console_line * line = calloc(1, sizeof * line);
  line->line = strndup(str, spooky_console_max_input_len);
  line->line_len = strnlen(str, spooky_console_max_input_len);
  line->is_command = is_command;
  if(self->data->lines->count > spooky_console_max_display_lines && self->data->lines->head->next != NULL) {
    spooky_console_line * deleted = self->data->lines->head->next;
    assert(deleted != NULL && deleted->next != NULL);
    if(deleted->next != NULL) {
      deleted->next->prev = self->data->lines->head;
      self->data->lines->head->next = deleted->next;
    }
    free(deleted), deleted = NULL;
  }
  if(self->data->lines->head == NULL) {
    line->next = line;
    line->prev = line;
    self->data->lines->head = line;
  } else {
    spooky_console_list_prepend(self->data->lines->head, line);
  }
  self->data->lines->count++;
}

const char * spooky_console_get_current_command(const spooky_console * self) {
  return self->data->current_command;
}

void spooky_console_clear_current_command(const spooky_console * self) {
  free(self->data->current_command), self->data->current_command = NULL;
}

void spooky_console_clear_console(const spooky_console * self) {
  spooky_console_data * data = self->data;

  spooky_console_line * t = data->lines->head;
  while(t != data->lines->head) {
    if(t->line != NULL) {
      free(t->line), t->line = NULL;
    }
    spooky_console_line * old = t;
    t = t->next;
    free(old), old = NULL;
  }
  free(data->lines->head), data->lines->head = NULL;

  free(data->text), data->text = NULL;
  data->text_capacity =spooky_console_max_line_capacity;
  data->text = calloc(data->text_capacity, sizeof * data->text);
  data->text_len = 0;
}
