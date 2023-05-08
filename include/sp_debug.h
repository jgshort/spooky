#ifndef SP_DEBUG__H
#define SP_DEBUG__H

#ifdef __cplusplus
extern "C" {
#endif

#include "sp_base.h"
#include "sp_context.h"

  struct sp_debug_data;
  typedef struct sp_debug sp_debug;
  typedef struct sp_debug {
    sp_base super;

    const sp_base * (*as_base)(const sp_debug * self);
    const sp_debug * (*ctor)(const sp_debug * self, const char * name, const sp_context * context);
    const sp_debug * (*dtor)(const sp_debug * self);
    void (*free)(const sp_debug * self);
    void (*release)(const sp_debug * self);

    struct sp_debug_data * data;
  } sp_debug;

  const sp_base * sp_debug_as_base(const sp_debug * self);
  const sp_debug * sp_debug_init(sp_debug * self);
  const sp_debug * sp_debug_alloc(void);
  const sp_debug * sp_debug_acquire(void);

  const sp_debug * sp_debug_ctor(const sp_debug * self, const char * name, const sp_context * context);
  const sp_debug * sp_debug_dtor(const sp_debug * self);
  void sp_debug_free(const sp_debug * self);
  void sp_debug_release(const sp_debug * self);

  void sp_debug_update(const sp_debug * self, int64_t fps, int64_t seconds_since_start, double interpolation);

  bool sp_debug_handle_event(const sp_base * self, SDL_Event * event);
  void sp_debug_handle_delta(const sp_base * self, const SDL_Event * event, double interpolation);
  void sp_debug_render(const sp_base * self, SDL_Renderer * renderer);

#ifdef __cplusplus
}
#endif

#endif

