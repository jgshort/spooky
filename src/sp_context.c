#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <math.h>

#include "config.h"
#include "sp_pak.h"
#include "sp_limits.h"
#include "sp_hash.h"
#include "sp_context.h"
#include "sp_error.h"
#include "sp_math.h"
#include "sp_gui.h"
#include "sp_font.h"
#include "sp_base.h"
#include "sp_console.h"
#include "sp_debug.h"
#include "sp_help.h"
#include "sp_time.h"
#include "sp_config.h"

#define MAX_FONT_LEN 20
static const size_t max_font_len = MAX_FONT_LEN;
static size_t spooky_font_sizes[MAX_FONT_LEN] = {
  4,
  8,
  9,
  10,
  11,
  12,
  14,
  18,
  24,
  30,
  36,
  48,
  60,
  72,
  84,
  98,
  114,
  133,
  155,
  180
};
static const size_t MIN_FONT_SIZE = 4;
static const size_t MAX_FONT_SIZE = 180;

typedef struct spooky_context_data {
  const spooky_config * config;
  SDL_Window * window;
  SDL_Renderer * renderer;
  SDL_GLContext glContext;
  SDL_Texture * canvas;
  const spooky_console * console;
  const spooky_hash_table * hash;

  size_t font_type_index;
  size_t fonts_index;

  size_t turns;

  const spooky_font * font_current;
  const spooky_font fonts[SPOOKY_FONT_MAX_TYPES][MAX_FONT_LEN];

  float window_scale_factor;
  float renderer_to_window_scale_factor;
  float reserved0;
  float scale_w;
  float scale_h;

  SDL_Rect scaled_window_size;

  int display_index;

  bool is_fullscreen;
  bool is_paused;
  bool is_running;
  char padding[5]; /* not portable */

  size_t fonts_len[SPOOKY_FONT_MAX_TYPES];
} spooky_context_data;
#undef MAX_FONT_LEN

static spooky_context_data global_data = { 0 };

static void spooky_context_translate_point(const spooky_context * context, SDL_Point * point) {
  float scale_factor = context->data->renderer_to_window_scale_factor;
  point->x = (int)floor((float)(point->x) * scale_factor);
  point->y = (int)floor((float)(point->y) * scale_factor);
}

static void spooky_context_translate_rect(const spooky_context * context, SDL_Rect * rect) {
  float scale_factor = context->data->renderer_to_window_scale_factor;
  rect->x = (int)floor((float)(rect->x) * scale_factor);
  rect->y = (int)floor((float)(rect->y) * scale_factor);
  rect->w = (int)floor((float)(rect->w) * scale_factor);
  rect->h = (int)floor((float)(rect->h) * scale_factor);
}

static float spooky_context_get_renderer_to_window_scale_factor(const spooky_context * context) {
  return context->data->renderer_to_window_scale_factor;
}

static float spooky_context_get_scale_w(const spooky_context * context) {
  return context->data->scale_w;
}

static void spooky_context_set_scale_w(const spooky_context * context, float w) {
  context->data->scale_w = w;
}

static float spooky_context_get_scale_h(const spooky_context * context) {
  return context->data->scale_h;
}

static void spooky_context_set_scale_h(const spooky_context * context, float h) {
  context->data->scale_h = h;
}

static const SDL_Rect * spooky_context_get_scaled_rect(const spooky_context * context) {
  return &(context->data->scaled_window_size);
}

static void spooky_context_set_scaled_rect(const spooky_context * context, const SDL_Rect * rect) {
  context->data->scaled_window_size = *rect;
}

static SDL_Window * spooky_context_get_window(const spooky_context * context) {
  return context->data->window;
}

static SDL_Renderer * spooky_context_get_renderer(const spooky_context * context) {
  return context->data->renderer;
}

static SDL_Texture * spooky_context_get_canvas(const spooky_context * context) {
  return context->data->canvas;
}

static void spooky_context_set_canvas(const spooky_context * context, SDL_Texture * texture) {
  if(context->data->canvas) {
    SDL_DestroyTexture(context->data->canvas);
  }
  context->data->canvas = texture;
}

static const spooky_font * spooky_context_get_font(const spooky_context * context) {
  return context->data->font_current;
}

static bool spooky_context_get_is_fullscreen(const spooky_context * context) {
  return context->data->is_fullscreen;
}

static void spooky_context_set_is_fullscreen(const spooky_context * context, bool is_fullscreen) {
  context->data->is_fullscreen = is_fullscreen;
}

static bool spooky_context_get_is_paused(const spooky_context * context) {
  return context->data->is_paused;
}

static int spooky_context_get_display_index(const spooky_context * context) {
  return context->data->display_index;
}

static const spooky_config * spooky_context_get_config(const spooky_context * context) {
  return context->data->config;
}

static void spooky_context_set_is_paused(const spooky_context * context, bool is_paused) {
  context->data->is_paused = is_paused;
}

static const spooky_hash_table * spooky_context_get_hash(const spooky_context * context) {
  return context->data->hash;
}

static void spooky_context_get_center_rect(const spooky_context * context, SDL_Rect * rect) {
  spooky_context_data * data = context->data;
  *rect = data->scaled_window_size;
}

static void spooky_context_get_translated_mouse_state(const spooky_context * context, uint32_t * state, int * x, int * y) {
  float scale_factor = context->data->renderer_to_window_scale_factor;

  int temp_x = 0, temp_y = 0;
  uint32_t temp_state = SDL_GetMouseState(&temp_x, &temp_y);

  temp_x = (int)floor((float)(temp_x) * scale_factor);
  temp_y = (int)floor((float)(temp_y) * scale_factor);

  if(state) { *state = temp_state; }
  if(x) { *x = temp_x; }
  if(y) { *y = temp_y; }
}

static void spooky_index_item_free_item(void * item) {
  if(item) {
    spooky_pack_item_file * pub = item;
    if(pub) {
      if(pub->data) {
        free(pub->data), pub->data = NULL;
        pub->data_len = 0;
      }
      free(pub), pub = NULL;
    }
  }
}

errno_t spooky_init_context(spooky_context * context, FILE * fp) {
  assert(!(context == NULL));

  if(context == NULL) { return SP_FAILURE; }

  context->get_window = &spooky_context_get_window;
  context->get_renderer = &spooky_context_get_renderer;
  context->get_canvas = &spooky_context_get_canvas;
  context->set_canvas = &spooky_context_set_canvas;
  context->get_font = &spooky_context_get_font;
  context->get_is_fullscreen = &spooky_context_get_is_fullscreen;
  context->set_is_fullscreen = &spooky_context_set_is_fullscreen;
  context->get_is_paused = &spooky_context_get_is_paused;
  context->set_is_paused = &spooky_context_set_is_paused;
  context->get_is_running = &spooky_context_get_is_running;
  context->set_is_running = &spooky_context_set_is_running;
  context->get_hash = &spooky_context_get_hash;
  context->get_display_index = &spooky_context_get_display_index;
  context->get_scaled_rect = &spooky_context_get_scaled_rect;
  context->set_scaled_rect = &spooky_context_set_scaled_rect;
  context->get_scale_w = &spooky_context_get_scale_w;
  context->set_scale_w = &spooky_context_set_scale_w;
  context->get_scale_h = &spooky_context_get_scale_h;
  context->set_scale_h = &spooky_context_set_scale_h;
  context->get_center_rect = &spooky_context_get_center_rect;
  context->get_renderer_to_window_scale_factor = &spooky_context_get_renderer_to_window_scale_factor;
  context->get_translated_mouse_state = &spooky_context_get_translated_mouse_state;
  context->translate_point = &spooky_context_translate_point;
  context->translate_rect = &spooky_context_translate_rect;
  context->get_config = &spooky_context_get_config;

  context->data = &global_data;

  const spooky_config * config = spooky_config_acquire();
  config = config->ctor(config);

  context->data->is_running = true;
  context->data->config = config;

  fprintf(stdout, "Initializing...\n");
  fflush(stdout);
  const char * error_message = NULL;

  const spooky_hash_table * hash = NULL;
  context->data->hash = spooky_hash_table_acquire();
  context->data->hash = context->data->hash->ctor(context->data->hash);
  hash = context->data->hash;

  errno_t res = spooky_pack_verify(fp, hash);
  if(res != SP_SUCCESS) {
    fprintf(stderr, "The resource pack is invalid.\n");
    fclose(fp);
    return EXIT_FAILURE;
  }

  SDL_ClearError();
  /* allow high-DPI windows */
  if(!SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, config->get_disable_high_dpi(config))) { goto err0; }
  if(spooky_is_sdl_error(SDL_GetError())) { fprintf(stderr, "> %s\n", SDL_GetError()); }

  if(!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0")) { goto err0; }
  if(spooky_is_sdl_error(SDL_GetError())) { fprintf(stderr, "> %s\n", SDL_GetError()); }

  SDL_ClearError();
  if(SDL_Init(SDL_INIT_EVERYTHING) != 0) { goto err1; }
  if(spooky_is_sdl_error(SDL_GetError())) { fprintf(stderr, "> %s\n", SDL_GetError()); }

  SDL_ClearError();
  if(TTF_Init() != 0) { goto err2; };
  if(spooky_is_sdl_error(SDL_GetError())) { fprintf(stderr, "> %s\n", SDL_GetError()); }

  SDL_ClearError();
  if(SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1) != 0) { goto err3; }
  if(spooky_is_sdl_error(SDL_GetError())) { fprintf(stderr, "> %s\n", SDL_GetError()); }

  SDL_ClearError();
  if(SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24) != 0) { goto err3; }
  if(spooky_is_sdl_error(SDL_GetError())) { fprintf(stderr, "> %s\n", SDL_GetError()); }

  SDL_ClearError();
  if(SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8) != 0) { goto err3; }
  if(spooky_is_sdl_error(SDL_GetError())) { fprintf(stderr, "> %s\n", SDL_GetError()); }

  SDL_ClearError();
  if(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2) != 0) { goto err3; }
  if(spooky_is_sdl_error(SDL_GetError())) { fprintf(stderr, "> %s\n", SDL_GetError()); }

  SDL_ClearError();
  if(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2) != 0) { goto err3; }
  if(spooky_is_sdl_error(SDL_GetError())) { fprintf(stderr, "> %s\n", SDL_GetError()); }

  global_data.scaled_window_size.x = 0;
  global_data.scaled_window_size.y = 0;
  global_data.scaled_window_size.w = spooky_gui_window_default_logical_width;
  global_data.scaled_window_size.h = spooky_gui_window_default_logical_height;

  global_data.window_scale_factor = 1.0f;

  SDL_ClearError();
  SDL_Window * window = SDL_CreateWindow(PACKAGE_STRING, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, config->get_window_width(config), config->get_window_height(config), spooky_gui_window_flags);
  if(spooky_is_sdl_error(SDL_GetError())) { fprintf(stderr, "> %s\n", SDL_GetError()); }
  if(window == NULL || spooky_is_sdl_error(SDL_GetError())) { goto err4; }

  global_data.display_index = SDL_GetWindowDisplayIndex(window);

  SDL_SetWindowSize(window, config->get_window_width(config), config->get_window_height(config));
  SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
  SDL_ShowWindow(window);

  SDL_ClearError();
  const int default_driver = -1;
  SDL_Renderer * renderer = SDL_CreateRenderer(window, default_driver, spooky_gui_renderer_flags);
  if(spooky_is_sdl_error(SDL_GetError())) { fprintf(stderr, "> %s\n", SDL_GetError()); }
  if(renderer == NULL || spooky_is_sdl_error(SDL_GetError())) { goto err5; }

  int renderer_w, renderer_h;
  SDL_GetRendererOutputSize(renderer, &renderer_w, &renderer_h);
  global_data.renderer_to_window_scale_factor = (float)(renderer_w / config->get_window_width(config));
  global_data.scaled_window_size.w = renderer_w;
  global_data.scaled_window_size.h = renderer_h;

  SDL_RenderSetViewport(renderer, &global_data.scaled_window_size);
  const SDL_Color c = { .r = 1, .g = 20, .b = 36, .a = 255 };
  const spooky_gui_rgba_context  * rgba = spooky_gui_push_draw_color(renderer, &c);
  {
    SDL_RenderFillRect(renderer, NULL);
    spooky_gui_pop_draw_color(rgba);
  }

  SDL_RenderPresent(renderer);

  SDL_ClearError();
  SDL_GLContext glContext = SDL_GL_CreateContext(window);
  if(spooky_is_sdl_error(SDL_GetError())) { fprintf(stderr, "> %s\n", SDL_GetError()); }
  if(glContext == NULL || spooky_is_sdl_error(SDL_GetError())) { goto err6; }

  SDL_ClearError();
  SDL_Texture * canvas = SDL_CreateTexture(renderer
      , SDL_GetWindowPixelFormat(window)
      , SDL_TEXTUREACCESS_TARGET
      , config->get_canvas_width(config)
      , config->get_canvas_height(config)
      );
  if(!canvas || spooky_is_sdl_error(SDL_GetError())) { goto err7; }

  fprintf(stdout, "Window: (%i, %i), Renderer: (%i, %i), Scaled: (%i, %i), Canvas: (%i, %i)\n",
      config->get_window_width(config)
    , config->get_window_height(config)
    , renderer_w
    , renderer_h
    , global_data.scaled_window_size.w
    , global_data.scaled_window_size.h
    , config->get_canvas_width(config)
    , config->get_canvas_height(config));

  global_data.window = window;
  global_data.renderer = renderer;
  global_data.glContext = glContext;
  global_data.canvas = canvas;

  global_data.turns = 0;

  /* This is a strange defect I can't figure out:
   * Fonts don't render until a call to SDL_ShowWindow. So calling ShowWindow here,
   * before the font ctor fixes the issue. I don't know why :(
   */
  SDL_ShowWindow(window);


  const char * font_name = config->get_font_name(config);

  // 16
  // 4 -> 6 -> 8 -> 10 -> 11 -> 14 -> 18
  size_t closest_font_size = 4;
  size_t requested_font_size = (size_t)config->get_font_size(config);
  for(size_t i = 0; i < max_font_len; i++) {
    size_t point_size = spooky_font_sizes[i];
    if(point_size > requested_font_size) {
      global_data.fonts_index = i;
      closest_font_size = spooky_font_sizes[i];
      break;
    }
  }
  if(closest_font_size < MIN_FONT_SIZE) { closest_font_size = MIN_FONT_SIZE; }
  if(closest_font_size > MAX_FONT_SIZE) { closest_font_size = MAX_FONT_SIZE; }
  global_data.font_type_index = 0;

  spooky_pack_item_file * ttf = NULL;
  void * temp = NULL;
  hash->find(hash, font_name, strnlen(font_name, SPOOKY_MAX_STRING_LEN), &temp);
  ttf = temp;

  const spooky_font * font = spooky_font_acquire();
  font = spooky_font_init((spooky_font *)(uintptr_t)font);
  font = font->ctor(font, renderer, ttf->data, ttf->data_len, (int)closest_font_size);

  fprintf(stdout, "Font '%s' set to %ipt\n", font->get_name(font), font->get_point_size(font));
  global_data.font_current = font;

  fprintf(stdout, "Done!\n");
  fflush(stdout);

  return SP_SUCCESS;

err7:
  if(!error_message) { error_message = "Unable to create canvas."; }

err6:
  /* unable to create GL context */
  if(!error_message) { error_message = "Unable to create GL context."; }
  SDL_DestroyRenderer(renderer), renderer = NULL;

err5:
  /* unable to create SDL renderer */
  if(!error_message) { error_message = "Unable to create renderer."; }
  SDL_DestroyWindow(window), window = NULL;

err4:
  /* unable to create SDL window */
  if(!error_message) { error_message = "Unable to create window."; }

err3:
  /* unable to set GL attributes */
  if(!error_message) { error_message = "Unable to set GL attributes."; }
  TTF_Quit();

err2:
  /* unable to initialize TTF */
  if(!error_message) { error_message = "Unable to initialize font library."; }
  SDL_Quit();

err1:
  /* unable to initialize SDL */
  if(!error_message) { error_message = "Unable to initialize media library."; }

err0:
  /* unable to enable high DPI hint */
  if(!error_message) { error_message = "Unable to enable high DPI hint."; }

  fprintf(stderr, "\n%s\n", error_message);
  return SP_FAILURE;
}

void spooky_release_context(spooky_context * context) {
  if(context) {
    spooky_context_data * data = context->data;

    spooky_hash_table_release(data->hash, &spooky_index_item_free_item);

    if(data->canvas) {
      SDL_ClearError();
      SDL_DestroyTexture(data->canvas), data->canvas = NULL;
      if(spooky_is_sdl_error(SDL_GetError())) { fprintf(stderr, "> %s\n", SDL_GetError()); }
      const char * error = SDL_GetError();
      if(spooky_is_sdl_error(error)) {
        fprintf(stderr, "Non-fatal error: Unable to destroy canvas, '%s'.\n", error);
      }
    }

    if(data->glContext) {
      SDL_ClearError();
      SDL_GL_DeleteContext(data->glContext), data->glContext = NULL;
      if(spooky_is_sdl_error(SDL_GetError())) { fprintf(stderr, "> %s\n", SDL_GetError()); }
      const char * error = SDL_GetError();
      if(spooky_is_sdl_error(error)) {
        fprintf(stderr, "Non-fatal error: Unable to release GL context, '%s'.\n", error);
      }
    }

    if(data->renderer) {
      SDL_ClearError();
      SDL_DestroyRenderer(data->renderer), data->renderer = NULL;
      if(spooky_is_sdl_error(SDL_GetError())) { fprintf(stderr, "> %s\n", SDL_GetError()); }
      const char * error = SDL_GetError();
      if(spooky_is_sdl_error(error)) {
        fprintf(stderr, "Non-fatal error: Unable to destroy renderer, '%s'.\n", error);
      }
    }

    if(data->window) {
      SDL_ClearError();
      SDL_DestroyWindow(data->window), data->window = NULL;
      if(spooky_is_sdl_error(SDL_GetError())) { fprintf(stderr, "> %s\n", SDL_GetError()); }
      const char * error = SDL_GetError();
      if(spooky_is_sdl_error(error)) {
        fprintf(stderr, "Non-fatal error: Unable to destroy window, '%s'.\n", error);
      }
    }
  }
}

errno_t spooky_quit_context(spooky_context * context) {
  assert(!(context == NULL));
  if(context == NULL) { return SP_FAILURE; }

  fprintf(stdout, "Shutting down...");
  fflush(stdout);

  spooky_release_context(context);

  TTF_Quit();
  SDL_Quit();

  fprintf(stdout, " Done!\n");
  fflush(stdout);

  return SP_SUCCESS;
}

errno_t spooky_test_resources(const spooky_context * context) {
  assert(!(context == NULL));
  if(context == NULL) { goto err0; }

  spooky_context_data * data = context->data;
  assert(!(data->renderer == NULL));
  if(data->renderer == NULL) { goto err0; }

  fprintf(stdout, "Testing resources...");
  fflush(stdout);

  SDL_Renderer * renderer = data->renderer;

  /* check surface resource path and method */
  SDL_Surface * test_surface = NULL;
  if(spooky_gui_load_image("res/test0.png", 13, &test_surface) != SP_SUCCESS) { goto err0; }
  if(test_surface == NULL) goto err0;
  SDL_ClearError();
  SDL_FreeSurface(test_surface), test_surface = NULL;
  if(spooky_is_sdl_error(SDL_GetError())) { fprintf(stderr, "> %s\n", SDL_GetError()); }

  /* check texture method */
  SDL_Texture * test_texture = NULL;
  if(spooky_gui_load_texture(renderer, "res/test0.png", 13, &test_texture) != SP_SUCCESS) { goto err0; }
  if(test_texture == NULL) { goto err0; }
  SDL_ClearError();
  SDL_DestroyTexture(test_texture), test_texture = NULL;
  if(spooky_is_sdl_error(SDL_GetError())) { fprintf(stderr, "> %s\n", SDL_GetError()); }

  fprintf(stdout, " Done!\n");
  fflush(stdout);
  return SP_SUCCESS;

err0:
  return SP_FAILURE;
}

bool spooky_context_get_is_running(const spooky_context * context) {
  return context->data->is_running;
}

void spooky_context_set_is_running(const spooky_context * context, bool value) {
  context->data->is_running = value;
}
