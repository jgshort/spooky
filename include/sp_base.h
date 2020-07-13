#ifndef SP_BASE__H
#define SP_BASE__H

#include <stdbool.h>
#include "sp_iter.h"
#include "sp_gui.h"

struct spooky_base_impl;
typedef struct spooky_base spooky_base;
typedef struct spooky_base {
  const spooky_base * (*ctor)(const spooky_base * /* self */, SDL_Rect /* origin */);
  const spooky_base * (*dtor)(const spooky_base * /* self */);
  void (*free)(const spooky_base * /* self */);
  void (*release)(const spooky_base * /* self */);
  
  bool (*handle_event)(const spooky_base * /* self */, SDL_Event * /* event */);
  void (*handle_delta)(const spooky_base * /* self */, int64_t /* last_update_time */, double /* interpolation */);
  void (*render)(const spooky_base * /* self */, SDL_Renderer * /* renderer */);

  const SDL_Rect * (*get_rect)(const spooky_base * /* self */);
  void (*set_rect)(const spooky_base * /* self */, const SDL_Rect * /* rect */);

  void (*set_x)(const spooky_base * /* self */, int /* x */);
  void (*set_y)(const spooky_base * /* self */, int /* y */);
  void (*set_w)(const spooky_base * /* self */, int /* w */);
  void (*set_h)(const spooky_base * /* self */, int /* h */);

  void (*update_rect_relative)(const spooky_base * /* self */, const SDL_Rect * /* rect */);

  void (*add_child)(const spooky_base * /* self */, const spooky_base * /* child */);
  const spooky_iter * (*children_iter)(const spooky_base * /* self */);
  void (*set_z_order)(const spooky_base * base, size_t z_order);
  size_t (*get_z_order)(const spooky_base * base);

  struct spooky_base_impl * impl;
} spooky_base;

const spooky_base * spooky_base_init(spooky_base * /* self */);
const spooky_base * spooky_base_alloc();
const spooky_base * spooky_base_acquire();
const spooky_base * spooky_base_ctor(const spooky_base * /* self */, SDL_Rect /* origin */);
const spooky_base * spooky_base_dtor(const spooky_base * /* self */);
void spooky_base_free(const spooky_base * /* self */);
void spooky_base_release(const spooky_base * /* self */);

void spooky_base_z_sort(const spooky_base ** /* items */, size_t /* items_count */);

#endif /* SP_BASE__H */

