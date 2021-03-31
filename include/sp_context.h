#ifndef SP_CONTEXT__H
#define SP_CONTEXT__H

#include <stdbool.h>

#include "sp_error.h"
#include "sp_hash.h"
#include "sp_gui.h"
#include "sp_font.h"

struct spooky_context_data;
typedef struct spooky_context spooky_context;
typedef struct spooky_context {
  SDL_Window * (*get_window)(const spooky_context * context);
  SDL_Renderer * (*get_renderer)(const spooky_context * context);
  SDL_Texture * (*get_canvas)(const spooky_context * context);
  void (*set_canvas)(const spooky_context * context, SDL_Texture * texture);

  const spooky_font * (*get_font)(const spooky_context * context);

  const SDL_Rect * (*get_native_rect)(const spooky_context * context);
  void (*set_native_rect)(const spooky_context * context, const SDL_Rect * rect);

  const SDL_Rect * (*get_scaled_rect)(const spooky_context * context);
  void (*set_scaled_rect)(const spooky_context * context, const SDL_Rect * rect);

  void (*get_center_rect)(const spooky_context * context, SDL_Rect * rect);

  bool (*get_is_fullscreen)(const spooky_context * context);
  void (*set_is_fullscreen)(const spooky_context * context, bool is_fullscreen);

  bool (*get_is_paused)(const spooky_context * context);
  void (*set_is_paused)(const spooky_context * context, bool is_paused);

  bool (*get_is_running)(const spooky_context * context);
  void (*set_is_running)(const spooky_context * context, bool value);

  void (*next_font_type)(spooky_context * context);

  const spooky_hash_table * (*get_hash)(const spooky_context * context);
  int (*get_display_index)(const spooky_context * context);

  float (*get_scale_w)(const spooky_context * context);
  void (*set_scale_w)(const spooky_context * context, float w);

  float (*get_scale_h)(const spooky_context * context);
  void (*set_scale_h)(const spooky_context * context, float h);

  struct spooky_context_data * data;
} spooky_context;

errno_t spooky_init_context(spooky_context * context, FILE * fp);
errno_t spooky_test_resources(const spooky_context * context);
errno_t spooky_quit_context(spooky_context * context);

void spooky_release_context(spooky_context * context);

void spooky_context_reload_font(spooky_context * context);
void spooky_context_scale_font(spooky_context * context, int new_point_size);

void spooky_context_scale_font_up(spooky_context * context, bool * is_done);
void spooky_context_scale_font_down(spooky_context * context, bool * is_done);

bool spooky_context_get_is_running(const spooky_context * context);
void spooky_context_set_is_running(const spooky_context * context, bool value);

#endif /* SP_CONTEXT__H */

