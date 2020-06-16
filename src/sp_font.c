#include <stdlib.h>
#include <assert.h>
#include <memory.h>
#include <stdbool.h>
#include <stdint.h>

#include "sp_error.h"
#include "sp_font.h"

const int spooky_default_font_size = 8;

typedef struct spooky_glyph {
  SDL_Texture * texture;
  int min_x;
  int max_x;
  int min_y;
  int max_y;
  int glyph_width;
  int glyph_height;
  int advance;
  int offset;
} spooky_glyph;

typedef struct spooky_glyph_index {
  uint32_t c;
  char padding[4]; /* not portable */
  spooky_glyph glyph;
} spooky_glyph_index;

typedef struct spooky_font_data {
  SDL_Renderer * renderer;
  TTF_Font * font;
  int height;
  int ascent;
  int descent;
  int line_skip;
  int m_dash;
  int drop_x;
  int drop_y;
  bool is_drop_shadow;
  bool reserved0;
  bool reserved1;
  bool reserved2;
  /* glyph array */
  spooky_glyph_index * glyphs;
  size_t glyph_index_capacity;
  size_t glyph_index_count;
} spooky_font_data;

static errno_t spooky_font_open_font(const char * file_path, int point_size, TTF_Font ** ttf_font);
static void spooky_font_write(const spooky_font * self, const spooky_point * destination, const SDL_Color * color, const char * s, int * w, int * h);
static void spooky_font_write_to_renderer(const spooky_font * self, SDL_Renderer * renderer, const spooky_point * destination, const SDL_Color * color, const char * s, int * w, int * h);

static SDL_Texture * spooky_font_glyph_create_texture(SDL_Renderer * renderer, TTF_Font * font, const char * character);
static spooky_glyph * spooky_font_glyph_create(TTF_Font * font, uint32_t character, spooky_glyph * glyph);

static int spooky_glyph_compare(const void * a, const void * b);

static int spooky_font_get_height(const spooky_font * self);
static int spooky_font_get_ascent(const spooky_font * self);
static int spooky_font_get_descent(const spooky_font * self);
static int spooky_font_get_line_skip(const spooky_font * self);
static int spooky_font_get_m_dash(const spooky_font * self);
static int spooky_font_get_height_line_skip_difference(const spooky_font * self);
static int spooky_font_get_height_line_skip_delta(const spooky_font * self);
static int spooky_font_nearest_x(const spooky_font * self, int x);
static int spooky_font_nearest_y(const spooky_font * self, int y);
static void spooky_font_measure_text(const spooky_font * self, const char * text, int * width, int * height);

static void spooky_font_set_font_attributes(const spooky_font * self);

static bool spooky_font_get_is_drop_shadow(const spooky_font * self);
static void spooky_font_set_is_drop_shadow(const spooky_font * self, bool is_drop_shadow);
static void spooky_font_set_drop_x(const spooky_font * self, int drop_x);
static int spooky_font_get_drop_x(const spooky_font * self);
static void spooky_font_set_drop_y(const spooky_font * self, int drop_y);
static int spooky_font_get_drop_y(const spooky_font * self);

errno_t spooky_font_open_font(const char * file_path, int point_size, TTF_Font ** ttf_font) {
  assert(!(file_path == NULL || point_size <= 0));
  
  if(file_path == NULL || point_size <= 0) { goto err0; }
 
  //SDL_ClearError();
  TTF_Font * font = TTF_OpenFont(file_path, point_size);
  if (!font || spooky_is_sdl_error(TTF_GetError())) { goto err0; }

  *ttf_font = font;

  return SP_SUCCESS;

err0:
  fprintf(stderr, "Unable to load font '%s'. %s\n", file_path, TTF_GetError());
  abort();
}

const spooky_font * spooky_font_alloc() {
  spooky_font * self = malloc(sizeof * self);
  if(!self) { abort(); }

  return self;
}

const spooky_font * spooky_font_init(spooky_font * self) {
  if(!self) { abort(); }

  self->ctor = &spooky_font_ctor;
  self->dtor = &spooky_font_dtor;
  self->free = &spooky_font_free;
  self->release = &spooky_font_release;
  self->write = &spooky_font_write;
  self->write_to_renderer = &spooky_font_write_to_renderer;
  self->get_height = &spooky_font_get_height;
  self->get_ascent = &spooky_font_get_ascent;
  self->get_descent = &spooky_font_get_descent;
  self->get_line_skip = &spooky_font_get_line_skip;
  self->get_m_dash = &spooky_font_get_m_dash;
  self->get_height_line_skip_difference = &spooky_font_get_height_line_skip_difference;
  self->get_height_line_skip_delta = &spooky_font_get_height_line_skip_delta;
  self->nearest_x = &spooky_font_nearest_x;
  self->nearest_y = &spooky_font_nearest_y;
  self->measure_text = &spooky_font_measure_text;
  self->get_is_drop_shadow = &spooky_font_get_is_drop_shadow;
  self->set_is_drop_shadow = &spooky_font_set_is_drop_shadow;
  self->set_drop_x = &spooky_font_set_drop_x;
  self->get_drop_x = &spooky_font_get_drop_x;
  self->set_drop_y = &spooky_font_set_drop_y;
  self->get_drop_y = &spooky_font_get_drop_y;

  return self;
}

const spooky_font * spooky_font_acquire() {
  return spooky_font_init((spooky_font *)(uintptr_t)spooky_font_alloc());
}

const spooky_font * spooky_font_ctor(const spooky_font * self, SDL_Renderer * renderer, const char * file_path, int point_size) {
  spooky_font_data * data = calloc(1, sizeof * data);
  if(!data) { abort(); }

  TTF_Font * ttf_font = NULL;
  if(spooky_font_open_font(file_path, point_size, &ttf_font) != SP_SUCCESS) { abort(); }
  data->font = ttf_font;
  data->renderer = renderer;
  data->height = 0;
  data->ascent = 0;
  data->descent = 0;
  data->line_skip = 0;
  data->m_dash = 0;

  data->is_drop_shadow = true;
  data->drop_x = 1;
  data->drop_y = 1;

  data->glyph_index_capacity = 127;
  data->glyph_index_count = 0;

  data->glyphs = calloc(data->glyph_index_capacity, sizeof data->glyphs[0]);

  ((spooky_font *)(uintptr_t)self)->data = data;

  /* Set font attributes requires self->data, set above */
  spooky_font_set_font_attributes(self);

  return self;
}

const spooky_font * spooky_font_dtor(const spooky_font * self) {
  if(self) {
    spooky_font_data * data = self->data;
    TTF_CloseFont(data->font);
    if(data->glyphs) {
      spooky_glyph_index * i = data->glyphs;
      while(i < data->glyphs + data->glyph_index_count) {
        spooky_glyph * g = &(i->glyph);
        if(g->texture != NULL) {
          SDL_DestroyTexture(g->texture), g->texture = NULL;
        }
        i++;
      }
      free(data->glyphs), data->glyphs = NULL;;
    }
    free(data), data = NULL;
  }

  return self;
}

void spooky_font_free(const spooky_font * self) {
  if(self) {
    free((void *)(uintptr_t)self), self = NULL;
  }
}

void spooky_font_release(const spooky_font * self) {
  self->free(self->dtor(self));
}

int spooky_glyph_binary_search(const spooky_glyph_index * arr, int low, int n, uint32_t x) {
  int i = low, j = n - 1;

  while(i <= j)
  {
    int k = i + ((j - i) / 2);

    if(arr[k].c == x) {
      return k;
    } else if(arr[k].c < x) {
      i = k + 1;
    } else {
      j = k - 1;
    }
  }

  // target doesn't exist in the array
  return -1;
}

const spooky_glyph * spooky_font_search_glyph_index(const spooky_font * self, uint32_t c) {
  spooky_font_data * data = self->data;

  int n = (int)data->glyph_index_count; 
  int index = spooky_glyph_binary_search(data->glyphs, 0, n, c);
  return index > -1 ? &(data->glyphs[index].glyph) : NULL;
}

void spooky_font_write(const spooky_font * self, const spooky_point * destination, const SDL_Color * color, const char * s, int * w, int * h) {
  spooky_font_data * data = self->data;

  spooky_font_write_to_renderer(self, data->renderer, destination, color, s, w, h);
}

void spooky_font_write_to_renderer(const spooky_font * self, SDL_Renderer * renderer, const spooky_point * destination, const SDL_Color * color, const char * s, int * w, int * h) {
  spooky_font_data * data = self->data;

  const spooky_glyph * space =  spooky_font_search_glyph_index(self, ' ');
  int space_advance = space->advance;

  int destX = destination->x;
  int destY = destination->y;

  int width = 0;
  int x = 0, y = 0;

  if (w) *w = 0;
  while (*s != '\0') {
    int skip = 0;
    uint32_t c = spooky_font_get_code_point(s, &skip);

    if (c == '\n') {
      if (w && *w == 0) *w = width;
      y += self->get_line_skip(self) + 1;
      x = 0;
    } else if (c == '\t') {
      x += (space_advance * 4);
      width += (space_advance * 4);
    } else if (c == ' ') {
      x += space_advance;
      width += space_advance;
    } else {
      const spooky_glyph * g = spooky_font_search_glyph_index(self, c);
      if(!g) {
        if(data->glyph_index_count >= data->glyph_index_capacity) {
          data->glyph_index_capacity += 127;
          data->glyphs = realloc(data->glyphs, sizeof data->glyphs[0] * data->glyph_index_capacity);
          if(!data->glyphs) {
            fprintf(stderr, "Failed to resize glyph index\n");
            abort();
          }

          fprintf(stdout, "Resized glyph index %i\n", (int)data->glyph_index_capacity);
        }

        TTF_Font * f = data->font;

        spooky_glyph_index * index_item = &(data->glyphs[data->glyph_index_count]);
        spooky_glyph * glyph = &(index_item->glyph);

        memset(glyph, 0, sizeof * glyph);

        index_item->c = c;
        glyph = (spooky_glyph*)(uintptr_t)spooky_font_glyph_create(f, c, glyph);
        glyph->offset = (int)data->glyph_index_count;

        glyph->texture = spooky_font_glyph_create_texture(renderer, f, s);

        data->glyph_index_count++;
        qsort(data->glyphs, data->glyph_index_count, sizeof data->glyphs[0], &spooky_glyph_compare);

        g = spooky_font_search_glyph_index(self, c); 
      }
      int advance = space_advance;
      if(g) {
        SDL_Texture * texture = g->texture;
        advance = g->advance;
        if (texture) {
          SDL_Rect dest = { .x = 0, .y = 0, .w = advance, .h = self->get_height(self) };

          dest.x = x + destX;
          dest.y = y + destY;

          SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
          SDL_SetTextureAlphaMod(texture, color->a);

          /* Render font shadow */
          if(data->is_drop_shadow) {
            SDL_Rect shadow = { .x = dest.x + data->drop_x, .y = dest.y + data->drop_y, .w = dest.w, .h = dest.h };
            SDL_SetTextureColorMod(texture, 20, 20, 20);
            if (SDL_RenderCopy(renderer, texture, NULL, &shadow) != 0) {
              fprintf(stderr, "Unable to render glyph during Write: %s\n", SDL_GetError()); 
              abort();
            }
          }

          /* Render font */
          SDL_SetTextureColorMod(texture, color->r, color->g, color->b);
          if (SDL_RenderCopy(renderer, texture, NULL, &dest) != 0) {
            fprintf(stderr, "Unable to render glyph during Write: %s\n", SDL_GetError()); 
            abort();
          }
        }
      }

      x += advance;
      width += advance;
    }

    s += skip;
  }

  if (h) *h = y;
  if (w && *w == 0) *w = width;
}

int spooky_font_get_height(const spooky_font * self) { return self->data->height; }
int spooky_font_get_ascent(const spooky_font * self) { return self->data->ascent; }
int spooky_font_get_descent(const spooky_font * self) { return self->data->descent; }
int spooky_font_get_line_skip(const spooky_font * self) { return self->data->line_skip; }
int spooky_font_get_m_dash(const spooky_font * self) { return self->data->m_dash; }
int spooky_font_get_height_line_skip_difference(const spooky_font * self) { return self->data->height - self->data->line_skip; }
int spooky_font_get_height_line_skip_delta(const spooky_font * self) { return self->data->height - spooky_font_get_height_line_skip_difference(self); }

int spooky_font_nearest_x(const spooky_font * self, int x) {
  const spooky_glyph * m_dash = spooky_font_search_glyph_index(self, 'M');
  int Advance =  m_dash->advance;
  float fx = (float)x / (float)Advance;
  int rx = fx >= 0.0 ? (int)(fx + 0.5) : (int)(fx - 0.5);
  return rx * Advance;
}

int spooky_font_nearest_y(const spooky_font * self, int y) {
  float fy = (float)y /(float)self->get_height_line_skip_delta(self);
  int iy = fy >= 0.0 ? (int)(fy + 0.5) : (int)(fy - 0.5);
  return iy * self->get_height_line_skip_delta(self);
}

/* TODO: Load font from memory
const spooky_font * spooky_font_load_from_memory(SDL_Renderer * renderer, int point_size, const char * memory, size_t size) {
  static const int AutoReleaseSource = 1;

  SDL_RWops * rw = SDL_RWFromConstMem(memory, (int)size);
  TTF_Font * ttf_font = TTF_OpenFontRW(rw, AutoReleaseSource, point_size);
  if (!ttf_font) {
    fprintf(stderr,  "Unable to load font from memory. %s\n", TTF_GetError());
    abort();
  }

  const spooky_font * font = spooky_font_acquire();
  font = font->ctor(font, renderer, ttf_font);
  return font;
}
*/

SDL_Texture * spooky_font_glyph_create_texture(SDL_Renderer * renderer, TTF_Font * font, const char * text) {
  SDL_Texture * texture = NULL;
  SDL_Color color = { .r = 255, .g = 255, .b = 255, .a = 255 };

  int skip = 0;
  uint32_t cp = spooky_font_get_code_point(text, &skip);
  /*
     char c[5] = { '\0', '\0', '\0', '\0' };
     c[0] = (char)(cp & 0x000000ff);
     c[1] = (char)(cp & 0x0000ff00) >>  8;
     c[2] = (char)(cp & 0x00ff0000) >> 16;
     c[3] = (char)(cp & 0xff000000) >> 24;
     c[4] = '\0';
     */
  SDL_ClearError();
  SDL_Surface * surface = TTF_RenderGlyph_Solid(font, (uint16_t)cp, color);
  if(!surface || spooky_is_sdl_error(SDL_GetError())) { goto err0; }
  
  texture = SDL_CreateTextureFromSurface(renderer, surface);
  SDL_FreeSurface(surface);
  return texture;

err0:
  SDL_ClearError();
  return NULL;
}

spooky_glyph * spooky_font_glyph_create(TTF_Font * font, uint32_t character, spooky_glyph * glyph) {
  uint16_t c16 = (uint16_t)character;
  if (TTF_GlyphMetrics(font, c16, &glyph->min_x, &glyph->max_x, &glyph->min_y, &glyph->max_y, &glyph->advance) != 0) {
    fprintf(stderr, "Glyph metrics not available for '%c'. %s\n", character, TTF_GetError());
    abort();
  }

  glyph->glyph_width = glyph->max_x - glyph->min_x;
  glyph->glyph_height = glyph->max_y - glyph->min_y;

  return glyph;
}

int spooky_glyph_compare(const void * a, const void * b) {
  spooky_glyph_index l;
  spooky_glyph_index r;
  l = *(const spooky_glyph_index *)a;
  r = *(const spooky_glyph_index *)b;
  if(l.c < r.c) return -1;
  if(l.c == r.c) return 0;
  else return 1;
}

void spooky_font_set_font_attributes(const spooky_font * self) {
  spooky_font_data * data = self->data;

  TTF_Font * f = data->font;

  data->ascent = TTF_FontAscent(f);
  data->descent = TTF_FontDescent(f);
  data->height = TTF_FontHeight(f);
  data->line_skip = TTF_FontLineSkip(f);

  int w = 0;
  /* create glyphs from table */
  for (unsigned int i = 0; i < data->glyph_index_capacity; i++) {
    spooky_glyph_index * index_item = &(data->glyphs[i]);
    index_item->c = i; 
    spooky_glyph * glyph = (spooky_glyph*)(uintptr_t)spooky_font_glyph_create(f, i, &index_item->glyph);
    glyph->offset = w;
    glyph->texture = spooky_font_glyph_create_texture(data->renderer, f, (char *)&i);
    w += glyph->advance;
    data->glyph_index_count++;
  }

  qsort(data->glyphs, data->glyph_index_count, sizeof data->glyphs[0], &spooky_glyph_compare);

  // get an M for an em dash. Which is cheezy
  const spooky_glyph * m = spooky_font_search_glyph_index(self, 'M');
  data->m_dash = m->advance;
}

void spooky_font_measure_text(const spooky_font * self, const char * text, int * width, int * height) {
  spooky_font_data * data = self->data;
  TTF_Font * font = data->font;
  TTF_SizeUTF8(font, text, width, height);
}

bool spooky_font_get_is_drop_shadow(const spooky_font * self) {
  spooky_font_data * data = self->data;
  return data->is_drop_shadow;
}

void spooky_font_set_is_drop_shadow(const spooky_font * self, bool is_drop_shadow) {
  spooky_font_data * data = self->data;
  data->is_drop_shadow = is_drop_shadow;
}

void spooky_font_set_drop_x(const spooky_font * self, int drop_x) {
  spooky_font_data * data = self->data;
  data->drop_x = drop_x; 
}

int spooky_font_get_drop_x(const spooky_font * self) {
  spooky_font_data * data = self->data;
  return data->drop_x;
}

void spooky_font_set_drop_y(const spooky_font * self, int drop_y) {
  spooky_font_data * data = self->data;
  data->drop_y = drop_y;
}

int spooky_font_get_drop_y(const spooky_font * self) {
  spooky_font_data * data = self->data;
  return data->drop_y;
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

int spooky_font_get_utf8_len(const char ch)
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
uint32_t spooky_font_get_utf8_codepoint(const char chr[4], int * skip, bool * is_error)
{
  int bytes = spooky_font_get_utf8_len(*chr);
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

uint32_t spooky_font_get_code_point(const char * s, int * skip) {
  bool is_error;

  char c[4] = { 0 };
  c[0] = *s != '\0' ? *s : '\0';
  c[1] = *(s + 1) != '\0' ? *(s + 1) : '\0';
  c[2] = *(s + 2) != '\0' ? *(s + 2) : '\0';
  c[3] = *(s + 3) != '\0' ? *(s + 3) : '\0';

  uint32_t cp = spooky_font_get_utf8_codepoint((char *)c, skip, &is_error);
  if(is_error) fprintf(stderr, "Error getting codepoint for '%s'\n", s);

  return cp;
}

