#ifndef SPOOKY_GUI__H
#define SPOOKY_GUI__H

#include "sp_error.h"

#ifdef __APPLE__
  /* For some reason, this won't build on OS X/clang without externing memset_pattern4 */
  #include <stdlib.h>
  #include <stdint.h>
  extern void memset_pattern4(void *dst, const void * val, size_t dwords);
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"
#include <SDL2/SDL.h>
#pragma GCC diagnostic pop 

#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

errno_t spooky_load_image(const char * file_path, size_t file_path_len, SDL_Surface ** surface_out);
errno_t spooky_load_texture(SDL_Renderer * renderer, const char * file_path, size_t file_path_len, SDL_Texture ** out_texture);

const int spooky_window_default_width;
const int spooky_window_default_height;

/*
#define WINDOW_SCALE (2)
#define WINDOW_WIDTH (320 * WINDOW_SCALE)
#define WINDOW_HEIGHT (200 * WINDOW_SCALE)

const float spooky_scale_increment;
const float spooky_scale_decrement;

const int spooky_ratcliff_factor;

const int spooky_gui_is_fullscreen;

const int spooky_window_scale;
const int spooky_gui_x_padding;
const int spooky_gui_y_padding;

const int spooky_window_screen_width;
const int spooky_window_screen_height;

void spooky_gui_init_gui(SDL_Window * window, SDL_Renderer * renderer);

float spooky_gui_get_scale_factor();

void spooky_gui_increment_scale_factor();
void spooky_gui_decrement_scale_factor();

void spooky_gui_reset_scale_factor(SDL_Renderer * renderer);

void spooky_gui_render_scaled(SDL_Renderer * renderer, SDL_Texture * texture,  const SDL_Rect * src_rect, const SDL_Rect * dest_rect);
*/

#endif /* SPOOKY_GUI__H */

