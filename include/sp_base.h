#ifndef SP_BASE__H
#define SP_BASE__H

#include "sp_gui.h"

typedef struct spooky_base spooky_base;

typedef struct spooky_base {
  const spooky_base * (*ctor)(const spooky_base * self);
  const spooky_base * (*dtor)(const spooky_base * self);
  void (*free)(const spooky_base * self);
  void (*release)(const spooky_base * self);

  void (*set_z_order)(const spooky_base * base, int z_order);
  int (*get_z_order)(const spooky_base * base);

  void (*handle_event)(const spooky_base * base, SDL_Event * event);
  void (*handle_delta)(const spooky_base * base, int64_t last_update_time, double interpolation);
  void (*render)(const spooky_base * base, SDL_Renderer * renderer);

  int z_order;
  char padding[4]; /* not portable */
} spooky_base;

const spooky_base * spooky_base_init(spooky_base * self);
const spooky_base * spooky_base_alloc();
const spooky_base * spooky_base_acquire();
const spooky_base * spooky_base_ctor(const spooky_base * self);
const spooky_base * spooky_base_dtor(const spooky_base * self);
void spooky_base_free(const spooky_base * self);
void spooky_base_release(const spooky_base * self);

void spooky_base_handle_event(const spooky_base * base, SDL_Event * event);
void spooky_base_handle_delta(const spooky_base * base, int64_t last_update_time, double interpolation);
void spooky_base_render(const spooky_base * base, SDL_Renderer * renderer);

void spooky_base_z_sort(const spooky_base ** items, size_t items_count);

#endif /* SP_BASE__H */

