#ifndef SP_HUD__H
#define SP_HUD__H

#include "sp_base.h"

struct spooky_hud_data;
typedef struct spooky_hud spooky_hud;
typedef struct spooky_hud {
  spooky_base super;

  const spooky_hud * (*ctor)(const spooky_hud * self, const spooky_font * font);
  const spooky_hud * (*dtor)(const spooky_hud * self);
  void (*free)(const spooky_hud * self);
  void (*release)(const spooky_hud * self);

  struct spooky_hud_data * data;
} spooky_hud;

const spooky_hud * spooky_hud_init(spooky_hud * self);
const spooky_hud * spooky_hud_alloc();
const spooky_hud * spooky_hud_acquire();

const spooky_hud * spooky_hud_ctor(const spooky_hud * self, const spooky_font * font);
const spooky_hud * spooky_hud_dtor(const spooky_hud * self);
void spooky_hud_free(const spooky_hud * self);
void spooky_hud_release(const spooky_hud * self);

void spooky_hud_update(const spooky_hud * self, int64_t fps, int64_t seconds_since_start, double interpolation);

void spooky_hud_handle_event(const spooky_base * self, SDL_Event * event);
void spooky_hud_handle_delta(const spooky_base * self, double interpolation);
void spooky_hud_render(const spooky_base * self, SDL_Renderer * renderer);
 
#endif

