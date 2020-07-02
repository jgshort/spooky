#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <math.h>

#include "config.h"
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

#define MAX_FONT_LEN 120
static const size_t max_font_len = MAX_FONT_LEN;
static size_t spooky_font_sizes[MAX_FONT_LEN] = { 
  0, 0, 0, 0,
  4, 0, 0, 0,
  8,
  9,
  10,
  11,
  12, 0,
  14, 0, 0, 0,
  18, 0, 0, 0, 0, 0,
  24, 0, 0, 0, 0, 0,
  30, 0, 0, 0, 0, 0,
  36, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  48, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  72, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  84, 0
};
#undef MAX_FONT_LEN

typedef struct spooky_context_data {
  SDL_Window * window;
  SDL_Renderer * renderer;
  SDL_GLContext glContext;
  SDL_Texture * canvas;

  size_t fonts_len;
  const spooky_font * font_current;
  const spooky_font ** fonts_index;
  const spooky_font ** fonts;

  float window_scale_factor;
  float reserved0;

  int window_width;
  int window_height;

  bool is_fullscreen;
  bool is_paused;

  char padding[6]; /* not portable */
} spooky_context_data;

static spooky_context_data global_data = { 0 };

static SDL_Window * spooky_context_get_window(const spooky_context * context) {
  return context->data->window;
}

static SDL_Renderer * spooky_context_get_renderer(const spooky_context * context) {
  return context->data->renderer;
}

static SDL_Texture * spooky_context_get_canvas(const spooky_context * context) {
  return context->data->canvas;
}

static const spooky_font * spooky_context_get_font(const spooky_context * context) {
  return context->data->font_current;
}

static int spooky_context_get_window_width(const spooky_context * context) {
  return context->data->window_width;
}

static void spooky_context_set_window_width(const spooky_context * context, int window_width) {
  context->data->window_width = window_width;
}

static int spooky_context_get_window_height(const spooky_context * context) {
  return context->data->window_height;
}

static void spooky_context_set_window_height(const spooky_context * context, int window_height) {
  context->data->window_height = window_height;
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

static void spooky_context_set_is_paused(const spooky_context * context, bool is_paused) {
  context->data->is_paused = is_paused;  
}

errno_t spooky_init_context(spooky_context * context) {
  assert(!(context == NULL));

  if(context == NULL) { return SP_FAILURE; }

  context->get_window = &spooky_context_get_window;
  context->get_renderer = &spooky_context_get_renderer;
  context->get_canvas = &spooky_context_get_canvas;
  context->get_font = &spooky_context_get_font;
  context->get_window_width = &spooky_context_get_window_width;
  context->set_window_width = &spooky_context_set_window_width;
  context->get_window_height = &spooky_context_get_window_height;
  context->set_window_height = &spooky_context_set_window_height;
  context->get_is_fullscreen = &spooky_context_get_is_fullscreen;
  context->set_is_fullscreen = &spooky_context_set_is_fullscreen;
  context->get_is_paused = &spooky_context_get_is_paused;
  context->set_is_paused = &spooky_context_set_is_paused;
  context->data = &global_data;

  fprintf(stdout, "Initializing...");
  fflush(stdout);
  const char * error_message = NULL;

  SDL_ClearError();
  /* allow high-DPI windows */
  if(!SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0")) { goto err0; }
  if(spooky_is_sdl_error(SDL_GetError())) { fprintf(stderr, "> %s\n", SDL_GetError()); }

  if(!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0")) { goto err0; }
  if(spooky_is_sdl_error(SDL_GetError())) { fprintf(stderr, "> %s\n", SDL_GetError()); }

  SDL_ClearError();
  if(SDL_Init(SDL_INIT_VIDEO) != 0) { goto err1; }
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

  global_data.window_width = spooky_window_default_width;
  global_data.window_height = spooky_window_default_height;

  bool spooky_gui_is_fullscreen = false;
  uint32_t window_flags = 
    spooky_gui_is_fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0
    | SDL_WINDOW_OPENGL
    | SDL_WINDOW_HIDDEN
    | SDL_WINDOW_ALLOW_HIGHDPI
    | SDL_WINDOW_RESIZABLE
    ;

  global_data.window_scale_factor = 1.0f;
  if(!spooky_gui_is_fullscreen) {
    SDL_Rect window_bounds;
    SDL_ClearError();
    if(SDL_GetDisplayUsableBounds(0, &window_bounds) == 0) {
      while(global_data.window_width + spooky_window_default_width < window_bounds.w) {
        global_data.window_width += spooky_window_default_width;
        global_data.window_scale_factor += 1.0f;
      }
      while(global_data.window_height + spooky_window_default_height < window_bounds.h) {
        global_data.window_height += spooky_window_default_height;
      }
    }
    if(spooky_is_sdl_error(SDL_GetError())) { fprintf(stderr, "> %s\n", SDL_GetError()); }
  }

  SDL_ClearError();
  SDL_Window * window = SDL_CreateWindow(PACKAGE_STRING, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, global_data.window_width, global_data.window_height, window_flags);
  if(spooky_is_sdl_error(SDL_GetError())) { fprintf(stderr, "> %s\n", SDL_GetError()); }
  if(window == NULL || spooky_is_sdl_error(SDL_GetError())) { goto err4; }

  uint32_t renderer_flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;
  SDL_ClearError();
  const int default_driver = -1;
  SDL_Renderer * renderer = SDL_CreateRenderer(window, default_driver, renderer_flags);
  if(spooky_is_sdl_error(SDL_GetError())) { fprintf(stderr, "> %s\n", SDL_GetError()); }
  if(renderer == NULL || spooky_is_sdl_error(SDL_GetError())) { goto err5; }

  SDL_Color c0 = { 0 };
  SDL_GetRenderDrawColor(renderer, &c0.r, &c0.g, &c0.b, & c0.a);
  const SDL_Color c = { .r = 1, .g = 20, .b = 36, .a = 255 };
  SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
  SDL_RenderFillRect(renderer, NULL);
  SDL_SetRenderDrawColor(renderer, c0.r, c0.g, c0.b, c0.a);
  SDL_RenderPresent(renderer);

  SDL_ClearError();
  SDL_GLContext glContext = SDL_GL_CreateContext(window);
  if(spooky_is_sdl_error(SDL_GetError())) { fprintf(stderr, "> %s\n", SDL_GetError()); }
  if(glContext == NULL || spooky_is_sdl_error(SDL_GetError())) { goto err6; }

  SDL_ClearError();
  SDL_Texture * canvas = SDL_CreateTexture(renderer
      , SDL_PIXELFORMAT_RGBA8888
      , SDL_TEXTUREACCESS_TARGET
      , spooky_window_default_logical_width 
      , spooky_window_default_logical_height 
      );
  if(!canvas || spooky_is_sdl_error(SDL_GetError())) { goto err7; }

  global_data.renderer = renderer;
  global_data.window = window;
  global_data.glContext = glContext;
  global_data.canvas = canvas;

  /* This is a strange defect I can't figure out: 
   * Fonts don't render until a call to SDL_ShowWindow. So calling ShowWindow here,
   * before the font ctor fixes the issue. I don't know why :(
   */
  SDL_ShowWindow(window);

  global_data.fonts_len = max_font_len;
  global_data.fonts = calloc(max_font_len, sizeof * global_data.fonts);

  int point_size = 1;
  const spooky_font ** next = global_data.fonts;
  do {
    assert(point_size > 0);
    assert(point_size <= (int)global_data.fonts_len);
    *next = spooky_font_acquire();
    *next = (*next)->ctor(*next, renderer, spooky_default_font_name, point_size);
    point_size++;
  } while(++next < global_data.fonts + global_data.fonts_len);
  
  global_data.fonts_index = global_data.fonts + spooky_default_font_size * (int)floor(global_data.window_scale_factor);
  global_data.font_current = *global_data.fonts_index;

  fprintf(stdout, " Done!\n");
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
  if(context != NULL) {
    spooky_context_data * data = context->data;

    if(data->fonts != NULL) {
      const spooky_font ** next = global_data.fonts;
      do {
        const spooky_font * font = *next;
        if(font != NULL) { spooky_font_release(font), font = NULL; }
      } while(++next < global_data.fonts + global_data.fonts_len);
    }
    if(data->canvas != NULL) {
      SDL_ClearError();
      SDL_DestroyTexture(data->canvas), data->canvas = NULL;
      if(spooky_is_sdl_error(SDL_GetError())) { fprintf(stderr, "> %s\n", SDL_GetError()); }
      const char * error = SDL_GetError();
      if(spooky_is_sdl_error(error)) {
        fprintf(stderr, "Non-fatal error: Unable to destroy canvas, '%s'.\n", error);
      }
    }

    if(data->glContext != NULL) {
      SDL_ClearError();
      SDL_GL_DeleteContext(data->glContext), data->glContext = NULL;
      if(spooky_is_sdl_error(SDL_GetError())) { fprintf(stderr, "> %s\n", SDL_GetError()); }
      const char * error = SDL_GetError();
      if(spooky_is_sdl_error(error)) {
        fprintf(stderr, "Non-fatal error: Unable to release GL context, '%s'.\n", error);
      }
    }

    if(data->renderer != NULL) {
      SDL_ClearError();
      SDL_DestroyRenderer(data->renderer), data->renderer = NULL;
      if(spooky_is_sdl_error(SDL_GetError())) { fprintf(stderr, "> %s\n", SDL_GetError()); }
      const char * error = SDL_GetError();
      if(spooky_is_sdl_error(error)) {
        fprintf(stderr, "Non-fatal error: Unable to destroy renderer, '%s'.\n", error);
      }
    }

    if(data->window != NULL) {
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
  if(spooky_load_image("res/test0.png", 13, &test_surface) != SP_SUCCESS) { goto err0; }
  if(test_surface == NULL) goto err0;
  SDL_ClearError();
  SDL_FreeSurface(test_surface), test_surface = NULL;
  if(spooky_is_sdl_error(SDL_GetError())) { fprintf(stderr, "> %s\n", SDL_GetError()); }
  
  /* check texture method */
  SDL_Texture * test_texture = NULL;
  if(spooky_load_texture(renderer, "res/test0.png", 13, &test_texture) != SP_SUCCESS) { goto err0; }
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

void spooky_context_scale_font_up(spooky_context * context, bool * is_done) {
  spooky_context_data * data = context->data;
  if(data->fonts_index + 1 > data->fonts + global_data.fonts_len - 1) {
    data->fonts_index = data->fonts + global_data.fonts_len - 1;
  } else {
    data->fonts_index++;
  }

  ptrdiff_t size = data->fonts_index - data->fonts;
  assert(size > 0);

  *is_done = spooky_font_sizes[(size_t)size];

  fprintf(stdout, "Resized to %i\n", (int)(size_t)(ptrdiff_t)(data->fonts_index - data->fonts));
  data->font_current = *data->fonts_index;
}

void spooky_context_scale_font_down(spooky_context * context, bool * is_done) {
  spooky_context_data * data = context->data;
  if(data->fonts_index - 1 < data->fonts) {
    data->fonts_index = data->fonts;
  } else {
    data->fonts_index--;
  }

  ptrdiff_t size = data->fonts_index - data->fonts;
  if(size < 4) {
    data->fonts_index = data->fonts + 4;
    size = 4;
  }
 
  *is_done = spooky_font_sizes[(size_t)size];

  fprintf(stdout, "Resized to %i\n", (int)(size_t)(ptrdiff_t)(data->fonts_index - data->fonts));
  data->font_current = *data->fonts_index;
}

