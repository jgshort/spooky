#ifndef SP_FONT__H
#define SP_FONT__H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "sp_gui.h"

#define SP_FONT_MAX_TYPES 3

  typedef char sp_char;

  extern const int sp_default_font_size;
  extern const char * sp_default_font_names[SP_FONT_MAX_TYPES];

  typedef enum sp_font_line_adornment {
    SFLA_NONE      = 0x0000,
    SFLA_PLAINTEXT = 0x0001,
    SFLA_UNDERLINE = 0x0002,
    SFLA_BOLD      = 0x0004,
    SFLA_ITALIC    = 0x0008,
    SFLA_EOE       = 0xffff
  } sp_font_line_adornment;

  const char * sp_font_line_adornment_to_string(sp_font_line_adornment adornment);

  typedef struct sp_font_line sp_font_line;
  typedef struct sp_font_line {
    sp_font_line * next;
    const sp_char * line;
    size_t line_len;
    sp_font_line_adornment adornment;
    bool has_newline;
    char padding[3];
  } sp_font_line;

  typedef struct sp_font_line_chunk {
    sp_char * source_buffer; /* to be freed */
    sp_font_line * source_lines; /* to be freed */

    size_t lines_capacity;
    size_t lines_count;
    sp_font_line * lines;
    size_t newline_count;

    const sp_char * text;

    size_t text_len;
    char padding[4];
    sp_font_line_adornment adornment;
  } sp_font_line_chunk;

  struct sp_font_data;
  typedef struct sp_font sp_font;

  typedef struct sp_font {
    const sp_font * (*ctor)(const sp_font * self, SDL_Renderer * renderer, const void * mem, size_t mem_len, int point_size);
    const sp_font * (*dtor)(const sp_font * self);
    void (*free)(const sp_font * self);
    void (*release)(const sp_font * self);
    int (*putchar)(const sp_font * self, const SDL_Point * destination, const SDL_Color * color, sp_font_line_adornment adornment, const sp_char * text, int * advance);
    int (*putchar_renderer)(const sp_font * self, SDL_Renderer * renderer, const SDL_Point * destination, const SDL_Color * color, sp_font_line_adornment adornment, const sp_char * text, int * advance);
    void (*write)(const sp_font * self, const SDL_Point * destination, const SDL_Color * color, const char * text, size_t text_len, int * w, int * h);
    void (*write_to_renderer)(const sp_font * self, SDL_Renderer * renderer, const SDL_Point * destination, const SDL_Color * color, const char * text, size_t text_len, int * w, int * h);
    const char * (*get_name)(const sp_font * self);
    int (*get_height)(const sp_font * self);
    int (*get_ascent)(const sp_font * self);
    int (*get_descent)(const sp_font * self);
    int (*get_line_skip)(const sp_font * self);
    int (*get_m_dash)(const sp_font * self);
    int (*get_height_line_skip_difference)(const sp_font * self);
    int (*get_height_line_skip_delta)(const sp_font * self);
    int (*nearest_x)(const sp_font * self, int x);
    int (*nearest_y)(const sp_font * self, int y);
    void (*measure_text)(const sp_font * self, const char * text, size_t text_len, int * width, int * height);
    bool (*get_is_drop_shadow)(const sp_font * self);
    void (*set_is_drop_shadow)(const sp_font * self, bool is_drop_shadow);
    bool (*get_enable_orthographic_ligatures)(const sp_font * self);
    void (*set_enable_orthographic_ligatures)(const sp_font * self, bool enable_orthographic_ligatures);
    void (*set_drop_x)(const sp_font * self, int drop_x);
    int  (*get_drop_x)(const sp_font * self);
    void (*set_drop_y)(const sp_font * self, int drop_y);
    int (*get_drop_y)(const sp_font * self);
    int (*get_point_size)(const sp_font * self);
    int (*get_glyph_advance)(const sp_font * self, const sp_char * text);

    struct sp_font_data * data;
  } sp_font;

  /* Allocate (malloc) interface */
  const sp_font * sp_font_alloc();
  /* Initialize interface methods */
  const sp_font * sp_font_init(sp_font * self);
  /* Allocate and initialize interface methods */
  const sp_font * sp_font_acquire();

  /* Construct data */
  const sp_font * sp_font_ctor(const sp_font * self, SDL_Renderer * renderer, const void * mem, size_t mem_len, int point_size);

  /* Destruct data */
  const sp_font * sp_font_dtor(const sp_font * self);

  /* Free interface */
  void sp_font_free(const sp_font * self);
  /* Destruct and free interface */
  void sp_font_release(const sp_font * self);

  const sp_font * sp_font_load_from_memory(SDL_Renderer * renderer, int point_size, const char * memory, size_t size);

  TTF_Font * sp_font_open_font(const char * file_path, int point_size);

  void sp_font_read_file_to_buf(const char * file_path, void ** out_buf, size_t * out_buf_len);
  void sp_font_parse_text(const sp_char * text, size_t text_len, const sp_font_line_chunk ** chunks, size_t * chunks_len);
  uint32_t sp_font_get_code_point(const sp_char * s, int * skip);
  size_t sp_font_count_new_lines(const sp_char * text, size_t text_len);

#ifdef __cplusplus
}
#endif

#endif /* SP_FONT__H */

