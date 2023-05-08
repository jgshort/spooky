#ifndef SP_CONTEXT__H
#define SP_CONTEXT__H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "sp_base.h"
#include "sp_config.h"
#include "sp_error.h"
#include "sp_hash.h"
#include "sp_gui.h"
#include "sp_font.h"

  struct spooky_context_data;
  typedef struct spooky_context spooky_context;
  typedef struct spooky_context {
    const spooky_config * (*get_config)(const spooky_context * context);
    SDL_Window * (*get_window)(const spooky_context * context);
    SDL_Renderer * (*get_renderer)(const spooky_context * context);
    SDL_Texture * (*get_canvas)(const spooky_context * context);
    void (*set_canvas)(const spooky_context * context, SDL_Texture * texture);

    const spooky_font * (*get_font)(const spooky_context * context);

    float (*get_renderer_to_window_scale_factor)(const spooky_context * context);

    const SDL_Rect * (*get_scaled_rect)(const spooky_context * context);
    void (*set_scaled_rect)(const spooky_context * context, const SDL_Rect * rect);

    void (*get_center_rect)(const spooky_context * context, SDL_Rect * rect);

    bool (*get_is_fullscreen)(const spooky_context * context);
    void (*set_is_fullscreen)(const spooky_context * context, bool is_fullscreen);

    bool (*get_is_paused)(const spooky_context * context);
    void (*set_is_paused)(const spooky_context * context, bool is_paused);

    bool (*get_is_running)(const spooky_context * context);
    void (*set_is_running)(const spooky_context * context, bool value);

    const spooky_hash_table * (*get_hash)(const spooky_context * context);
    int (*get_display_index)(const spooky_context * context);

    float (*get_scale_w)(const spooky_context * context);
    void (*set_scale_w)(const spooky_context * context, float w);

    float (*get_scale_h)(const spooky_context * context);
    void (*set_scale_h)(const spooky_context * context, float h);

    void (*get_translated_mouse_state)(const spooky_context * context, uint32_t * state, int * x, int * y);
    void (*translate_point)(const spooky_context * context, SDL_Point * point);
    void (*translate_rect)(const spooky_context * context, SDL_Rect * rect);

    void (*scale_font_up)(void);
    void (*scale_font_down)(void);

    const spooky_base * (*get_modal)(const spooky_context * context);
    void (*set_modal)(const spooky_context * context, const spooky_base * modal);

    int (*get_current_score)(const spooky_context * context);
    int (*get_max_score)(const spooky_context * context);

    struct spooky_context_data * data;
  } spooky_context;

  errno_t spooky_init_context(spooky_context * context, FILE * fp);
  errno_t spooky_test_resources(const spooky_context * context);
  errno_t spooky_quit_context(spooky_context * context);

  void spooky_release_context(spooky_context * context);

  bool spooky_context_get_is_running(const spooky_context * context);
  void spooky_context_set_is_running(const spooky_context * context, bool value);

#ifdef __cplusplus
}
#endif

#endif /* SP_CONTEXT__H */

