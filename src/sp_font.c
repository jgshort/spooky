#define _POSIX_C_SOURCE 200809L

// ENABLE orthographic ligatures: #define SP_FONT_ORTHOGRAPHIC_LIGATURE

#include <stdlib.h>
#include <memory.h>
#include <errno.h>
#include <limits.h>
#include <assert.h>
#include <memory.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "../include/sp_str.h"
#include "../include/sp_error.h"
#include "../include/sp_math.h"
#include "../include/sp_font.h"

static const size_t SP_MAX_STRING_LEN = 4096;

const char * sp_default_font_names[SP_FONT_MAX_TYPES] = {
  "print.char",
  "deja.sans",
  "pr.number"
};

static const size_t sp_glyphs_alloc_unit = UCHAR_MAX;

const int sp_default_font_size = 1;

const char * sp_font_line_adornment_to_string(sp_font_line_adornment adornment) {
  switch(adornment) {
    case SFLA_NONE: return "none";
    case SFLA_PLAINTEXT: return "plain text";
    case SFLA_UNDERLINE: return "underline";
    case SFLA_BOLD: return "bold";
    case SFLA_ITALIC: return "italic";
    case SFLA_EOE:
    default: return "invalid adornment";
  }
}

typedef struct sp_glyph {
  SDL_Texture * texture;
  SDL_Texture * fg_texture;
  sp_char * c;
  int advance;
  char padding[4];
} sp_glyph;

typedef struct sp_font_data {
  SDL_Renderer * renderer;
  TTF_Font * font;
  SDL_RWops * stream;

  size_t glyph_text_buf_count;
  size_t glyph_text_buf_capacity;
  sp_char * glyph_text_buf;
  sp_char * glyph_text_next;

  const void * memory;
  const char * name;
  size_t memory_len;
  int point_size;
  int height;
  int ascent;
  int descent;
  int line_skip;
  int m_dash;
  int drop_x;
  int drop_y;
  bool is_drop_shadow;
  bool enable_orthographic_ligatures;
  char padding[6]; /* not portable */
  /* glyph array */
  size_t glyphs_capacity;
  size_t glyphs_count;
  sp_glyph * glyphs;
} sp_font_data;

static const sp_glyph * sp_font_add_glyph(const sp_font * self, int skip, sp_char glyph_to_find[4]);
static int sp_font_string_to_bytes(const sp_font * self, const sp_char * text, sp_char bytes[4], int * text_skip);
const sp_font * sp_font_cctor(const sp_font * self, SDL_Renderer * renderer, int point_size, const void * memory, size_t memory_len, SDL_RWops * stream, TTF_Font * ttf_font);
static void parse_chunk_lines(const sp_char * text, size_t text_len, sp_font_line_chunk * chunks, size_t chunks_len);
static int sp_font_get_point_size(const sp_font * self);
int sp_font_putchar(const sp_font * self, const SDL_Point * destination, const SDL_Color * color, sp_font_line_adornment adornment, const sp_char * text, int * advance);
int sp_font_putchar_renderer(const sp_font * self, SDL_Renderer * renderer, const SDL_Point * destination, const SDL_Color * color, sp_font_line_adornment adornment, const sp_char * text, int * advance);
static errno_t sp_font_glyph_create_texture(const sp_font * self, const char * text, SDL_Texture ** out_texture, SDL_Texture ** out_fg_texture);
//static sp_glyph * sp_font_glyph_create(const sp_font * self, uint32_t character, sp_glyph * glyph);

static void sp_font_write(const sp_font * self, const SDL_Point * destination, const SDL_Color * color, const char * text, size_t text_len, int * w, int * h);
static void sp_font_write_to_renderer(const sp_font * self, SDL_Renderer * renderer, const SDL_Point * destination, const SDL_Color * color, const char * text, size_t text_len, int * w, int * h);

static int sp_glyph_compare(const void * a, const void * b);
static int sp_font_get_glyph_advance(const sp_font * self, const sp_char * text);
static const char * sp_font_get_name(const sp_font * self);
static int sp_font_get_height(const sp_font * self);
static int sp_font_get_ascent(const sp_font * self);
static int sp_font_get_descent(const sp_font * self);
static int sp_font_get_line_skip(const sp_font * self);
static int sp_font_get_m_dash(const sp_font * self);
static int sp_font_get_height_line_skip_difference(const sp_font * self);
static int sp_font_get_height_line_skip_delta(const sp_font * self);
static int sp_font_nearest_x(const sp_font * self, const int x);
static int sp_font_nearest_y(const sp_font * self, const int y);
static void sp_font_measure_text(const sp_font * self, const sp_char * text, size_t text_len, int * width, int * height);

static void sp_font_set_font_attributes(const sp_font * self);

static bool sp_font_get_is_drop_shadow(const sp_font * self);
static void sp_font_set_is_drop_shadow(const sp_font * self, bool is_drop_shadow);
static bool sp_font_get_enable_orthographic_ligatures(const sp_font * self);
static void sp_font_set_enable_orthographic_ligatures(const sp_font * self, bool enable_orthographic_ligatures);
static void sp_font_set_drop_x(const sp_font * self, int drop_x);
static int sp_font_get_drop_x(const sp_font * self);
static void sp_font_set_drop_y(const sp_font * self, int drop_y);
static int sp_font_get_drop_y(const sp_font * self);

TTF_Font * sp_font_open_font(const char * file_path, int point_size) {
  TTF_Font * temp = TTF_OpenFont((const char *)file_path, point_size);
  if (!temp) {
    fprintf(stderr, "Unable to load font '%s'. %s\n", file_path, TTF_GetError());
    abort();
  }
  return temp;
}

const sp_font * sp_font_alloc(void) {
  sp_font * self = calloc(1, sizeof * self);
  if(self == NULL) {
    fprintf(stderr, "Unable to allocate memory.");
    abort();
  }
  return self;
}

const sp_font * sp_font_init(sp_font * self) {
  assert(self != NULL);
  if(!self) { abort(); }

  self->ctor = &sp_font_ctor;
  self->dtor = &sp_font_dtor;
  self->free = &sp_font_free;
  self->release = &sp_font_release;
  self->putchar = &sp_font_putchar;
  self->putchar_renderer = &sp_font_putchar_renderer;
  self->get_name = &sp_font_get_name;
  self->get_height = &sp_font_get_height;
  self->get_ascent = &sp_font_get_ascent;
  self->get_descent = &sp_font_get_descent;
  self->get_line_skip = &sp_font_get_line_skip;
  self->get_m_dash = &sp_font_get_m_dash;
  self->get_height_line_skip_difference = &sp_font_get_height_line_skip_difference;
  self->get_height_line_skip_delta = &sp_font_get_height_line_skip_delta;
  self->nearest_x = &sp_font_nearest_x;
  self->nearest_y = &sp_font_nearest_y;
  self->measure_text = &sp_font_measure_text;
  self->get_is_drop_shadow = &sp_font_get_is_drop_shadow;
  self->set_is_drop_shadow = &sp_font_set_is_drop_shadow;
  self->get_enable_orthographic_ligatures = &sp_font_get_enable_orthographic_ligatures;
  self->set_enable_orthographic_ligatures = &sp_font_set_enable_orthographic_ligatures;
  self->set_drop_x = &sp_font_set_drop_x;
  self->get_drop_x = &sp_font_get_drop_x;
  self->set_drop_y = &sp_font_set_drop_y;
  self->get_drop_y = &sp_font_get_drop_y;
  self->get_point_size = &sp_font_get_point_size;
  self->get_glyph_advance = &sp_font_get_glyph_advance;
  self->write = &sp_font_write;
  self->write_to_renderer = &sp_font_write_to_renderer;

  return self;
}

const sp_font * sp_font_acquire(void) {
  return sp_font_init((sp_font *)(uintptr_t)sp_font_alloc());
}

const sp_font * sp_font_cctor(const sp_font * self, SDL_Renderer * renderer, int point_size, const void * memory, size_t memory_len, SDL_RWops * stream, TTF_Font * ttf_font) {
  assert(self != NULL && renderer != NULL);
  sp_font_data * data = calloc(1, sizeof * data);
  if(!data) { abort(); }

  assert(ttf_font != NULL);

  data->memory = memory;
  data->memory_len = memory_len;
  data->stream = stream;
  data->font = ttf_font;

  data->name = TTF_FontFaceFamilyName(ttf_font);
  data->point_size = point_size;
  data->renderer = renderer;
  data->height = 0;
  data->ascent = 0;
  data->descent = 0;
  data->line_skip = 0;
  data->m_dash = 0;

  data->is_drop_shadow = true;
  data->enable_orthographic_ligatures = true;
  data->drop_x = 1;
  data->drop_y = 1;

  data->glyphs_capacity = sp_glyphs_alloc_unit;
  data->glyphs_count = 0;

  data->glyphs = calloc(data->glyphs_capacity, sizeof * data->glyphs);
  if(data->glyphs == NULL) {
    fprintf(stderr, "Unable to allocate memory for glyphs.");
    abort();
  }
  ((sp_font *)(uintptr_t)self)->data = data;

  data->glyph_text_buf_count = 0;
  data->glyph_text_buf_capacity = 4096;
  data->glyph_text_buf = calloc(data->glyph_text_buf_capacity, sizeof * data->glyph_text_buf);
  data->glyph_text_next = data->glyph_text_buf;

  /* Set font attributes requires self->data, set above */
  sp_font_set_font_attributes(self);

  return self;
}

/* From: http://www.zlib.net/zlib_how.html */
/* This is an ugly hack required to avoid corruption of the input and output
 * data on Windows/MS-DOS systems. Without this, those systems would assume
 * that the input and output files are text, and try to convert the end-of-line
 * characters from one standard to another. That would corrupt binary data, and
 * in particular would render the compressed data unusable. This sets the input
 * and output to binary which suppresses the end-of-line conversions.
 * SET_BINARY_MODE() will be used later on stdin and stdout, at the beginning
 * of main():
*/
#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#include <fcntl.h>
#include <io.h>
#define SP_SET_BINARY_MODE(file) { setmode(fileno((file)), O_BINARY) }
#else
#define SP_SET_BINARY_MODE(file)
#endif /* >> if defined(MSDOS) || ... */

const sp_font * sp_font_ctor(const sp_font * self, SDL_Renderer * renderer, const void * mem, size_t mem_len, int point_size) {
  const int DO_NOT_FREE_SRC = 0;
  assert(mem_len < INT_MAX);

  SDL_RWops * stream = SDL_RWFromConstMem(mem, (int)mem_len);

  SDL_RWseek(stream, 0, RW_SEEK_SET);
  SDL_ClearError();
  TTF_Font * ttf_font = TTF_OpenFontRW(stream, DO_NOT_FREE_SRC, point_size);
  assert(ttf_font);

  const sp_font * font = sp_font_cctor(self, renderer, point_size, mem, mem_len, stream, ttf_font);

  return font;
}

const sp_font * sp_font_dtor(const sp_font * self) {
  if(self) {
    sp_font_data * data = self->data;

    if(data->glyphs) {
      sp_glyph * glyph = data->glyphs;
      while(glyph < data->glyphs + data->glyphs_count) {
        if(glyph->texture != NULL) {
          SDL_DestroyTexture(glyph->texture), glyph->texture = NULL;
        }
        if(glyph->fg_texture != NULL) {
          SDL_DestroyTexture(glyph->fg_texture), glyph->fg_texture = NULL;
        }
        glyph++;
      }
      free(data->glyphs), data->glyphs = NULL;;

      TTF_CloseFont(data->font);
      SDL_RWclose(data->stream);
    }
    free(data), data = NULL;
  }

  return self;
}

void sp_font_free(const sp_font * self) {
  if(self) {
    free((void *)(uintptr_t)self), self = NULL;
  }
}

void sp_font_release(const sp_font * self) {
  self->free(self->dtor(self));
}

const char * sp_font_get_name(const sp_font * self) {
  return self->data->name;
}

int sp_glpyh_multibyte_compare(const sp_char * lhs, const sp_char * rhs) {
  int N = 0;

  wchar_t left[8] = { '\0' };
  size_t left_len =  mbstowcs(left, lhs, 8);

  wchar_t right[8] = { '\0' };
  size_t right_len = mbstowcs(right, rhs, 8);

  (void)left_len; (void)right_len;

  N = wcsncmp(left, right, 8);

  return N;
}

int sp_glyph_binary_search(const sp_glyph * haystack, int low, int n, const sp_char * needle) {
  int i = low, j = n - 1;

  while(i <= j) {
    int k = i + ((j - i) / 2);

    int N = sp_glpyh_multibyte_compare(haystack[k].c, needle);
    if(N == 0) {
      return k;
    } else if(N < 0) {
      i = k + 1;
    } else {
      j = k - 1;
    }
  }

  // target doesn't exist in the array
  return -1;
}

static const sp_glyph * sp_font_search_glyph_index(const sp_font * self, const sp_char * c) {
  sp_font_data * data = self->data;

  int n = (int)data->glyphs_count;
  int index = sp_glyph_binary_search(data->glyphs, 0, n, c);

  return index > -1 ? &(data->glyphs[index]) : NULL;
}

static int sp_font_get_glyph_advance(const sp_font * self, const sp_char * text) {
  static const sp_glyph * M = NULL;
  if(!M) { M = sp_font_search_glyph_index(self, "M"); }

  const sp_glyph * glyph = sp_font_search_glyph_index(self, text);
  if(glyph) {
    return glyph->advance;
  } else { return M->advance; }
}

static int sp_font_string_to_bytes(const sp_font * self, const sp_char * text, sp_char bytes[4], int * text_skip) {
  int bytes_skip = 0;
  bool use_x = false;
  wchar_t x[8] = { '\0' };

  if(self->data->enable_orthographic_ligatures) {
    /* Find orthographic ligatures */
    if(strncmp(text, "ffi", 3) == 0) {
      wcsncpy(x, L"ﬃ", 8);
      use_x = true;
      *text_skip = 3;
    } else if(strncmp(text, "ffl", 3) == 0) {
      wcsncpy(x, L"ﬄ", 8);
      use_x = true;
      *text_skip = 3;
    } else if(strncmp(text, "ff", 2) == 0) {
      wcsncpy(x, L"ﬀ", 8);
      use_x = true;
      *text_skip = 2;
    } else if(strncmp(text, "fl", 2)  == 0) {
      wcsncpy(x, L"ﬂ", 8);
      use_x = true;
      *text_skip = 2;
    } else if(strncmp(text, "fi", 2) == 0) {
      wcsncpy(x, L"ﬁ", 8);
      use_x = true;
      *text_skip = 2;
    }
  }

  if(use_x) {
    bytes_skip = (int)wcstombs(bytes, x, 4);
  } else {
    sp_font_get_code_point(text, &bytes_skip);
    *text_skip = bytes_skip;
    memset(bytes, '\0', 4 * sizeof * bytes);
    switch(bytes_skip) {
      case 1:
        bytes[0] = *(text);
        break;
      case 2:
        bytes[0] = *(text);
        bytes[1] = *(text + 1);
        break;
      case 3:
        bytes[0] = *(text);
        bytes[1] = *(text + 1);
        bytes[2] = *(text + 2);
        break;
      case 4:
        bytes[0] = *(text);
        bytes[1] = *(text + 1);
        bytes[2] = *(text + 2);
        bytes[3] = *(text + 3);
        break;
      default:
        fprintf(stderr, "Unexpected skip in %s\n", text);
        abort();
    }
  }

  return bytes_skip;
}

static const sp_glyph * sp_font_add_glyph(const sp_font * self, int skip, sp_char glyph_to_find[4]) {
  sp_font_data * data = self->data;
  if(data->glyphs_count + 1 >= data->glyphs_capacity) {
    data->glyphs_capacity += CHAR_MAX;
    sp_glyph * temp = realloc(data->glyphs, sizeof * data->glyphs * data->glyphs_capacity);
    if(!temp) {
      fprintf(stderr, "Failed to resize glyph index\n");
      abort();
    }
    data->glyphs = temp;
  }

  sp_glyph glyph = { 0 };

  if(data->glyph_text_buf_count + 1 >= data->glyph_text_buf_capacity) {
    fprintf(stderr, "Too many glyphs for glyph buffer!\n");
    abort();
  }

  assert(skip > 0);

  glyph.c = data->glyph_text_next;
  data->glyph_text_next += skip + (int)(sizeof '\0');
  data->glyph_text_buf_count++;

  memmove(glyph.c, glyph_to_find, 4 * sizeof * glyph_to_find);

  fprintf(stdout, "Cannot find '%s' (%i), adding to glyph buffer as '%s'.\n", glyph_to_find, skip, glyph.c);
  sp_font_glyph_create_texture(self, glyph.c, &(glyph.texture), &(glyph.fg_texture));
  assert(glyph.texture && glyph.fg_texture);

  int glyph_w = 0, glyph_h = 0;
  SDL_QueryTexture(glyph.texture, NULL, NULL, &glyph_w, &glyph_h);
  TTF_SizeUTF8(self->data->font, glyph.c, &(glyph.advance), &glyph_h);

  data->glyphs_count++;
  sp_glyph * next_glyph = &(data->glyphs[data->glyphs_count - 1]);

  memmove(next_glyph, &glyph, sizeof * next_glyph);

  qsort(data->glyphs, data->glyphs_count, sizeof * data->glyphs, &sp_glyph_compare);
  return sp_font_search_glyph_index(self, next_glyph->c);
}

int sp_font_putchar(const sp_font * self, const SDL_Point * destination, const SDL_Color * color, sp_font_line_adornment adornment, const sp_char * text, int * advance) {
  return sp_font_putchar_renderer(self, self->data->renderer, destination, color, adornment, text, advance);
}

int sp_font_putchar_renderer(const sp_font * self, SDL_Renderer * renderer, const SDL_Point * destination, const SDL_Color * color, sp_font_line_adornment adornment, const sp_char * text, int * advance) {
  int bytes_skip = 0, text_skip = 0;

  if(advance) { *advance = 1; }

  if(!text) { return 0; }
  if(*text == '\0') { return 1; }

  sp_char glyph_to_find[4] = { '\0' };
  bytes_skip = sp_font_string_to_bytes(self, text, glyph_to_find, &text_skip);
  assert(text_skip > 0);

  int code_point_skip = 0;
  sp_font_get_code_point(text, &code_point_skip);
  if(code_point_skip < bytes_skip) { code_point_skip = bytes_skip; }

  const sp_glyph * g = sp_font_search_glyph_index(self, glyph_to_find);
  /* TODO: Figure out why this is needed: */
  if(glyph_to_find[0] == 0 && glyph_to_find[1] == 0 && glyph_to_find[2] == 0 && glyph_to_find[3] == 0) { return 1; }

  SDL_Point point = { .x = destination->x, .y = destination->y };

  if(!g) {
    g = sp_font_add_glyph(self, text_skip, glyph_to_find);
    if(!g->texture) {
      fprintf(stderr, "Cannot find glyph for (%i) '%s'\n", (int)(*glyph_to_find), glyph_to_find);
      if(advance) { *advance = 0; }
      return 1;
    }
    // assert(g && g->texture);
  }

  if(g) {
    if(g->advance != 0) {
      /* fix the advance if the advance exists */
      if(advance) { *advance = g->advance; }
    }
    SDL_Texture * texture = g->texture;
    if(!g->texture) {
      fprintf(stderr, "Cannot find glyph for (%i) '%s'\n", (int)(*glyph_to_find), glyph_to_find);
      if(advance) { *advance = 0; }
      return 1;
    }
    assert(g->texture);
    if(texture) {
      int checked_advance = g->advance;
      if(advance) { checked_advance = *advance; }

      SDL_Rect dest = { .x = point.x, .y = point.y, .w = checked_advance, .h = sp_font_get_height(self) };
      if(advance) {
        *advance = checked_advance;
      }
      /* Render font in a specific color: */
      SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
      SDL_SetTextureAlphaMod(texture, color->a);
      SDL_SetTextureColorMod(texture, color->r, color->g, color->b);

      if(adornment == SFLA_UNDERLINE) {
        const sp_glyph * underline_glyph = sp_font_search_glyph_index(self, "_");
        if(!underline_glyph) {
          sp_char underlines[4] = { '_', '\0', '\0', '\0' };
          underline_glyph = sp_font_add_glyph(self, 1, underlines);
        }
        SDL_Texture * underline_texture = underline_glyph->fg_texture;
        SDL_SetTextureBlendMode(underline_texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureAlphaMod(underline_texture, color->a);
        SDL_SetTextureColorMod(underline_texture, color->r, color->g, color->b);
        SDL_RenderCopy(renderer, underline_texture, NULL, &dest);
      }

      if(SDL_RenderCopy(renderer, texture, NULL, &dest) != 0) {
        fprintf(stderr, "Unable to render glyph during write: %s\n", SDL_GetError());
        abort();
      }
    }
  }

  if(text_skip == 0) {
    fprintf(stdout, "Skip is 0 for some reason?\n");
    abort();
  }

  return text_skip;
}

static void sp_font_write(const sp_font * self, const SDL_Point * destination, const SDL_Color * color, const char * text, size_t text_len, int * w, int * h) {
  sp_font_write_to_renderer(self, self->data->renderer, destination, color, text, text_len, w, h);
}

static void sp_font_write_to_renderer(const sp_font * self, SDL_Renderer * renderer, const SDL_Point * destination, const SDL_Color * color, const char * text, size_t text_len, int * w, int * h) {
  (void)h;

  SDL_Point dest = {
    .x = destination->x,
    .y = destination->y
  };
  if(w) { *w = 0; }
  if(h) { *h = 0; }
  const char * eos = text + text_len;
  const char * s = text;
  while(s < eos && s && *s != '\0') {
    int skip = 0;
    if(*s == '\n') {
      dest.y += sp_font_get_line_skip(self);
      dest.x = destination->x;
      skip = 1;
    } else {
      int advance;
      skip = sp_font_putchar_renderer(self, renderer, &dest, color, SFLA_NONE, s, &advance);
      dest.x += advance;
    }

    s += skip;
    if(s >= eos) { break; }
  }

  if(h) { *h = dest.y; }
  if(w) { *w = dest.x; }
}

int sp_font_get_height(const sp_font * self) { return self->data->height; }
int sp_font_get_ascent(const sp_font * self) { return self->data->ascent; }
int sp_font_get_descent(const sp_font * self) { return self->data->descent; }
int sp_font_get_line_skip(const sp_font * self) { return self->data->line_skip; }
int sp_font_get_m_dash(const sp_font * self) { return self->data->m_dash; }
int sp_font_get_height_line_skip_difference(const sp_font * self) { return self->data->height - self->data->line_skip; }
int sp_font_get_height_line_skip_delta(const sp_font * self) { return self->data->height - sp_font_get_height_line_skip_difference(self); }

int sp_font_nearest_x(const sp_font * self, int x) {
  static const sp_glyph * m_dash = NULL;
  if(m_dash == NULL) {
    m_dash = sp_font_search_glyph_index(self, "M");
  }
  assert(m_dash != NULL && x > 0);
  int advance = m_dash->advance;
  assert(advance > 0);
  float fx = (float)x / (float)advance;
  int rx = fx >= 0.0 ? (int)(fx + 0.5) : (int)(fx - 0.5);
  return rx * advance;
}

int sp_font_nearest_y(const sp_font * self, int y) {
  float fy = (float)y /(float)sp_font_get_height_line_skip_delta(self);
  int iy = fy >= 0.0 ? (int)(fy + 0.5) : (int)(fy - 0.5);
  return iy * sp_font_get_height_line_skip_delta(self);
}

SDL_Surface * sp_font_render_ligature(const char * text, TTF_Font * ttf_font, SDL_Color color) {
  static const uint16_t ff[4] = { 0xfffe,  0x00fb };
  static const uint16_t fi[4] = { 0xfffe,  0x01fb};
  static const uint16_t fl[4] = { 0xfffe,  0x02fb};
  static const uint16_t ffi[4] = { 0xfffe,  0x03fb };
  static const uint16_t ffl[4] = { 0xfffe,  0x04fb};

  wchar_t left[8] = { '\0' };
  mbstowcs(left, text, 8);

  if(*text == ' ') {
    return TTF_RenderGlyph_Blended(ttf_font, (uint16_t)' ', color);
  } else if(wcsncmp(left, L"ﬀ", 4) == 0) {
    return TTF_RenderUNICODE_Blended(ttf_font, ff, color);
  } else if(wcsncmp(left, L"ﬃ", 4) == 0) {
    return TTF_RenderUNICODE_Blended(ttf_font, ffi, color);
  } else if(wcsncmp(left, L"ﬄ", 4) == 0) {
    return TTF_RenderUNICODE_Blended(ttf_font, ffl, color);
  } else if(wcsncmp(left, L"ﬁ", 4) == 0) {
    return TTF_RenderUNICODE_Blended(ttf_font, fi, color);
  } else if(wcsncmp(left, L"ﬂ", 4) == 0) {
    return TTF_RenderUNICODE_Blended(ttf_font, fl, color);
  }

  return TTF_RenderUTF8_Blended(ttf_font, text, color);
}

errno_t sp_font_glyph_create_texture(const sp_font * self, const char * text, SDL_Texture ** out_texture, SDL_Texture ** out_fg_texture) {
  assert(out_texture && self->data->font);

  SDL_Texture * original_target = SDL_GetRenderTarget(self->data->renderer);

  static const SDL_Color white = { .r = 255, .g = 255, .b = 255, .a = 255 };

  TTF_Font * ttf_font = self->data->font;
  SDL_Renderer * renderer = self->data->renderer;

  SDL_ClearError();
  SDL_Surface * fg_surface = sp_font_render_ligature(text, ttf_font, white);
  if(!fg_surface || sp_is_sdl_error(SDL_GetError())) { goto err1; }

  assert(fg_surface != NULL);

  SDL_ClearError();
  SDL_Texture * fg_texture = SDL_CreateTextureFromSurface(renderer, fg_surface);
  if(!fg_texture || sp_is_sdl_error(SDL_GetError())) { goto err2; }

  SDL_ClearError();
  SDL_Texture * texture = SDL_CreateTexture(renderer
      , SDL_PIXELFORMAT_RGBA8888
      , SDL_TEXTUREACCESS_TARGET
      , fg_surface->w
      , fg_surface->h
      );
  if(!texture || sp_is_sdl_error(SDL_GetError())) { goto err4; }

  assert(fg_texture != NULL && texture != NULL);

  const SDL_Color transparent_black = { 0, 0, 0, 0 };
  const sp_gui_rgba_context * rgba = sp_gui_push_draw_color(renderer, &transparent_black);
  {
    /* make texture a temporary render target */
    SDL_SetRenderTarget(renderer, texture);

    /* clear the texture render target */
    SDL_RenderFillRect(renderer, NULL); /* screen color */
    SDL_RenderClear(renderer);

    /* render the text */
    SDL_Rect fg_rect = {.x = 1, .y = 1, .w = fg_surface->w, .h = fg_surface->h };
    SDL_RenderCopy(renderer, fg_texture, NULL, &fg_rect);

    /* reset the render target */
    SDL_SetRenderTarget(renderer, NULL);

    sp_gui_pop_draw_color(rgba);
  }

  SDL_FreeSurface(fg_surface), fg_surface = NULL;

  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
  SDL_SetTextureAlphaMod(texture, 255);
  SDL_SetTextureColorMod(texture, 255, 255, 255);

  SDL_SetRenderTarget(renderer, original_target);
  *out_texture = texture;
  *out_fg_texture = fg_texture;
  return SP_SUCCESS;

err4:
  SDL_DestroyTexture(fg_texture), fg_texture = NULL;

err2:
  SDL_FreeSurface(fg_surface), fg_surface = NULL;

err1:
  *out_texture = NULL;
  return SP_FAILURE;
}

int sp_glyph_compare(const void * a, const void * b) {
  const sp_glyph * l = (const sp_glyph *)a;
  const sp_glyph * r = (const sp_glyph *)b;

  if(l->c == NULL) { return -1; }
  if(r->c == NULL) { return 1; }

  return strncmp(l->c, r->c, 4);
}

void sp_font_set_font_attributes(const sp_font * self) {
  assert(self != NULL && self->data != NULL && self->data->font != NULL);
  sp_font_data * data = self->data;

  TTF_Font * ttf_font = data->font;

  data->ascent = TTF_FontAscent(ttf_font);
  data->descent = TTF_FontDescent(ttf_font);
  data->height = TTF_FontHeight(ttf_font);
  data->line_skip = TTF_FontLineSkip(ttf_font);

  /* create glyphs from table */
  for(size_t i = 0; i < data->glyphs_capacity; ++i) {
    sp_glyph * glyph = data->glyphs + i;
    if(data->glyph_text_buf_count + 1 >= data->glyph_text_buf_capacity) {
      fprintf(stderr, "Not enough glpyh text buffers\n");
      abort();
    }
    glyph->c = data->glyph_text_next;
    memset(glyph->c, '\0', sizeof(sp_char));

    glyph->c[0] = (sp_char)i;
    glyph->c[1] = '\0';

    data->glyph_text_next += sizeof * glyph->c + sizeof '\0';

    data->glyph_text_buf_count++;

    sp_font_glyph_create_texture(self, glyph->c, &glyph->texture, &glyph->fg_texture);

    int glyph_w = 0, glyph_h = 0;
    SDL_QueryTexture(glyph->texture, NULL, NULL, &glyph_w, &glyph_h);
    TTF_SizeUTF8(data->font, glyph->c, &(glyph->advance), &glyph_h);

    assert(glyph_w >= 0);
    data->glyphs_count++;
  }

  qsort(data->glyphs, data->glyphs_count, sizeof * data->glyphs, &sp_glyph_compare);

  // get an M for an em dash. Which is cheezy
  const sp_glyph * m = sp_font_search_glyph_index(self, "M");
  if(!m) {
    fprintf(stderr, "Can't find 'M'\n");
    abort();
  }

  data->m_dash = m->advance;
}

int sp_font_get_point_size(const sp_font * self) {
  assert(self != NULL);
  return self->data->point_size;
}

void sp_font_measure_text(const sp_font * self, const sp_char * text, size_t text_len, int * w, int * h) {
  if(w) { *w = 0; }
  if(h) { *h = 0; }

  if(text_len == 0 || !text) {
    return;
  }

  const char * eos = text + text_len;
  const char * s = text;
  const char * max_line = text;
  int y = 0; long max_line_len = 0;
  while(s < eos && s && *s != '\0') {
    if(*s == '\n') {
      y++;
      if(s - max_line > max_line_len) {
        max_line_len = s - max_line;
        max_line = s + 1;
      }
    }
    s++;
  }

  if(max_line_len == 0 && text_len > 0) {
    TTF_SizeUTF8(self->data->font, text, w, h);
    return;
  }

  char * max_line_copy = NULL;
  if(max_line_len > 0 && (size_t)max_line_len != text_len) {
    assert(max_line_len > 0);
    assert((size_t)max_line_len <= text_len);
    max_line_copy = calloc((size_t)max_line_len, sizeof * max_line_copy);
    if(!max_line_copy) { abort(); }
    memmove(max_line_copy, max_line, (size_t)max_line_len);
    max_line_copy[max_line_len - 1] = '\0';
    max_line = max_line_copy;
  } else {
    max_line = text;
  }

  if(self->data->font && y > 0) {
    TTF_SizeUTF8(self->data->font, max_line, w, h);
    *h *= y;
  }

  free(max_line_copy), max_line_copy = NULL;
}

bool sp_font_get_is_drop_shadow(const sp_font * self) {
  return self->data->is_drop_shadow;
}

void sp_font_set_is_drop_shadow(const sp_font * self, bool is_drop_shadow) {
  self->data->is_drop_shadow = is_drop_shadow;
}

static bool sp_font_get_enable_orthographic_ligatures(const sp_font * self) {
  return self->data->enable_orthographic_ligatures;
}

static void sp_font_set_enable_orthographic_ligatures(const sp_font * self, bool enable_orthographic_ligatures) {
  self->data->enable_orthographic_ligatures = enable_orthographic_ligatures;
}

void sp_font_set_drop_x(const sp_font * self, int drop_x) {
  sp_font_data * data = self->data;
  data->drop_x = drop_x;
}

int sp_font_get_drop_x(const sp_font * self) {
  sp_font_data * data = self->data;
  return data->drop_x;
}

void sp_font_set_drop_y(const sp_font * self, int drop_y) {
  sp_font_data * data = self->data;
  data->drop_y = drop_y;
}

int sp_font_get_drop_y(const sp_font * self) {
  sp_font_data * data = self->data;
  return data->drop_y;
}

static void sp_font_line_chunk_alloc(sp_font_line_chunk ** chunks, size_t * chunks_capacity, size_t * chunks_count, const sp_char * chunk_start, const sp_char * chunk_end, sp_font_line_adornment adornment) {
  assert(chunk_start && chunk_end);

  if(chunk_end) {
    if(*chunks_count + 1 > *chunks_capacity) {
      /* realloc chunks cuz we're under-chunked; we need more flippin'
       * chunks, ok?! */
      *chunks_capacity *= 2;
      sp_font_line_chunk * new_chunks = realloc(*chunks,
          *chunks_capacity * sizeof * new_chunks
          );
      if((*chunks_capacity * sizeof * new_chunks > 0 && !new_chunks) || errno == ENOMEM) {
        abort();
      }
      *chunks = new_chunks;
    }

    /* chunk copy */
    sp_font_line_chunk * next_chunk = &((*chunks)[(*chunks_count)]);
    memset(next_chunk, 0, sizeof * next_chunk);

    /* sp_strcpy performs an alloc */
    next_chunk->text = sp_strcpy(chunk_start, chunk_end, &(next_chunk->text_len));
    next_chunk->adornment = adornment;

    *chunks_count += 1;
  }
}

void sp_font_parse_text(const sp_char * text, size_t text_len, const sp_font_line_chunk ** chunks, size_t * chunks_len) {
  assert(chunks && chunks_len);

  *chunks = NULL;
  *chunks_len = 0;

  if(!text) { return; }
  if(text_len <= 0) { return; }

  sp_font_line_adornment adornments[128] = { 0 };
  sp_font_line_adornment * top = adornments;
  const sp_font_line_adornment * adornments_end = adornments + 128;
  const sp_font_line_adornment * adornments_start = adornments;

  *top = SFLA_PLAINTEXT;

  const sp_char * s = text;
  const sp_char * end = text + text_len;

  size_t temp_chunks_capacity = 16;
  size_t temp_chunks_count = 0;
  sp_font_line_chunk * temp_chunks = calloc(temp_chunks_capacity, sizeof * temp_chunks);

  const sp_char * current_chunk_start = s;
  const sp_char * current_chunk_end = NULL;
  while(*s != '\0') {
    int skip = 0;
    sp_font_get_code_point(s, &skip);

    /* controls codes are ${CODE_START TEXT CODE_END} */
    if(s < end && (s + 1) < end && (s + 2) < end) {
      const sp_char * L = s;
      const sp_char * LL = s + 1;
      const sp_char * LLL = s + 2;

      /* Control code */
      if(*L != '\0' && *LL != '\0' && *LLL != '\0') {
        if(*L == '$' && *LL == '{') {
          /* new chunk begins; save the previous chunk */
          if(current_chunk_start && !current_chunk_end && current_chunk_start != text) {
            assert(top >= adornments_start && top < adornments_end);
            sp_font_line_chunk_alloc(&temp_chunks, &temp_chunks_capacity, &temp_chunks_count, current_chunk_start, s, *top);
          }

          /* Inside a control code */
          switch(*LLL) {
            case '_': {
                ++top;
                assert(top >= adornments_start && top < adornments_end);
                *top = SFLA_UNDERLINE;
              }
              break;
            default:
              fprintf(stderr, "Unexpected opening control character '%c' encountered in control code block.\n", *LLL);
              exit(EXIT_FAILURE);
              break;
          }

          current_chunk_start = LLL + 1;
          current_chunk_end = NULL;
        } else if(*LL == '}' && *LLL == '$') {
          /* Control code ends */
          switch(*L) {
            case '_':
              current_chunk_end = L;
              break;
            default:
              fprintf(stderr, "Unexpected closing control character '%c' encountered in control code block.\n", *L);
              exit(EXIT_FAILURE);
              break;
          }
        }
      }
    }

    s += skip;

    if(current_chunk_start && current_chunk_end) {
      assert(top >= adornments_start && top < adornments_end);
      sp_font_line_chunk_alloc(&temp_chunks, &temp_chunks_capacity, &temp_chunks_count, current_chunk_start, current_chunk_end, *top);
      current_chunk_start = s + skip + 1; /* shrug */
      current_chunk_end = NULL;
      --top;
      assert(top >= adornments_start && top < adornments_end);
    }
  }

  if(!current_chunk_end) { current_chunk_end = s; }
  if(top > adornments) { top--; }
  assert(top == adornments_start);
  sp_font_line_chunk_alloc(&temp_chunks, &temp_chunks_capacity, &temp_chunks_count, current_chunk_start, current_chunk_end, *top);

  if(temp_chunks_count > 0) {
    sp_font_line_chunk * last_chunk = &(temp_chunks[temp_chunks_count - 1]);
    /* check control code balance; if we're unbalanced and the last chunk is not plaintext, we fail */
    if(last_chunk->adornment == SFLA_PLAINTEXT) {
      /* we're good to create a new, final chunk */
      if(!current_chunk_end) {
        current_chunk_end = end;
        sp_font_line_chunk_alloc(&temp_chunks, &temp_chunks_capacity, &temp_chunks_count, current_chunk_start, current_chunk_end, *top);
      }
    } else {
      /* non-closing chunk */
      fprintf(stderr, "Chunk %s not closed.\n", sp_font_line_adornment_to_string(last_chunk->adornment));
      exit(EXIT_FAILURE);
    }
  } else {
    /* encountered EOF without parsing any chunks */
    fprintf(stderr, "No chunks parsed.\n");
    exit(EXIT_FAILURE);
  }

  parse_chunk_lines(text, text_len, temp_chunks, temp_chunks_count);

  *chunks = temp_chunks;
  *chunks_len = temp_chunks_count;
}

static void parse_chunk_lines(const sp_char * src_text, size_t src_text_len, sp_font_line_chunk * chunks, size_t chunks_len) {
  sp_char * string_buffer = calloc(src_text_len * 2, sizeof * string_buffer);
  const sp_char * const end_string_buffer = string_buffer + (src_text_len * 2);

  sp_char * next_string = string_buffer;

  size_t lines_count = sp_font_count_new_lines(src_text, src_text_len);
  fprintf(stdout, "Lines counted: %lu\n", lines_count);

  sp_font_line * lines_buf = calloc(lines_count * 2, sizeof * lines_buf);
  const sp_font_line * const end_line = lines_buf + (lines_count * 2);
  sp_font_line * next_line = lines_buf;

  sp_font_line_chunk * chunk = chunks;
  do {
    sp_font_line * first_line = next_line;
    const sp_char * end = chunk->text + chunk->text_len;

    /* to be freed */
    chunk->source_buffer = string_buffer;
    chunk->source_lines = lines_buf;

    const sp_char * start = chunk->text;
    const sp_char * text = start;

    sp_font_line * prev = NULL;

    chunk->newline_count = sp_font_count_new_lines(chunk->text, chunk->text_len);

    size_t line_len = 0;
    if(chunk->newline_count > 0) {
      while(text < end) {
        int skip = 0;
        sp_font_get_code_point(text, &skip);

        if(*text == '\n') {
          ptrdiff_t diff = (text - start);
          if(diff <= 0) {
            assert(next_string + 2 < end_string_buffer);

            *next_string = '\n';
            *(next_string + 1) = '\0';
            line_len = 1;
          } else {
            assert(diff > 0 && (size_t)diff <= SIZE_MAX);
            line_len = (size_t)diff;

            assert(next_string + line_len + 1 < end_string_buffer);

            memmove(next_string, start, line_len);
            next_string[line_len] = '\0';
          }

          assert(next_line < end_line);
          assert(next_string + line_len + 1 < end_string_buffer);

          memset(next_line, 0, sizeof * next_line);
          next_line->next = NULL;
          next_line->line = next_string;
          next_line->line_len = strnlen((const char *)next_line->line, SP_MAX_STRING_LEN);
          next_line->has_newline = sp_font_count_new_lines(chunk->text, chunk->text_len) >= 1;
          next_line->adornment = chunk->adornment;

          next_string += (line_len + 1);

          if(prev) {
            prev->next = next_line;
          }

          prev = next_line;
          next_line++;
          chunk->lines_count++;
          start = text;
        }
        text += skip;
      }

      /* copy remainder */
      ptrdiff_t diff = (text - start);
      assert(diff >= 0 && (size_t)diff <= SIZE_MAX);
      line_len = (size_t)diff;
      assert(next_string + line_len + 1 < end_string_buffer);

      memmove(next_string, start, line_len);
      next_string[line_len] = '\0';

      memset(next_line, 0, sizeof * next_line);
      next_line->next = NULL;
      next_line->line = next_string;
      next_line->line_len = strnlen((const char *)next_line->line, SP_MAX_STRING_LEN);
      next_line->has_newline = sp_font_count_new_lines(chunk->text, chunk->text_len) >= 1;
      next_line->adornment = chunk->adornment;

      next_string += (line_len + 1);
      if(prev) {
        prev->next = next_line;
      }

      prev = next_line;
      next_line++;
    } else {
      size_t len = strnlen((const char *)text, SP_MAX_STRING_LEN);
      assert((next_string + len + 1) < end_string_buffer);

      memmove(next_string, text, len);
      next_string[len] = '\0';

      memset(next_line, 0, sizeof * next_line);
      next_line->next = NULL;
      next_line->line = next_string;
      next_line->line_len = strnlen((const char *)next_line->line, SP_MAX_STRING_LEN);
      next_line->has_newline = sp_font_count_new_lines(chunk->text, chunk->text_len) >= 1;
      next_line->adornment = chunk->adornment;

      next_string += len + 1;
      if(prev) {
        prev->next = next_line;
      }

      first_line = next_line;
      prev = next_line;
      next_line++;
      chunk->lines_count++;
      chunk->newline_count = 0;
    }

    chunk->lines = first_line;

  } while(++chunk < chunks + chunks_len);

  /* CHUNK DIAGNOSTICS:
*  for(int i = 0; i < 20; i++) {
*    sp_font_line_chunk * chank = &(chunks[i]);
*    fprintf(stdout, "Chunk lines: %lu\n", chank->newline_count);
*    sp_font_line * line = chank->lines;
*    const sp_font_line * end_of_lines = chank->lines + chank->lines_count;
*    while(line < end_of_lines) {
*      fprintf(stdout, "-> (%lu) >%s<\n", line->line_len, line->line);
*      line++;
*    }
*  }
  */
}

size_t sp_font_count_new_lines(const sp_char * text, size_t text_len) {
  assert(text);

  if(text_len <= 0) { return 0; }

  const sp_char * iter = text;
  const sp_char * const end = text + text_len;
  size_t lines_count = 0;

  do {
    if(*iter == '\n') {
      lines_count++;
    }
    iter++;
  } while(iter < end);

  return lines_count;
}

/* WARNING: Everything below is garbage */
/* From https://stackoverflow.com/questions/395832/how-to-get-code-point-number-for-a-given-character-in-a-utf-8-string
    U+0000 — U+007F:    1 byte:  0xxxxxxx
    U+0080 — U+07FF:    2 bytes: 110xxxxx 10xxxxxx
    U+0800 — U+FFFF:    3 bytes: 1110xxxx 10xxxxxx 10xxxxxx
    U+10000 — U+10FFFF: 4 bytes: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
*/
/* From https://en.wikipedia.org/wiki/UTF-8
| # of bytes | Bits for code point | 1st code point | Last code point | Byte 1   | Byte 2   | Byte 3   | Byte 4   |
| ---------- | ------------------- | -------------- | --------------- | ------   | -------- | -------- | -------- |
| 1 	       | 7                   | U+0000         | U+007F          | 0xxxxxxx |          |          |          |
| 2 	       | 11                  | U+0080         | U+07FF          | 110xxxxx | 10xxxxxx |          |          |
| 3 	       | 16                  | U+0800         | U+FFFF          | 1110xxxx | 10xxxxxx | 10xxxxxx |          |
| 4 	       | 21                  | U+10000        | U+10FFFF        | 11110xxx | 10xxxxxx | 10xxxxxx | 10xxxxxx |
  */
/* Reference Code 1: */
/*
wchar_t utf8_char_to_ucs2(const char *utf8) {
  if(!(utf8[0] & 0x80))      // 0xxxxxxx
    return (wchar_t)utf8[0];
  else if((utf8[0] & 0xE0) == 0xC0)  // 110xxxxx
    return (wchar_t)(((utf8[0] & 0x1F) << 6) | (utf8[1] & 0x3F));
  else if((utf8[0] & 0xF0) == 0xE0)  // 1110xxxx
    return (wchar_t)(((utf8[0] & 0x0F) << 12) | ((utf8[1] & 0x3F) << 6) | (utf8[2] & 0x3F));
  else
    return ERROR;  // uh-oh, UCS-2 can't handle code points this high
}
*/

/* Borrowed from Rosetta Code */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"
typedef struct {
	char mask;        /* char data will be bitwise AND with this */
	char lead;        /* start bytes of current char in utf-8 encoded character */
	uint32_t beg;     /* beginning of codepoint range */
	uint32_t end;     /* end of codepoint range */
	int bits_stored;  /* the number of bits from the codepoint that fits in char */
}utf_t;
#pragma GCC diagnostic pop

utf_t * utf[] = {
	[0] = &(utf_t){ .mask = 0x3f, .lead = (char)0x80, .beg = 0,       .end = 0,        .bits_stored = 6 },
	[1] = &(utf_t){ .mask = 0x7f, .lead = (char)0x00, .beg = 0000,    .end = 0177,     .bits_stored = 7 },
	[2] = &(utf_t){ .mask = 0x1f, .lead = (char)0xc0, .beg = 0200,    .end = 03777,    .bits_stored = 5 },
	[3] = &(utf_t){ .mask = 0xf,  .lead = (char)0xe0, .beg = 04000,   .end = 0177777,  .bits_stored = 4 },
	[4] = &(utf_t){ .mask = 0x7,  .lead = (char)0xf0, .beg = 0200000, .end = 04177777, .bits_stored = 3 },
	      &(utf_t){ 0 },
};

int sp_font_get_utf8_len(const char ch)
{
	int len = 0;
	for(utf_t **u = utf; *u; ++u) {
		if((ch & ~(*u)->mask) == (*u)->lead) {
			break;
		}
		++len;
	}
	if(len > 4) { /* Malformed leading byte */
    fprintf(stderr, "Malformed leading byte '%c': %i\n", ch, len);
    return -1;
	}
	return len;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wint-conversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
uint32_t sp_font_get_utf8_codepoint(const char chr[4], int * skip, bool * is_error) {
	int bytes = sp_font_get_utf8_len(*chr);
  if(bytes == -1) {
    *is_error = true;
    return 0;
  }
	int shift = utf[0]->bits_stored * (bytes - 1);
	uint32_t cp = (*chr++ & utf[bytes]->mask) << shift;

	for(int i = 1; i < bytes; ++i, ++chr) {
		shift -= utf[0]->bits_stored;
		cp |= ((char)*chr & utf[0]->mask) << shift;
	}

  *is_error = false;
  *skip = bytes;

  return cp;
}
#pragma GCC diagnostic pop

uint32_t sp_font_get_code_point(const sp_char * s, int * skip) {
  bool is_error;

  char c[4] = { 0 };
  c[0] = *s != '\0' ? *s : '\0';
  c[1] = *(s + 1) != '\0' ? *(s + 1) : '\0';
  c[2] = *(s + 2) != '\0' ? *(s + 2) : '\0';
  c[3] = *(s + 3) != '\0' ? *(s + 3) : '\0';

  uint32_t cp = sp_font_get_utf8_codepoint(c, skip, &is_error);
  if(is_error) { fprintf(stderr, "Error getting codepoint for '%s'\n", s); }

  return cp;
}

#if 1 == 2

int sp_font_write_context_get_balance(const sp_font_write_context * self) {
  assert(self);
  if(self->data->stack->is_empty(self->data->stack)) {
    return -1;
  } else {
    const sp_font_write_stack_item * item = self->data->stack->peek(self->data->stack);
    return item->balance;
  }
}

int sp_font_write_context_get_offset(const sp_font_write_context * self) {
  assert(self);
  if(self->data->stack->is_empty(self->data->stack)) {
    return -1;
  } else {
    const sp_font_write_stack_item * item = self->data->stack->peek(self->data->stack);
    return item->offset;
  }
}

const sp_font_write_context * sp_font_copy_write_context(const sp_font_write_context * context) {
  const sp_font_write_context * temp = sp_font_open_write_context();

  sp_font_write_context_data * data = temp->data;

  data->stack->data->capacity = context->data->stack->data->capacity;
  data->stack->data->count = context->data->stack->data->count;
  if(context->data->stack->data->items) {
    /* not initialized in stack constructor: */
    data->stack->data->items = calloc(data->stack->data->capacity, sizeof * data->stack->data->items);
    for(size_t i = 0; i < data->stack->data->count; i++) {
      data->stack->data->items[i] = context->data->stack->data->items[i];
    }
  }

  return temp;
}

const sp_font_write_context * sp_font_open_write_context() {
  sp_font_write_context * temp = calloc(1, sizeof * temp);
  if(!temp) { abort(); }

  sp_font_write_context_data * data = calloc(1, sizeof * data);
  if(!data) { abort(); }

  data->stack = sp_font_write_stack_init();

  temp->get_balance = &sp_font_write_context_get_balance;
  temp->get_offset = &sp_font_write_context_get_offset;
  temp->copy = &sp_font_copy_write_context;
  temp->parse_text = &sp_font_parse_text;

  temp->data = data;

  return temp;
}

void sp_font_close_write_context(const sp_font_write_context * context) {
  sp_font_write_stack_close(context->data->stack), context->data->stack = NULL;
  free(context->data);
  free((void *)(uintptr_t)context), context = NULL;
}

#endif

