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

  struct sp_context_data;
  typedef struct sp_context sp_context;
  typedef struct sp_context {
    const sp_config * (*get_config)(const sp_context * context);
    SDL_Window * (*get_window)(const sp_context * context);
    SDL_Renderer * (*get_renderer)(const sp_context * context);
    SDL_Texture * (*get_canvas)(const sp_context * context);
    void (*set_canvas)(const sp_context * context, SDL_Texture * texture);

    const sp_font * (*get_font)(const sp_context * context);

    float (*get_renderer_to_window_scale_factor)(const sp_context * context);

    const SDL_Rect * (*get_scaled_rect)(const sp_context * context);
    void (*set_scaled_rect)(const sp_context * context, const SDL_Rect * rect);

    void (*get_center_rect)(const sp_context * context, SDL_Rect * rect);

    bool (*get_is_fullscreen)(const sp_context * context);
    void (*set_is_fullscreen)(const sp_context * context, bool is_fullscreen);

    bool (*get_is_paused)(const sp_context * context);
    void (*set_is_paused)(const sp_context * context, bool is_paused);

    bool (*get_is_running)(const sp_context * context);
    void (*set_is_running)(const sp_context * context, bool value);

    const sp_hash_table * (*get_hash)(const sp_context * context);
    int (*get_display_index)(const sp_context * context);

    float (*get_scale_w)(const sp_context * context);
    void (*set_scale_w)(const sp_context * context, float w);

    float (*get_scale_h)(const sp_context * context);
    void (*set_scale_h)(const sp_context * context, float h);

    void (*get_translated_mouse_state)(const sp_context * context, uint32_t * state, int * x, int * y);
    void (*translate_point)(const sp_context * context, SDL_Point * point);
    void (*translate_rect)(const sp_context * context, SDL_Rect * rect);

    void (*scale_font_up)(void);
    void (*scale_font_down)(void);

    const sp_base * (*get_modal)(const sp_context * context);
    void (*set_modal)(const sp_context * context, const sp_base * modal);

    int (*get_current_score)(const sp_context * context);
    int (*get_max_score)(const sp_context * context);

    struct sp_context_data * data;
  } sp_context;

  errno_t sp_init_context(sp_context * context, FILE * fp);
  errno_t sp_test_resources(const sp_context * context);
  errno_t sp_quit_context(sp_context * context);

  void sp_release_context(sp_context * context);

  bool sp_context_get_is_running(const sp_context * context);
  void sp_context_set_is_running(const sp_context * context, bool value);

#ifdef __cplusplus
}
#endif

#endif /* SP_CONTEXT__H */

