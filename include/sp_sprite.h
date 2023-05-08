#pragma once

#ifndef SP_SPRITE__H
#define SP_SPRITE__H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "sp_base.h"

  typedef struct sp_sprite sp_sprite;
  typedef struct sp_sprite_data sp_sprite_data;

  typedef struct sp_sprite {
    const sp_sprite * (*ctor)(const sp_sprite * /* self */, SDL_Texture * /* texture */);
    const sp_sprite * (*dtor)(const sp_sprite * /* self */);
    void (*free)(const sp_sprite * /* self */);
    void (*release)(const sp_sprite * /* self */);

    bool (*handle_event)(const sp_sprite * /* self */, SDL_Event * /* event */);
    void (*handle_delta)(const sp_sprite * /* self */, const SDL_Event * /* event */, uint64_t /* last_update_time */, double /* interpolation */);
    void (*render)(const sp_sprite * /* self */, SDL_Renderer * /* renderer */, const SDL_Rect * /* src */, const SDL_Rect * /* dest */);

    void (*set_sheet)(const sp_sprite * /* self */, int /* sheet */);
    void (*next_sheet)(const sp_sprite * /* self */);
    void (*prev_sheet)(const sp_sprite * /* self */);

    bool (*get_is_visible)(const sp_sprite * /* self */);
    void (*set_is_visible)(const sp_sprite * /* self */, bool /* is_visible */);
    void (*set_texture)(const sp_sprite * /* self */, SDL_Texture * /* texture */);

    sp_sprite_data * data;
  } sp_sprite;

  const sp_sprite * sp_sprite_alloc();
  const sp_sprite * sp_sprite_init(sp_sprite * /* self */);
  const sp_sprite * sp_sprite_acquire();
  const sp_sprite * sp_sprite_ctor(const sp_sprite * /* self */, SDL_Texture * /* texture */);
  const sp_sprite * sp_sprite_dtor(const sp_sprite * /* self */);
  void sp_sprite_free(const sp_sprite * /* self */);
  void sp_sprite_release(const sp_sprite * /* self */);

#ifdef __cplusplus
}
#endif

#endif /* SP_SPRITE__H */

