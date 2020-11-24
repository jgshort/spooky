#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <limits.h>
#include <assert.h>
#include <memory.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "sp_error.h"
#include "sp_font.h"

const char * spooky_default_font_names[SPOOKY_FONT_MAX_TYPES] = {
  "pr.number",
  "print.char"
//  "deja.sans.mono"
};

static const int spooky_font_outline = 1;
static const size_t spooky_glyphs_alloc_unit = 127;

const int spooky_default_font_size = 1;

typedef struct spooky_glyph {
  SDL_Texture * texture;
  size_t offset;
  uint32_t c;
  int min_x;
  int max_x;
  int min_y;
  int max_y;
  int glyph_width;
  int glyph_height;
  int advance;
} spooky_glyph;

typedef struct spooky_font_data {
  SDL_Renderer * renderer;
  TTF_Font * font;
  TTF_Font * font_outline;
  const char * name;
  int point_size;
  int height;
  int ascent;
  int descent;
  int line_skip;
  int m_dash;
  int drop_x;
  int drop_y;
  bool is_drop_shadow;
  char padding[7]; /* not portable */
  /* glyph array */
  size_t glyphs_capacity;
  size_t glyphs_count;
  spooky_glyph * glyphs;
} spooky_font_data;

const spooky_font * spooky_font_cctor(const spooky_font * self, SDL_Renderer * renderer, int point_size, SDL_RWops * stream, TTF_Font * ttf_font, TTF_Font * ttf_font_outline);

static int spooky_font_get_point_size(const spooky_font * self);
static uint32_t spooky_font_get_code_point(const char * s, int * skip);
//static void spooky_font_realloc_glyphs(const spooky_font * self);
static void spooky_font_write(const spooky_font * self, const SDL_Point * destination, const SDL_Color * color, const char * s, int * w, int * h);
static void spooky_font_write_to_renderer(const spooky_font * self, SDL_Renderer * renderer, const SDL_Point * destination, const SDL_Color * color, const char * s, int * w, int * h);

static errno_t spooky_font_glyph_create_texture(const spooky_font * self, const char * text, SDL_Texture ** out_texture);
static spooky_glyph * spooky_font_glyph_create(const spooky_font * self, uint32_t character, spooky_glyph * glyph);

static int spooky_glyph_compare(const void * a, const void * b);

static const char * spooky_font_get_name(const spooky_font * self);
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

const spooky_font * spooky_font_alloc() {
  spooky_font * self = calloc(1, sizeof * self);
  if(self == NULL) { 
    fprintf(stderr, "Unable to allocate memory.");
    abort();
  }
  return self;
}

const spooky_font * spooky_font_init(spooky_font * self) {
  assert(self != NULL);
  if(!self) { abort(); }

  self->ctor = &spooky_font_ctor;
  self->dtor = &spooky_font_dtor;
  self->free = &spooky_font_free;
  self->release = &spooky_font_release;
  self->write = &spooky_font_write;
  self->write_to_renderer = &spooky_font_write_to_renderer;
  self->get_name = &spooky_font_get_name;
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
  self->get_point_size = &spooky_font_get_point_size;
  return self;
}

const spooky_font * spooky_font_acquire() {
  return spooky_font_init((spooky_font *)(uintptr_t)spooky_font_alloc());
}

const spooky_font * spooky_font_cctor(const spooky_font * self, SDL_Renderer * renderer, int point_size, SDL_RWops * stream, TTF_Font * ttf_font, TTF_Font * ttf_font_outline) {
  assert(self != NULL && renderer != NULL);
  spooky_font_data * data = calloc(1, sizeof * data);
  if(!data) { abort(); }

  assert(ttf_font != NULL && ttf_font_outline != NULL);

  data->name = TTF_FontFaceFamilyName(ttf_font);
  data->point_size = point_size;
  data->font = ttf_font;
  data->font_outline = ttf_font_outline;
  data->renderer = renderer;
  data->height = 0;
  data->ascent = 0;
  data->descent = 0;
  data->line_skip = 0;
  data->m_dash = 0;

  data->is_drop_shadow = true;
  data->drop_x = 1;
  data->drop_y = 1;

  data->glyphs_capacity = spooky_glyphs_alloc_unit;
  data->glyphs_count = 0;

  data->glyphs = calloc(data->glyphs_capacity, sizeof * data->glyphs);
  if(data->glyphs == NULL) {
    fprintf(stderr, "Unable to allocate memory for glyphs.");
    abort();
  }
  ((spooky_font *)(uintptr_t)self)->data = data;

  /* Set font attributes requires self->data, set above */
  spooky_font_set_font_attributes(self);

  TTF_CloseFont(data->font);
  TTF_CloseFont(data->font_outline);
  SDL_RWclose(stream);

  return self;
}

const spooky_font * spooky_font_ctor(const spooky_font * self, SDL_Renderer * renderer, const void * mem, size_t mem_len, int point_size) {
  const int FREE_SRC = 0;
  assert(mem_len < INT_MAX);

  SDL_RWops * stream = SDL_RWFromConstMem(mem, (int)mem_len);

  SDL_ClearError();
  TTF_Font * ttf_font = TTF_OpenFontRW(stream, FREE_SRC, point_size);
  assert(ttf_font);

  SDL_ClearError();
  TTF_Font * ttf_font_outline = TTF_OpenFontRW(stream, FREE_SRC, point_size);
  TTF_SetFontOutline(ttf_font_outline, spooky_font_outline);

  const spooky_font * font = spooky_font_cctor(self, renderer, point_size, stream, ttf_font, ttf_font_outline); 

  return font;
}

const spooky_font * spooky_font_dtor(const spooky_font * self) {
  if(self) {
    spooky_font_data * data = self->data;
 
    if(data->glyphs) {
      spooky_glyph * glyph = data->glyphs;
      while(glyph < data->glyphs + data->glyphs_count) {
        if(glyph->texture != NULL) {
          SDL_DestroyTexture(glyph->texture), glyph->texture = NULL;
        }
        glyph++;
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

const char * spooky_font_get_name(const spooky_font * self) {
  return self->data->name;
}

bool spooky_glyph_binary_search(const spooky_glyph * glyphs, size_t low, size_t n, uint32_t c, size_t * out_index) {
  assert(out_index != NULL && n > 0);
  size_t i = low, j = n - 1;

  while(i <= j) {
    assert(j != 0);
    size_t k = i + ((j - i) / 2);
    assert(k <= n);

    if(glyphs[k].c < c) {
      i = k + 1;
    } else if(glyphs[k].c > c) {
      j = k - 1;
    } else {
      *out_index = k;
      return true;
    } 
  }

  return false;
}

const spooky_glyph * spooky_font_search_glyph_index(const spooky_font * self, uint32_t c) {
  spooky_font_data * data = self->data;
  size_t n = data->glyphs_count, index = 0; 
  assert(n > index);
  bool found = spooky_glyph_binary_search(data->glyphs, 0, n, c, &index);
  return found ? data->glyphs + index : NULL;
}

void spooky_font_write(const spooky_font * self, const SDL_Point * destination, const SDL_Color * color, const char * s, int * w, int * h) {
  spooky_font_write_to_renderer(self, self->data->renderer, destination, color, s, w, h);
}

void spooky_font_write_to_renderer(const spooky_font * self, SDL_Renderer * renderer, const SDL_Point * destination, const SDL_Color * color, const char * s, int * w, int * h) {
  //spooky_font_data * data = self->data;
  
  const spooky_glyph * M = spooky_font_search_glyph_index(self, 'M');

  int destX = destination->x;
  int destY = destination->y;

  int width = 0;
  int x = 0, y = 0;

  if(w) { *w = 0; }

  while (*s != '\0') {
    int skip = 0;
    uint32_t c = spooky_font_get_code_point(s, &skip);

    if (c == '\n') {
      if(w && *w == 0) { *w = width; }
      y += spooky_font_get_line_skip(self) + 1;
      x = 0;
    } else if (c == '\t') {
      const spooky_glyph * space = spooky_font_search_glyph_index(self, ' ');
      x += (space->advance * 4);
      width += (space->advance * 4);
    } else if (c == ' ') {
      const spooky_glyph * space = spooky_font_search_glyph_index(self, ' ');
      x += space->advance;
      width += space->advance;
    } else {
      const spooky_glyph * g = spooky_font_search_glyph_index(self, c);
      if(g == NULL) {
        g = spooky_font_search_glyph_index(self, '?');
        /*
        spooky_font_realloc_glyphs(self);

        spooky_glyph * glyph = data->glyphs + data->glyphs_count;
        memset(glyph, 0, sizeof * glyph);

        glyph = spooky_font_glyph_create(self, c, glyph);
        glyph->c = c;
        glyph->offset = data->glyphs_count;
        (void)spooky_font_glyph_create_texture(self, s, &glyph->texture);
        data->glyphs_count++;
        qsort(data->glyphs, data->glyphs_count, sizeof * data->glyphs, &spooky_glyph_compare);
        g = spooky_font_search_glyph_index(self, c); 
        */
      }
      int advance = M->advance; /* use M-dash for consistent spacing; weird due to font outline */
      if(g) {
        SDL_Texture * texture = g->texture;
        if(texture != NULL) {
          SDL_Rect dest = { .x = 0, .y = 0, .w = advance, .h = spooky_font_get_height(self) };
          
          dest.x = x + destX;
          dest.y = y + destY;

          /* Render font in a specific color: */
          SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
          SDL_SetTextureAlphaMod(texture, color->a);
          SDL_SetTextureColorMod(texture, color->r, color->g, color->b);
          
          if(SDL_RenderCopy(renderer, texture, NULL, &dest) != 0) {
            fprintf(stderr, "Unable to render glyph during Write: %s\n", SDL_GetError()); 
            abort();
          }
        }
      }
      assert(advance > -1);
      
      x += advance;
      width += advance;
    }

    s += skip;
  }

  if(h) { *h = y; }
  if(w && *w == 0) { *w = width; }
}

int spooky_font_get_height(const spooky_font * self) { return self->data->height; }
int spooky_font_get_ascent(const spooky_font * self) { return self->data->ascent; }
int spooky_font_get_descent(const spooky_font * self) { return self->data->descent; }
int spooky_font_get_line_skip(const spooky_font * self) { return self->data->line_skip; }
int spooky_font_get_m_dash(const spooky_font * self) { return self->data->m_dash; }
int spooky_font_get_height_line_skip_difference(const spooky_font * self) { return self->data->height - self->data->line_skip; }
int spooky_font_get_height_line_skip_delta(const spooky_font * self) { return self->data->height - spooky_font_get_height_line_skip_difference(self); }

int spooky_font_nearest_x(const spooky_font * self, int x) {
  static const spooky_glyph * m_dash = NULL;
  if(m_dash == NULL) {
    m_dash = spooky_font_search_glyph_index(self, 'M');
  }
  assert(m_dash != NULL && x > 0);
  int advance = m_dash->advance;
  assert(advance > 0);
  float fx = (float)x / (float)advance;
  int rx = fx >= 0.0 ? (int)(fx + 0.5) : (int)(fx - 0.5);
  return rx * advance;
}

int spooky_font_nearest_y(const spooky_font * self, int y) {
  float fy = (float)y /(float)spooky_font_get_height_line_skip_delta(self);
  int iy = fy >= 0.0 ? (int)(fy + 0.5) : (int)(fy - 0.5);
  return iy * spooky_font_get_height_line_skip_delta(self);
}

errno_t spooky_font_glyph_create_texture(const spooky_font * self, const char * text, SDL_Texture ** out_texture) {
  assert(out_texture != NULL && self->data->font_outline != NULL);

  const SDL_Color black = { .r = 0, .g = 0, .b = 0, .a = 255 }; 
  const SDL_Color white = { .r = 255, .g = 255, .b = 255, .a = 255 };
  
  SDL_ClearError();
  SDL_Surface *bg_surface = TTF_RenderUTF8_Blended(self->data->font_outline, text, black);
  if(!bg_surface || spooky_is_sdl_error(SDL_GetError())) { goto err0; }
  
  SDL_ClearError();
  SDL_Surface *fg_surface = TTF_RenderUTF8_Blended(self->data->font, text, white);
  if(!fg_surface || spooky_is_sdl_error(SDL_GetError())) { goto err1; }
  
  assert(bg_surface != NULL && fg_surface != NULL);
 
  SDL_ClearError();
  SDL_Texture * fg_texture = SDL_CreateTextureFromSurface(self->data->renderer, fg_surface);
  if(!fg_texture || spooky_is_sdl_error(SDL_GetError())) { goto err2; }

  SDL_ClearError();
  SDL_Texture * bg_texture = SDL_CreateTextureFromSurface(self->data->renderer, bg_surface);
  if(!bg_texture || spooky_is_sdl_error(SDL_GetError())) { goto err3; }

  SDL_ClearError();
  SDL_Texture * texture = SDL_CreateTexture(self->data->renderer
      , SDL_PIXELFORMAT_RGBA8888
      , SDL_TEXTUREACCESS_TARGET
      , bg_surface->w
      , bg_surface->h
      );
  if(!texture || spooky_is_sdl_error(SDL_GetError())) { goto err4; }

  assert(fg_texture != NULL && bg_texture != NULL && texture != NULL);
  
  uint8_t r, g, b, a;
  SDL_GetRenderDrawColor(self->data->renderer, &r, &g, &b, &a);
  /* make texture a temporary render target */
  SDL_SetRenderTarget(self->data->renderer, texture);

  /* clear the texture render target */
  SDL_SetRenderDrawColor(self->data->renderer, 0, 0, 0, 0);
  SDL_RenderFillRect(self->data->renderer, NULL); /* screen color */
  SDL_RenderClear(self->data->renderer);

  /* render the outline */
  SDL_Rect bg_rect = {.x = 0, .y = 0, .w = bg_surface->w, .h = bg_surface->h}; 
  
  SDL_RenderCopy(self->data->renderer, bg_texture, NULL, &bg_rect);

  /* render the text */
  SDL_Rect fg_rect = {.x = 1, .y = 1, .w = fg_surface->w, .h = fg_surface->h }; 
  SDL_RenderCopy(self->data->renderer, fg_texture, NULL, &fg_rect);

  /* reset the render target */
  SDL_SetRenderTarget(self->data->renderer, NULL);
  SDL_SetRenderDrawColor(self->data->renderer, r, g, b, a);

  SDL_FreeSurface(bg_surface), bg_surface = NULL;
  SDL_FreeSurface(fg_surface), fg_surface = NULL;
  SDL_DestroyTexture(bg_texture), bg_texture = NULL;
  SDL_DestroyTexture(fg_texture), fg_texture = NULL;

  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
  SDL_SetTextureAlphaMod(texture, 255);
  SDL_SetTextureColorMod(texture, 255, 255, 255);

  *out_texture = texture;
  return SP_SUCCESS;

err4:
  SDL_DestroyTexture(bg_texture), bg_texture = NULL;

err3:
  SDL_DestroyTexture(fg_texture), fg_texture = NULL;

err2:
  SDL_FreeSurface(fg_surface), fg_surface = NULL;

err1:
  SDL_FreeSurface(bg_surface), bg_surface = NULL;

err0:
  *out_texture = NULL;
  return SP_FAILURE;
}

spooky_glyph * spooky_font_glyph_create(const spooky_font * self, uint32_t character, spooky_glyph * glyph) {
  uint16_t c16 = (uint16_t)character;

  if (TTF_GlyphMetrics(self->data->font, c16, &glyph->min_x, &glyph->max_x, &glyph->min_y, &glyph->max_y, NULL) != 0) {
    fprintf(stderr, "Glyph metrics not available for '%c'. %s\n", character, TTF_GetError());
    abort();
  }

  glyph->glyph_width = glyph->max_x - glyph->min_x;
  glyph->glyph_height = glyph->max_y - glyph->min_y;

  return glyph;
}

int spooky_glyph_compare(const void * a, const void * b) {
  const spooky_glyph * l = (const spooky_glyph *)a;
  const spooky_glyph * r = (const spooky_glyph *)b;
  if(l->c < r->c) return -1;
  if(l->c == r->c) return 0;
  else return 1;
}

void spooky_font_set_font_attributes(const spooky_font * self) {
  assert(self != NULL && self->data != NULL && self->data->font != NULL);
  spooky_font_data * data = self->data;

  TTF_Font * ttf_font = data->font;

  data->ascent = TTF_FontAscent(ttf_font);
  data->descent = TTF_FontDescent(ttf_font);
  data->height = TTF_FontHeight(ttf_font);
  data->line_skip = TTF_FontLineSkip(ttf_font);

  size_t w = 0;
  /* create glyphs from table */
  for(size_t i = 0; i < data->glyphs_capacity; ++i) {
    spooky_glyph * glyph = data->glyphs + i;
    if(i < UINT_MAX) {
      char buf[256] = { 0 };
      glyph->c = (uint32_t)i;
      glyph = spooky_font_glyph_create(self, glyph->c, glyph);
      glyph->offset = w;
      int out = snprintf(buf, sizeof(buf), "%c", glyph->c);
      assert(out > 0 && (size_t)out < sizeof(buf));
      (void)spooky_font_glyph_create_texture(self, buf, &glyph->texture);
      
      int glyph_w = 0, glyph_h = 0;
      SDL_QueryTexture(glyph->texture, NULL, NULL, &glyph_w, &glyph_h);
      glyph->advance = glyph_w;

      assert(glyph_w >= 0);
      w += (size_t)glyph->advance;
      data->glyphs_count++;
    } else {
      fprintf(stderr, "Fatal error generating glyph index.");
      abort();
    }
  }

  qsort(data->glyphs, data->glyphs_count, sizeof * data->glyphs, &spooky_glyph_compare);

  // get an M for an em dash. Which is cheezy
  const spooky_glyph * m = spooky_font_search_glyph_index(self, 'M');
  data->m_dash = m->advance;
}

int spooky_font_get_point_size(const spooky_font * self) {
  assert(self != NULL);
  return self->data->point_size;
}

void spooky_font_measure_text(const spooky_font * self, const char * text, int * width, int * height) {
  //spooky_font_data * data = self->data;
  size_t len = strnlen(text, 1024);
  const spooky_glyph * g = spooky_font_search_glyph_index(self, 'M');
  *width = g->advance * (int)len;
  size_t line_count = 1;
  for(size_t i = 0; i < len; i++) {
    if(text[i] == '\n') { line_count++; }
  }
  *height = g->glyph_height * (int)line_count;
  /*
  TTF_Font * font = data->font_outline;
  TTF_SizeUTF8(font, text, width, height);
  */
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

