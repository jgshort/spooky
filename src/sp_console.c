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

static const int console_direction = 73;
static const int console_height = 500;

typedef struct spooky_console_line spooky_console_line;
typedef struct spooky_console_line {
  spooky_console_line * next;
  spooky_console_line * prev;
  size_t line_len;
  char * line;
} spooky_console_line;

static spooky_console_line SPOOKY_CONSOLE_LINE_HEAD = {
  .next = &SPOOKY_CONSOLE_LINE_HEAD,
  .prev = &SPOOKY_CONSOLE_LINE_HEAD,
  .line_len = 0,
  .line = NULL
};

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
} spooky_console_data;

static void spooky_console_handle_event(const spooky_base * self, SDL_Event * event);
static void spooky_console_handle_delta(const spooky_base * self, int64_t last_update_time, double interpolation);
static void spooky_console_render(const spooky_base * self, SDL_Renderer * renderer);
static void spooky_console_push_str(const spooky_console * self, const char * str);
static void spooky_console_list_prepend(spooky_console_line * X, spooky_console_line * P);

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
  
  data->lines = calloc(1, sizeof * data->lines);
  data->lines->head = &SPOOKY_CONSOLE_LINE_HEAD;
  data->lines->count = 0;

  int w, h;
  if(SDL_GetRendererOutputSize(data->context->get_renderer(data->context), &w, &h) == 0) {
    data->rect.w = w - 200;
  }

  ((spooky_console *)(uintptr_t)self)->data = data;

  return self;
}

const spooky_console * spooky_console_dtor(const spooky_console * self) {
  if(self) {
    spooky_console_data * data = self->data;

    spooky_console_line * H = &SPOOKY_CONSOLE_LINE_HEAD;
    spooky_console_line * t = H->next;
    if(t != H) {
      do {
        if(t->line_len > 0 && t->line != NULL) {
          free(t->line), t->line = NULL;
        }
        t = t->next;
        if(t != H) {
          free(t->prev), t->prev = NULL;
        }
      } while(t != H);
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
    SDL_Point text_dest = { .x = data->rect.x + 10, .y =  data->rect.y + data->rect.h - (line_skip * ((int)data->lines->count + 1)) - 10};

    SDL_Color color = { .r = 0, .g = 255, .b = 0, .a = 255};
    
    spooky_console_line * H = &SPOOKY_CONSOLE_LINE_HEAD;
    spooky_console_line * t = H->next;

    if(t != H) {
      do {
        if(t->line_len > 0 && t->line != NULL) {
          font->write_to_renderer(font, renderer, &text_dest, &color, t->line, NULL, NULL);
        }
        t = t->next;
      } while(t != H);
    }

    if(data->hide_cursor) { 
      font->write_to_renderer(font, renderer, &dest, &color, "$", NULL, NULL);
    } else {
      font->write_to_renderer(font, renderer, &dest, &color, "$ _", NULL, NULL);
    }
  }
}

static void spooky_console_list_prepend(spooky_console_line * X, spooky_console_line * P){
  P->next = X;
  P->prev = X->prev;
  X->prev->next = P;
  X->prev = P;
}

void spooky_console_push_str(const spooky_console * self, const char * str) {
  spooky_console_line * line = calloc(1, sizeof * line);
  line->line = strndup(str, 80);
  line->line_len = strnlen(str, 80);
  spooky_console_list_prepend(self->data->lines->head, line);
  self->data->lines->count++;
}

