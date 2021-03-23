#include <assert.h>
#include "sp_gui.h"

const int spooky_ratcliff_factor = 7;

const int spooky_window_default_width = 320;
const int spooky_window_default_height = 200;

const int spooky_window_default_logical_width = 320;
const int spooky_window_default_logical_height = 200;

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
