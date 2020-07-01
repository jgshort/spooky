#ifndef SP_CONTEXT__H
#define SP_CONTEXT__H

#include <stdbool.h>

#include "sp_error.h"
#include "sp_gui.h"
#include "sp_font.h"

struct spooky_context_data;
typedef struct spooky_context spooky_context;
typedef struct spooky_context {
  SDL_Window * (*get_window)(const spooky_context * context);
  SDL_Renderer * (*get_renderer)(const spooky_context * context);
  SDL_Texture * (*get_canvas)(const spooky_context * context);
  const spooky_font * (*get_font)(const spooky_context * context);

  int (*get_window_width)(const spooky_context * context);
  void (*set_window_width)(const spooky_context * context, int window_width);
  
  int (*get_window_height)(const spooky_context * context);
  void (*set_window_height)(const spooky_context * context, int window_height);
  
  bool (*get_is_fullscreen)(const spooky_context * context);
  void (*set_is_fullscreen)(const spooky_context * context, bool is_fullscreen);

  bool (*get_is_paused)(const spooky_context * context);
  void (*set_is_paused)(const spooky_context * context, bool is_paused);

  struct spooky_context_data * data;
} spooky_context;

errno_t spooky_init_context(spooky_context * context);
errno_t spooky_test_resources(const spooky_context * context);
errno_t spooky_quit_context(spooky_context * context);

void spooky_release_context(spooky_context * context);

void spooky_context_reload_font(spooky_context * context);
void spooky_context_scale_font(spooky_context * context, int new_point_size);

void spooky_context_scale_font_up(spooky_context * context, bool * is_done);
void spooky_context_scale_font_down(spooky_context * context, bool * is_done);

#endif /* SP_CONTEXT__H */

