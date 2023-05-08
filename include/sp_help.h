#ifndef SP_HELP__H
#define SP_HELP__H

#ifdef __cplusplus
extern "C" {
#endif

#include "sp_base.h"
#include "sp_context.h"

  struct spooky_help_impl;
  typedef struct spooky_help spooky_help;
  typedef struct spooky_help {
    spooky_base super;

    const spooky_base * (*as_base)(const spooky_help * self);
    const spooky_help * (*ctor)(const spooky_help * self, const char * name, const spooky_context * context);
    const spooky_help * (*dtor)(const spooky_help * self);
    void (*free)(const spooky_help * self);
    void (*release)(const spooky_help * self);

    struct spooky_help_impl * impl;
  } spooky_help;

  const spooky_base * spooky_help_as_base(const spooky_help * self);
  const spooky_help * spooky_help_init(spooky_help * self);
  const spooky_help * spooky_help_alloc(void);
  const spooky_help * spooky_help_acquire(void);

  const spooky_help * spooky_help_ctor(const spooky_help * self, const char * name, const spooky_context * context);
  const spooky_help * spooky_help_dtor(const spooky_help * self);
  void spooky_help_free(const spooky_help * self);
  void spooky_help_release(const spooky_help * self);

  bool spooky_help_handle_event(const spooky_base * self, SDL_Event * event);
  void spooky_help_handle_delta(const spooky_base * self, const SDL_Event * event, uint64_t last_update_time, double interpolation);
  void spooky_help_render(const spooky_base * self, SDL_Renderer * renderer);

#ifdef __cplusplus
}
#endif

#endif

