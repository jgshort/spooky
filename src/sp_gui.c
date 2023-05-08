#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include "../include/sp_gui.h"
#include "../include/sp_math.h"

const bool sp_gui_is_fullscreen = false;
const float sp_gui_canvas_scale_factor = 1.2f;

const uint32_t sp_gui_window_flags
  = sp_gui_is_fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0
  | SDL_WINDOW_OPENGL
  | SDL_WINDOW_HIDDEN
  | SDL_WINDOW_ALLOW_HIGHDPI
  ;
const uint32_t sp_gui_renderer_flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;

const int sp_gui_ratcliff_factor = 7;

const int sp_gui_window_default_logical_width = 1024;
const int sp_gui_window_default_logical_height = 768;

const float sp_gui_default_aspect_ratio = (float)sp_gui_window_default_logical_height / (float)sp_gui_window_default_logical_width;

typedef struct sp_gui_rgba_context {
  SDL_Renderer * renderer;
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
  char padding[4];
} sp_gui_rgba_context;

#define sp_GUI_MAX_DRAW_CONTEXTS 64
static sp_gui_rgba_context sp_gui_draw_contexts[sp_GUI_MAX_DRAW_CONTEXTS];
static size_t sp_gui_next_draw_context = 0;
static const sp_gui_rgba_context * sp_gui_last_draw_context = sp_gui_draw_contexts + sp_GUI_MAX_DRAW_CONTEXTS;

errno_t sp_gui_load_image(const char * file_path, size_t file_path_len, SDL_Surface ** out_surface) {
  assert(!(file_path == NULL || file_path_len <= 0));

  if(file_path == NULL || file_path_len <= 0) { goto err0; }

  SDL_ClearError();
  SDL_Surface * surface = IMG_Load(file_path);
  if(!surface) { goto err1; }

  assert(surface != NULL);
  *out_surface = surface;

  return SP_SUCCESS;

err1:
  {
    const char * error = SDL_GetError();
    fprintf(stderr, "> %s\n", error);
  }
err0:
  {
    *out_surface = NULL;
    fprintf(stderr, "Could not load surface from path '%s'.\n", file_path);
    fprintf(stderr, "This is a fatal error. Check the resources path and restart.\n");
    abort();
  }
}

errno_t sp_gui_load_texture(SDL_Renderer * renderer, const char * file_path, size_t file_path_len, SDL_Texture ** out_texture) {
  assert(!(*out_texture != NULL || renderer == NULL || file_path == NULL || file_path_len <= 0));

  if(*out_texture != NULL || renderer == NULL || file_path == NULL || file_path_len <= 0) { goto err0; }

  SDL_Surface * surface = NULL;
  errno_t surface_error = sp_gui_load_image(file_path, file_path_len, &surface);
  if(surface_error != SP_SUCCESS || surface == NULL) { goto err1; }

  SDL_ClearError();
  SDL_Texture * texture = SDL_CreateTextureFromSurface(renderer, surface);
  if(texture == NULL) { goto err2; }

  SDL_FreeSurface(surface), surface = NULL;

  *out_texture = texture;

  return SP_SUCCESS;

err2:
  {
    const char * error = SDL_GetError();
    if(surface != NULL) {
      SDL_FreeSurface(surface), surface = NULL;
    }
    fprintf(stderr, "> %s\n", error);
  }

err1:
  {
    fprintf(stderr, "Could not load texture from surface from path '%s'.\n", file_path);
  }
err0:
  {
    *out_texture = NULL;
    fprintf(stderr, "This is a fatal error. Check the resources path and restart.\n");
    abort();
  }
}

float get_ui_scale_factor(void) { return 1.f; }

const sp_gui_rgba_context * sp_gui_push_draw_color(SDL_Renderer * renderer, const SDL_Color * new_color) {
  if(sp_gui_next_draw_context + 1 >= sp_GUI_MAX_DRAW_CONTEXTS) {
    /* abort? */
    return NULL;
  }

  sp_gui_rgba_context * context = &sp_gui_draw_contexts[sp_gui_next_draw_context];
  assert(context < sp_gui_last_draw_context);

  sp_gui_next_draw_context++;
  SDL_GetRenderDrawColor(renderer, &context->r, &context->g, &context->b, &context->a);
  if(new_color) {
    SDL_SetRenderDrawColor(renderer, new_color->r, new_color->g, new_color->b, new_color->a);
  }
  context->renderer = renderer;
  return context;
}

void sp_gui_pop_draw_color(const sp_gui_rgba_context * context) {
  if(!context) {
    return;
  }

  assert(context->renderer);

  SDL_SetRenderDrawColor(context->renderer, context->a, context->g, context->b, context->a);

  sp_gui_next_draw_context--;
  if(sp_gui_next_draw_context < 1) { sp_gui_next_draw_context = 0; }
}

void sp_gui_color_lighten(SDL_Color * color, float luminosity) {
  color->r = (uint8_t)(sp_int_min(sp_int_max(0, (int)((float)color->r + (luminosity * 255.f))), 255));
  color->g = (uint8_t)(sp_int_min(sp_int_max(0, (int)((float)color->g + (luminosity * 255.f))), 255));
  color->b = (uint8_t)(sp_int_min(sp_int_max(0, (int)((float)color->b + (luminosity * 255.f))), 255));
}

void sp_gui_color_darken(SDL_Color * color, int percent) {
  color->r = (uint8_t)(color->r - (int)((color->r * percent) / 100));
  color->g = (uint8_t)(color->g - (int)((color->g * percent) / 100));
  color->b = (uint8_t)(color->b - (int)((color->b * percent) / 100));
}

