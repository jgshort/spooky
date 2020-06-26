#ifndef SP_BASE__H
#define SP_BASE__H

#include "sp_gui.h"

typedef struct spooky_base spooky_base;

typedef struct spooky_base {
  const spooky_base * (*ctor)(const spooky_base * self);
  const spooky_base * (*dtor)(const spooky_base * self);
  void (*free)(const spooky_base * self);
  void (*release)(const spooky_base * self);

  void (*handle_event)(const spooky_base * base, SDL_Event * event);
  void (*handle_delta)(const spooky_base * base, double interpolation);
  void (*render)(const spooky_base * base, SDL_Renderer * renderer);
} spooky_base;

const spooky_base * spooky_base_init(spooky_base * self);
const spooky_base * spooky_base_alloc();
const spooky_base * spooky_base_acquire();
const spooky_base * spooky_base_ctor(const spooky_base * self);
const spooky_base * spooky_base_dtor(const spooky_base * self);
void spooky_base_free(const spooky_base * self);
void spooky_base_release(const spooky_base * self);


void spooky_base_handle_event(const spooky_base * base, SDL_Event * event);
void spooky_base_handle_delta(const spooky_base * base, double interpolation);
void spooky_base_render(const spooky_base * base, SDL_Renderer * renderer);

#endif /* SP_BASE__H */

