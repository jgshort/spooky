#ifndef SP_GUI__H
#define SP_GUI__H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

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

  errno_t sp_gui_load_image(const char * /* file_path */, size_t /* file_path_len */, SDL_Surface ** /* surface_out */);
  errno_t sp_gui_load_texture(SDL_Renderer * /* renderer */, const char * /* file_path */, size_t /* file_path_len */, SDL_Texture ** /* out_texture */);

  extern const float sp_gui_canvas_scale_factor;
  extern const bool sp_gui_is_fullscreen;
  extern const uint32_t sp_gui_window_flags;
  extern const uint32_t sp_gui_renderer_flags;
  extern const float sp_gui_default_aspect_ratio;
  extern const int sp_gui_window_default_logical_width;
  extern const int sp_gui_window_default_logical_height;
  extern const int sp_gui_ratcliff_factor;

  float get_ui_scale_factor();

  typedef struct sp_gui_rgba_context sp_gui_rgba_context;

  const sp_gui_rgba_context * sp_gui_push_draw_color(SDL_Renderer * /* renderer */, const SDL_Color * /* new_color */);
  void sp_gui_pop_draw_color(const sp_gui_rgba_context * /* context */);

  typedef enum sp_view_perspective {
    SP_SVP_DEFAULT = 0,
    SP_SVP_Z = SP_SVP_DEFAULT,
    SP_SVP_X,
    SP_SVP_Y,
    SP_SVP_EOE
  } sp_view_perspective;

  void sp_gui_color_lighten(SDL_Color * /* color */, float /* luminosity */);
  void sp_gui_color_darken(SDL_Color * /* color */, int /* percent */);

#ifdef __cplusplus
}
#endif

#endif /* SP_GUI__H */

