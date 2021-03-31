#ifndef SPOOKY_GUI__H
#define SPOOKY_GUI__H

#ifdef __cplusplus
extern "C" {
#endif

#include "sp_error.h"

#ifdef __APPLE__
  /* For some reason, this won't build on OS X/clang without externing memset_pattern4 */
  #include <stdlib.h>
  extern void memset_pattern4(void * /* dst */, const void * /* val */, size_t /* dwords */);
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"
#include <SDL2/SDL.h>
#pragma GCC diagnostic pop

#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

errno_t spooky_load_image(const char * /* file_path */, size_t /* file_path_len */, SDL_Surface ** /* surface_out */);
errno_t spooky_load_texture(SDL_Renderer * /* renderer */, const char * /* file_path */, size_t /* file_path_len */, SDL_Texture ** /* out_texture */);

extern const int spooky_window_default_width;
extern const int spooky_window_default_height;
extern const int spooky_window_default_logical_width;
extern const int spooky_window_default_logical_height;
extern const int spooky_ratcliff_factor;

float get_ui_scale_factor();

typedef struct spooky_gui_rgba_context spooky_gui_rgba_context;

const spooky_gui_rgba_context * spooky_gui_push_draw_color(SDL_Renderer * /* renderer */);
void spooky_gui_pop_draw_color(const spooky_gui_rgba_context * /* context */);

#ifdef __cplusplus
}
#endif

#endif /* SPOOKY_GUI__H */

