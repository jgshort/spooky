#ifndef SPOOKY_BOX__H
#define SPOOKY_BOX__H

#include "sp_context.h"
#include "sp_sprite.h"

typedef struct spooky_box spooky_box;
typedef struct spooky_box_data spooky_box_data;

typedef struct spooky_box {
  spooky_base super;
  const spooky_box * (*ctor)(const spooky_box * /* self */, const spooky_context * /* context */, SDL_Rect /* origin */);
  const spooky_box * (*dtor)(const spooky_box * /* self */);
  void (*free)(const spooky_box * /* self */);
  void (*release)(const spooky_box * /* self */);
  const spooky_base * (*as_base)(const spooky_box * /* self */);

  void (*set_name)(const spooky_box * /* self */, const char * /* name */);
  const char * (*get_name)(const spooky_box * /* self */);

  void (*set_sprite)(const spooky_box * /* self */, const spooky_sprite * /* sprite */);
  const spooky_sprite * (*get_sprite)(const spooky_box * /* self */);

  spooky_box_data * data;
} spooky_box;

const spooky_base * spooky_box_as_base(const spooky_box * /* self */);

/* Allocate (malloc) interface */
const spooky_box * spooky_box_alloc();
/* Initialize interface methods */
const spooky_box * spooky_box_init(spooky_box * /* self */);
/* Allocate and initialize interface methods */
const spooky_box * spooky_box_acquire();
/* Construct data */
const spooky_box * spooky_box_ctor(const spooky_box * /* self */, const spooky_context * /* context */, SDL_Rect /* origin */);
/* Destruct (dtor) data */
const spooky_box * spooky_box_dtor(const spooky_box * /* self */);
/* Free interface */
void spooky_box_free(const spooky_box * /* self */);
/* Destruct and free interface */
void spooky_box_release(const spooky_box * /* self */);


#if 1 == 2

#include "sp_base.h"
#include "sp_types.h"
#include "sp_gui.h"
#include "sp_wm.h"

const int scroll_bar_width;

typedef struct spooky_box spooky_box;
typedef struct spooky_box_data spooky_box_data;
typedef struct spooky_box_scroll_box_item spooky_box_scroll_box_item;

typedef enum spooky_box_types {
  SBT_NULL,
  SBT_WINDOW,
  SBT_WINDOW_TEXTURED,
  SBT_BUTTON,
  SBT_SCROLL_BOX,
  SBT_IMAGE,
  SBT_TEXT,
  SBT_MAIN_MENU,
  SBT_MAIN_MENU_ITEM,
  SBT_EOE
} spooky_box_types;

typedef enum spooky_box_scroll_box_direction {
  SBSBD_VERTICAL = 0,
  SBSBD_HORIZONTAL = 1,
  SBSBD_EOE,
} spooky_box_scroll_box_direction;

typedef struct spooky_box {
  spooky_base super;
  const spooky_base * (*as_base)(const spooky_box * self);
  const spooky_box * (*ctor)(const spooky_box * self, const spooky_context * context, const spooky_wm * wm, const spooky_box * parent, SDL_Window * window, SDL_Renderer * renderer, const char * name, SDL_Rect rect, bool is_visible, spooky_box_types box_type);
  const spooky_box * (*dtor)(const spooky_box * self);
  void (*free)(const spooky_box * self);
  void (*release)(const spooky_box * self);

  void (*set_z_order)(const spooky_box * self, int z_order);
  int (*get_z_order)(const spooky_box * self);

  const char * (*get_name)(const spooky_box * self);
  const SDL_Rect * (*get_rect)(const spooky_box * self);
  const spooky_box * (*attach_box)(const spooky_box * self, const spooky_box * box);
  //const spooky_box * (*attach_select_box_item)(const spooky_box * self, const char * text);
  spooky_box_types (*get_box_type)(const spooky_box * self);

  void (*set_x)(const spooky_box * self, int x);
  int (*get_x)(const spooky_box * self);
  void (*set_y)(const spooky_box * self, int y);
  int (*get_y)(const spooky_box * self);

  void (*set_w)(const spooky_box * self, int w);
  int (*get_w)(const spooky_box * self);

  void (*set_h)(const spooky_box * self, int h);
  int (*get_h)(const spooky_box * self);

  const spooky_box_scroll_box_item * (*attach_scroll_box_item_text)(const spooky_box * self, const char * text, const char * description);
  const spooky_box_scroll_box_item * (*attach_scroll_box_item_image)(const spooky_box * self, const char * text, const char * description, const char * path);
  void (*set_direction)(const spooky_box * self, spooky_box_scroll_box_direction direction);

  spooky_box_data * data;
} spooky_box_interface;

const spooky_base * spooky_box_as_base(const spooky_box * self);

const spooky_box_interface spooky_box_class;

/* Allocate (malloc) interface */
const spooky_box * spooky_box_alloc();
const spooky_box * spooky_box_scroll_box_alloc();

/* Initialize interface methods */
const spooky_box * spooky_box_init(spooky_box * self);
const spooky_box * spooky_box_scroll_box_init(spooky_box * self);

/* Allocate and initialize interface methods */
const spooky_box * spooky_box_acquire();
const spooky_box * spooky_box_scroll_box_acquire();

/* Construct data */
const spooky_box * spooky_box_ctor(const spooky_box * self, const spooky_context * context, const spooky_wm * wm, const spooky_box * parent, SDL_Window * window, SDL_Renderer * renderer, const char * name, SDL_Rect rect, bool is_visible, spooky_box_types box_type);

const spooky_box * spooky_box_scroll_box_ctor(const spooky_box * self, const spooky_context * context, const spooky_wm * wm, const spooky_box * parent, SDL_Window * window, SDL_Renderer * renderer, const char * name, SDL_Rect rect, bool is_visible, spooky_box_types box_type);

/* Destruct (dtor) data */
const spooky_box * spooky_box_dtor(const spooky_box * self);
const spooky_box * spooky_box_scroll_box_dtor(const spooky_box * self);

/* Free interface */
void spooky_box_free(const spooky_box * self);
/* Destruct and free interface */
void spooky_box_release(const spooky_box * self);

inline bool spooky_box_intersect(const SDL_Rect * const r, int x, int y) {
  return (x >= r->x) && (x < r->x + r->w) && (y >= r->y) && (y <= r->y + r->h);
}

#endif /* if 1 == 2 */

#endif /* SPOOKY_BOX__H */

