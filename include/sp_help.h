#ifndef SP_HELP__H
#define SP_HELP__H

#ifdef __cplusplus
extern "C" {
#endif

#include "sp_base.h"
#include "sp_context.h"

  struct sp_help_impl;
  typedef struct sp_help sp_help;
  typedef struct sp_help {
    sp_base super;

    const sp_base * (*as_base)(const sp_help * self);
    const sp_help * (*ctor)(const sp_help * self, const char * name, const sp_context * context);
    const sp_help * (*dtor)(const sp_help * self);
    void (*free)(const sp_help * self);
    void (*release)(const sp_help * self);

    struct sp_help_impl * impl;
  } sp_help;

  const sp_base * sp_help_as_base(const sp_help * self);
  const sp_help * sp_help_init(sp_help * self);
  const sp_help * sp_help_alloc(void);
  const sp_help * sp_help_acquire(void);

  const sp_help * sp_help_ctor(const sp_help * self, const char * name, const sp_context * context);
  const sp_help * sp_help_dtor(const sp_help * self);
  void sp_help_free(const sp_help * self);
  void sp_help_release(const sp_help * self);

  bool sp_help_handle_event(const sp_base * self, SDL_Event * event);
  void sp_help_handle_delta(const sp_base * self, const SDL_Event * event, uint64_t last_update_time, double interpolation);
  void sp_help_render(const sp_base * self, SDL_Renderer * renderer);

#ifdef __cplusplus
}
#endif

#endif

