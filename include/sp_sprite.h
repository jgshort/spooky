#pragma once

#ifndef SPOOKY_SPRITE__H
#define SPOOKY_SPRITE__H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "sp_base.h"

  typedef struct spooky_sprite spooky_sprite;
  typedef struct spooky_sprite_data spooky_sprite_data;

  typedef struct spooky_sprite {
    const spooky_sprite * (*ctor)(const spooky_sprite * /* self */, SDL_Texture * /* texture */);
    const spooky_sprite * (*dtor)(const spooky_sprite * /* self */);
    void (*free)(const spooky_sprite * /* self */);
    void (*release)(const spooky_sprite * /* self */);

    bool (*handle_event)(const spooky_sprite * /* self */, SDL_Event * /* event */);
    void (*handle_delta)(const spooky_sprite * /* self */, const SDL_Event * /* event */, uint64_t /* last_update_time */, double /* interpolation */);
    void (*render)(const spooky_sprite * /* self */, SDL_Renderer * /* renderer */, const SDL_Rect * /* src */, const SDL_Rect * /* dest */);

    void (*set_sheet)(const spooky_sprite * /* self */, int /* sheet */);
    void (*next_sheet)(const spooky_sprite * /* self */);
    void (*prev_sheet)(const spooky_sprite * /* self */);

    bool (*get_is_visible)(const spooky_sprite * /* self */);
    void (*set_is_visible)(const spooky_sprite * /* self */, bool /* is_visible */);
    void (*set_texture)(const spooky_sprite * /* self */, SDL_Texture * /* texture */);

    spooky_sprite_data * data;
  } spooky_sprite;

  const spooky_sprite * spooky_sprite_alloc();
  const spooky_sprite * spooky_sprite_init(spooky_sprite * /* self */);
  const spooky_sprite * spooky_sprite_acquire();
  const spooky_sprite * spooky_sprite_ctor(const spooky_sprite * /* self */, SDL_Texture * /* texture */);
  const spooky_sprite * spooky_sprite_dtor(const spooky_sprite * /* self */);
  void spooky_sprite_free(const spooky_sprite * /* self */);
  void spooky_sprite_release(const spooky_sprite * /* self */);

#ifdef __cplusplus
}
#endif

#endif /* SPOOKY_SPRITE__H */

