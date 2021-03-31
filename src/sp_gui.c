#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include "sp_gui.h"

const bool spooky_gui_is_fullscreen = false;
const float spooky_gui_canvas_scale_factor = 1.2f;

const uint32_t spooky_gui_window_flags =
  spooky_gui_is_fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0
    | SDL_WINDOW_OPENGL
    | SDL_WINDOW_HIDDEN
    | SDL_WINDOW_ALLOW_HIGHDPI
    | SDL_WINDOW_RESIZABLE
    ;
const uint32_t spooky_gui_renderer_flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;

const int spooky_gui_ratcliff_factor = 7;

/* 4K UHD-1  3840 × 2160 */
const int spooky_gui_window_default_width = 640 * 2;
const int spooky_gui_window_default_height = 480 * 2;
const int spooky_gui_window_min_width = 640 * 2;
const int spooky_gui_window_min_height = 480 * 2;

const int spooky_gui_window_default_logical_width = 640 * 2;
const int spooky_gui_window_default_logical_height = 480 * 2;

const float spooky_gui_default_aspect_ratio = (float)spooky_gui_window_default_logical_height / (float)spooky_gui_window_default_logical_width;

typedef struct spooky_gui_rgba_context {
  SDL_Renderer * renderer;
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
  char padding[4];
} spooky_gui_rgba_context;

#define SPOOKY_GUI_MAX_DRAW_CONTEXTS 64
static spooky_gui_rgba_context spooky_gui_draw_contexts[SPOOKY_GUI_MAX_DRAW_CONTEXTS];
static size_t spooky_gui_next_draw_context = 0;
static const spooky_gui_rgba_context * spooky_gui_last_draw_context = spooky_gui_draw_contexts + SPOOKY_GUI_MAX_DRAW_CONTEXTS;

errno_t spooky_load_image(const char * file_path, size_t file_path_len, SDL_Surface ** out_surface) {
  assert(!(file_path == NULL || file_path_len <= 0));

  if(file_path == NULL || file_path_len <= 0) { goto err0; }

  SDL_ClearError();
  SDL_Surface * surface = IMG_Load(file_path);
  if(!surface) { goto err1; }

  assert(surface != NULL);
  *out_surface = surface;

  return SP_SUCCESS;

err1: ;
  const char * error = SDL_GetError();
  fprintf(stderr, "> %s\n", error);

err0:
  *out_surface = NULL;
  fprintf(stderr, "Could not load surface from path '%s'.\n", file_path);
  fprintf(stderr, "This is a fatal error. Check the resources path and restart.\n");
  abort();
}

errno_t spooky_load_texture(SDL_Renderer * renderer, const char * file_path, size_t file_path_len, SDL_Texture ** out_texture) {
  assert(!(*out_texture != NULL || renderer == NULL || file_path == NULL || file_path_len <= 0));

  if(*out_texture != NULL || renderer == NULL || file_path == NULL || file_path_len <= 0) { goto err0; }


  SDL_Surface * surface = NULL;
  errno_t surface_error = spooky_load_image(file_path, file_path_len, &surface);
  if(surface_error != SP_SUCCESS || surface == NULL) { goto err1; }

  SDL_ClearError();
  SDL_Texture * texture = SDL_CreateTextureFromSurface(renderer, surface);
  if(texture == NULL) { goto err2; }

  SDL_FreeSurface(surface), surface = NULL;

  *out_texture = texture;

  return SP_SUCCESS;

err2: ;
  const char * error = SDL_GetError();
  if(surface != NULL) {
    SDL_FreeSurface(surface), surface = NULL;
  }
  fprintf(stderr, "> %s\n", error);

err1:
  fprintf(stderr, "Could not load texture from surface from path '%s'.\n", file_path);

err0:
  *out_texture = NULL;
  fprintf(stderr, "This is a fatal error. Check the resources path and restart.\n");
  abort();
}

float get_ui_scale_factor() { return 1.f; }

const spooky_gui_rgba_context * spooky_gui_push_draw_color(SDL_Renderer * renderer) {
  if(spooky_gui_next_draw_context + 1 >= SPOOKY_GUI_MAX_DRAW_CONTEXTS) {
    /* abort? */
    return NULL;
  }

  spooky_gui_rgba_context * context = &spooky_gui_draw_contexts[spooky_gui_next_draw_context];
  assert(context < spooky_gui_last_draw_context);

  spooky_gui_next_draw_context++;
  SDL_GetRenderDrawColor(renderer, &context->r, &context->g, &context->b, &context->a);
  context->renderer = renderer;
  return context;
}

void spooky_gui_pop_draw_color(const spooky_gui_rgba_context * context) {
  if(!context) {
    return;
  }

  assert(context->renderer);

  SDL_SetRenderDrawColor(context->renderer, context->a, context->g, context->b, context->a);

  spooky_gui_next_draw_context--;
  if(spooky_gui_next_draw_context < 1) { spooky_gui_next_draw_context = 0; }
}
