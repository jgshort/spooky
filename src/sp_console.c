#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include "config.h"
#include "sp_gui.h"
#include "sp_base.h"
#include "sp_context.h"
#include "sp_console.h"

static const size_t max_buf_len = 1048576;
static const int console_direction = 73;
static const int console_height = 500;

typedef struct spooky_console_data {
  const spooky_context * context;
  size_t buf_capacity;
  size_t buf_len;
  char * buf;
  char * input;
  int line_count;
  SDL_Rect rect;
  int direction;
  bool show_console;
  bool is_animating;
  bool hide_cursor;
  char padding[5]; /* not portable */
} spooky_console_data;

static void spooky_console_handle_event(const spooky_base * self, SDL_Event * event);
static void spooky_console_handle_delta(const spooky_base * self, int64_t last_update_time, double interpolation);
static void spooky_console_render(const spooky_base * self, SDL_Renderer * renderer);
static void spooky_console_push_str(const spooky_console * self, const char * str);

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
  data->direction = -console_direction;
  data->rect = (SDL_Rect){.x = 100, .y = -300, .w = 0, .h = console_height };
  data->show_console = false;
  data->is_animating = false;
  data->hide_cursor = true;
  
  data->buf_capacity = 65536;
  data->buf = calloc(data->buf_capacity, sizeof * data->buf);
  data->line_count = 1;
  data->buf_len = 0;

  int w, h;
  SDL_GetRendererOutputSize(renderer, &w, &h);
  data->rect.w = w - 200;

  ((spooky_console *)(uintptr_t)self)->data = data;

  return self;
}

const spooky_console * spooky_console_dtor(const spooky_console * self) {
  if(self) {
    spooky_console_data * data = self->data;
    if(data->buf != NULL) {
      free(data->buf), data->buf = NULL;
    }
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

void spooky_console_handle_event(const spooky_base * self, SDL_Event * event) {
  spooky_console_data * data = ((const spooky_console *)self)->data;
  if(event->type == SDL_KEYUP && event->key.keysym.sym == SDLK_BACKQUOTE) {
    if(!data->is_animating) { data->show_console = !data->show_console; }
    data->direction = data->direction < 0 ? console_direction : -console_direction;
    data->is_animating = data->rect.y + data->rect.h > 0 || data->rect.y + data->rect.h < data->rect.h;
  } 
  if (event->type == SDL_WINDOWEVENT && (
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
      data->rect.w = w - 200;
    }
  }
}

void spooky_console_handle_delta(const spooky_base * self, int64_t last_update_time, double interpolation) {
  spooky_console_data * data = ((const spooky_console *)self)->data;

  if(data->show_console) {
    data->rect.y += (int)floor((double)data->direction * interpolation); 

    if(data->rect.y < 0 - data->rect.h) { data->rect.y = 0 - data->rect.h; }
    if(data->rect.y > 0) { data->rect.y = 0; }

    data->hide_cursor = (last_update_time / 650000000) % 2 == 0;//!data->hide_cursor;
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
    SDL_Point text_dest = { .x = data->rect.x + 10, .y =  data->rect.y + data->rect.h - (line_skip * data->line_count) - 10};

    SDL_Color color = { .r = 0, .g = 255, .b = 0, .a = 255};
    font->write_to_renderer(font, renderer, &text_dest, &color, data->buf, NULL, NULL);
    if(data->hide_cursor) { 
      font->write_to_renderer(font, renderer, &dest, &color, "$", NULL, NULL);
    } else {
      font->write_to_renderer(font, renderer, &dest, &color, "$ _", NULL, NULL);
    }
  }
}

void spooky_console_push_str(const spooky_console * self, const char * str) {
  size_t buf_len = self->data->buf_len;
  size_t str_len = strnlen(str, 1024);
  if(self->data->buf_len + str_len > self->data->buf_capacity) {
    if(buf_len > max_buf_len) {
      abort(); 
    }
    self->data->buf_capacity += 65536;
    char * temp = realloc(self->data->buf, self->data->buf_capacity);
    if(temp != NULL) {
      self->data->buf = temp; 
    } else { 
      abort();
    }
  }
  
  self->data->buf_len += str_len;

  const char * s = str;
  while(s != NULL && *s != '\0') {
    if(*s == '\n') { self->data->line_count++; }
    s++;
  }

  snprintf(self->data->buf, self->data->buf_len, "%s%s", self->data->buf, str);
}

