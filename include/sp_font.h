#ifndef SPOOKY_FONT__H
#define SPOOKY_FONT__H

#include "sp_gui.h"

#define SPOOKY_FONT_MAX_TYPES 2

extern const int spooky_default_font_size;
extern const char * spooky_default_font_names[SPOOKY_FONT_MAX_TYPES];

struct spooky_font_data;
typedef struct spooky_font spooky_font;

typedef struct spooky_font {
  const spooky_font * (*ctor)(const spooky_font * self, SDL_Renderer * renderer, const void * mem, size_t mem_len, int point_size);
  const spooky_font * (*dtor)(const spooky_font * self);
  void (*free)(const spooky_font * self);
  void (*release)(const spooky_font * self);
  void (*write)(const spooky_font * self, const SDL_Point * destination, const SDL_Color * color, const char * s, int * w, int * h); 
  void (*write_to_renderer)(const spooky_font * self, SDL_Renderer * renderer, const SDL_Point * destination, const SDL_Color * color, const char * s, int * w, int * h);
  const char * (*get_name)(const spooky_font * self);
  int (*get_height)(const spooky_font * self);
  int (*get_ascent)(const spooky_font * self);
  int (*get_descent)(const spooky_font * self);
  int (*get_line_skip)(const spooky_font * self);
  int (*get_m_dash)(const spooky_font * self);
  int (*get_height_line_skip_difference)(const spooky_font * self);
  int (*get_height_line_skip_delta)(const spooky_font * self);
  int (*nearest_x)(const spooky_font * self, int x);
  int (*nearest_y)(const spooky_font * self, int y);
  void (*measure_text)(const spooky_font * self, const char * text, int * width, int * height);
  bool (*get_is_drop_shadow)(const spooky_font * self);
  void (*set_is_drop_shadow)(const spooky_font * self, bool is_drop_shadow);
  void (*set_drop_x)(const spooky_font * self, int drop_x);
  int  (*get_drop_x)(const spooky_font * self);
  void (*set_drop_y)(const spooky_font * self, int drop_y);
  int (*get_drop_y)(const spooky_font * self);
  int (*get_point_size)(const spooky_font * self);
  struct spooky_font_data * data;
} spooky_font;

/* Allocate (malloc) interface */
const spooky_font * spooky_font_alloc();
/* Initialize interface methods */
const spooky_font * spooky_font_init(spooky_font * self);
/* Allocate and initialize interface methods */
const spooky_font * spooky_font_acquire();

/* Construct data */
const spooky_font * spooky_font_ctor(const spooky_font * self, SDL_Renderer * renderer, const void * mem, size_t mem_len, int point_size);

/* Destruct data */
const spooky_font * spooky_font_dtor(const spooky_font * self);

/* Free interface */ 
void spooky_font_free(const spooky_font * self);
/* Destruct and free interface */
void spooky_font_release(const spooky_font * self);

const spooky_font * spooky_font_load_from_memory(SDL_Renderer * renderer, int point_size, const char * memory, size_t size);

#endif /* SPOOKY_FONT__H */

