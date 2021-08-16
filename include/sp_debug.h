#ifndef SP_DEBUG__H
#define SP_DEBUG__H

#include "sp_base.h"
#include "sp_context.h"

struct spooky_debug_data;
typedef struct spooky_debug spooky_debug;
typedef struct spooky_debug {
  spooky_base super;

  const spooky_base * (*as_base)(const spooky_debug * self);
  const spooky_debug * (*ctor)(const spooky_debug * self, const spooky_context * context);
  const spooky_debug * (*dtor)(const spooky_debug * self);
  void (*free)(const spooky_debug * self);
  void (*release)(const spooky_debug * self);

  struct spooky_debug_data * data;
} spooky_debug;

const spooky_base * spooky_debug_as_base(const spooky_debug * self);
const spooky_debug * spooky_debug_init(spooky_debug * self);
const spooky_debug * spooky_debug_alloc();
const spooky_debug * spooky_debug_acquire();

const spooky_debug * spooky_debug_ctor(const spooky_debug * self, const spooky_context * context);
const spooky_debug * spooky_debug_dtor(const spooky_debug * self);
void spooky_debug_free(const spooky_debug * self);
void spooky_debug_release(const spooky_debug * self);

void spooky_debug_update(const spooky_debug * self, int64_t fps, int64_t seconds_since_start, double interpolation);

bool spooky_debug_handle_event(const spooky_base * self, SDL_Event * event);
void spooky_debug_handle_delta(const spooky_base * self, const SDL_Event * event, double interpolation);
void spooky_debug_render(const spooky_base * self, SDL_Renderer * renderer);

#endif

