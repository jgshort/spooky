#ifndef SPOOKY_FONT__H
#define SPOOKY_FONT__H

#include <stdbool.h>
#include "sp_gui.h"

#define SPOOKY_FONT_MAX_TYPES 3

typedef char spooky_text;

extern const int spooky_default_font_size;
extern const char * spooky_default_font_names[SPOOKY_FONT_MAX_TYPES];

typedef enum spooky_font_line_adornment {
  SFLA_NONE      = 0x0000,
  SFLA_PLAINTEXT = 0x0001,
  SFLA_UNDERLINE = 0x0002,
  SFLA_BOLD      = 0x0004,
  SFLA_ITALIC    = 0x0008,
  SFLA_EOE       = 0xffff
} spooky_font_line_adornment;

const char * spooky_font_line_adornment_to_string(spooky_font_line_adornment adornment);

typedef struct spooky_font_line spooky_font_line;
typedef struct spooky_font_line {
  spooky_font_line * next;
  const spooky_text * line;
  size_t line_len;
  spooky_font_line_adornment adornment;
  bool has_newline;
  char padding[3];
} spooky_font_line;

typedef struct spooky_font_line_chunk {
  spooky_text * source_buffer; /* to be freed */
  spooky_font_line * source_lines; /* to be freed */

  size_t lines_capacity;
  size_t lines_count;
  spooky_font_line * lines;
  size_t newline_count;

  const spooky_text * text;

  size_t text_len;
  char padding[4];
  spooky_font_line_adornment adornment;
} spooky_font_line_chunk;

struct spooky_font_data;
typedef struct spooky_font spooky_font;

typedef struct spooky_font {
  const spooky_font * (*ctor)(const spooky_font * self, SDL_Renderer * renderer, const void * mem, size_t mem_len, int point_size);
  const spooky_font * (*dtor)(const spooky_font * self);
  void (*free)(const spooky_font * self);
  void (*release)(const spooky_font * self);
  int (*putchar)(const spooky_font * self, const SDL_Point * destination, const SDL_Color * color, spooky_font_line_adornment adornment, const spooky_text * text, int * advance);
  int (*putchar_renderer)(const spooky_font * self, SDL_Renderer * renderer, const SDL_Point * destination, const SDL_Color * color, spooky_font_line_adornment adornment, const spooky_text * text, int * advance);
  void (*write)(const spooky_font * self, const SDL_Point * destination, const SDL_Color * color, const char * text, size_t text_len, int * w, int * h);
  void (*write_to_renderer)(const spooky_font * self, SDL_Renderer * renderer, const SDL_Point * destination, const SDL_Color * color, const char * text, size_t text_len, int * w, int * h);
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
  void (*measure_text)(const spooky_font * self, const char * text, size_t text_len, int * width, int * height);
  bool (*get_is_drop_shadow)(const spooky_font * self);
  void (*set_is_drop_shadow)(const spooky_font * self, bool is_drop_shadow);
  bool (*get_enable_orthographic_ligatures)(const spooky_font * self);
  void (*set_enable_orthographic_ligatures)(const spooky_font * self, bool enable_orthographic_ligatures);
  void (*set_drop_x)(const spooky_font * self, int drop_x);
  int  (*get_drop_x)(const spooky_font * self);
  void (*set_drop_y)(const spooky_font * self, int drop_y);
  int (*get_drop_y)(const spooky_font * self);
  int (*get_point_size)(const spooky_font * self);
  int (*get_glyph_advance)(const spooky_font * self, const spooky_text * text);

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

TTF_Font * spooky_font_open_font(const char * file_path, int point_size);

void spooky_font_read_file_to_buf(const char * file_path, void ** out_buf, size_t * out_buf_len);
void spooky_font_parse_text(const spooky_text * text, size_t text_len, const spooky_font_line_chunk ** chunks, size_t * chunks_len);
uint32_t spooky_font_get_code_point(const spooky_text * s, int * skip);
size_t spooky_font_count_new_lines(const spooky_text * text, size_t text_len);

#endif /* SPOOKY_FONT__H */
