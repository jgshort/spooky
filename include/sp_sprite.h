#pragma once

#ifndef SPOOKY_SPRITE__H
#define SPOOKY_SPRITE__H

#include "sp_base.h"

typedef struct spooky_sprite spooky_sprite;
typedef struct spooky_sprite_data spooky_sprite_data;

typedef struct spooky_sprite {
  spooky_base super;

  const spooky_sprite * (*ctor)(const spooky_sprite * /* self */, SDL_Texture * /* texture */);
  const spooky_sprite * (*dtor)(const spooky_sprite * /* self */);
  void (*free)(const spooky_sprite * /* self */);
  void (*release)(const spooky_sprite * /* self */);
  const spooky_base * (*as_base)(const spooky_sprite * self);

  spooky_sprite_data * data;
} spooky_sprite;

const spooky_base * spooky_sprite_as_base(const spooky_sprite * /* self */);
const spooky_sprite * spooky_sprite_alloc();
const spooky_sprite * spooky_sprite_init(spooky_sprite * /* self */);
const spooky_sprite * spooky_sprite_acquire();
const spooky_sprite * spooky_sprite_ctor(const spooky_sprite * /* self */, SDL_Texture * /* texture */);
const spooky_sprite * spooky_sprite_dtor(const spooky_sprite * /* self */);
void spooky_sprite_free(const spooky_sprite * /* self */);
void spooky_sprite_release(const spooky_sprite * /* self */);

#endif /* SPOOKY_SPRITE__H */
