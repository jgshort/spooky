#ifndef SPOOKY_FONT__H
#define SPOOKY_FONT__H

#include "sp_gui.h"

typedef struct spooky_point {
  int x;
  int y;
} spooky_point;

typedef struct spooky_glyph spooky_glyph;
typedef struct spooky_font_data spooky_font_data;
typedef struct spooky_font spooky_font;

typedef struct spooky_font {
  const spooky_font * (*ctor)(const spooky_font * self, SDL_Renderer * renderer, TTF_Font * font);
  const spooky_font * (*dtor)(const spooky_font * self);
  void (*free)(const spooky_font * self);
  void (*release)(const spooky_font * self);

  const spooky_font * (*write)(const spooky_font * self, const spooky_point * destination, const SDL_Color * color, const char * s, int * w, int * h); 
  const spooky_font * (*write_to_renderer)(const spooky_font * self, SDL_Renderer * renderer, const spooky_point * destination, const SDL_Color * color, const char * s, int * w, int * h);

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

  //SDL_Texture * (*glyph_create_texture)(SDL_Renderer * renderer, TTF_Font * font, const char * text);
  //void (*glyph_destroy_texture)(spooky_glyph * glyph);
  //spooky_glyph * (*glyph_create)(TTF_Font * font, uint32_t character, spooky_glyph * glyph);

  bool (*get_is_drop_shadow)(const spooky_font * self);
  void (*set_is_drop_shadow)(const spooky_font * self, bool is_drop_shadow);
  void (*set_drop_x)(const spooky_font * self, int drop_x);
  int  (*get_drop_x)(const spooky_font * self);
  void (*set_drop_y)(const spooky_font * self, int drop_y);
  int (*get_drop_y)(const spooky_font * self);

  spooky_font_data * data;
} spooky_font;

/* Allocate (malloc) interface */
const spooky_font * spooky_font_alloc();
/* Initialize interface methods */
const spooky_font * spooky_font_init(spooky_font * self);
/* Allocate and initialize interface methods */
const spooky_font * spooky_font_acquire();

/* Construct data */
const spooky_font * spooky_font_ctor(const spooky_font * self, SDL_Renderer * renderer, TTF_Font * font);

/* Destruct (dtor) data */
const spooky_font * spooky_font_dtor(const spooky_font * self);

/* Free interface */ 
void spooky_font_free(const spooky_font * self);
/* Destruct and free interface */
void spooky_font_release(const spooky_font * self);

const spooky_font * spooky_font_load_from_memory(SDL_Renderer * renderer, int pointSize, const char * memory, size_t size);

TTF_Font * spooky_font_open_font(const char * file_path, int point_size);
uint32_t spooky_font_get_code_point(const char * s, int * skip);

#endif /* SPOOKY_FONT__H */


