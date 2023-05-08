#ifndef SP_BOX__H
#define SP_BOX__H

#ifdef __cplusplus
extern "C" {
#endif

#include "sp_context.h"
#include "sp_sprite.h"

  typedef struct sp_box sp_box;
  typedef struct sp_box_data sp_box_data;

  typedef enum sp_box_draw_style {
    SBDS_EMPTY,
    SBDS_DEFAULT,
    SBDS_FILL = SBDS_DEFAULT,
    SBDS_OUTLINE,
    SBDS_EOE
  } sp_box_draw_style;

  typedef struct sp_box {
    sp_base super;
    const sp_box * (*ctor)(const sp_box * /* self */, const char * /* name */, const sp_context * /* context */, SDL_Rect /* origin */);
    const sp_box * (*dtor)(const sp_box * /* self */);
    void (*free)(const sp_box * /* self */);
    void (*release)(const sp_box * /* self */);
    const sp_base * (*as_base)(const sp_box * /* self */);

    void (*set_name)(const sp_box * /* self */, const char * /* name */);
    const char * (*get_name)(const sp_box * /* self */);

    void (*set_sprite)(const sp_box * /* self */, const sp_sprite * /* sprite */);
    const sp_sprite * (*get_sprite)(const sp_box * /* self */);

    sp_box_draw_style (*get_draw_style)(const sp_box * /* self */);
    void (*set_draw_style)(const sp_box * /* self */, sp_box_draw_style /* style */);

    sp_box_data * data;
  } sp_box;

  const sp_base * sp_box_as_base(const sp_box * /* self */);

  /* Allocate (malloc) interface */
  const sp_box * sp_box_alloc(void);
  /* Initialize interface methods */
  const sp_box * sp_box_init(sp_box * /* self */);
  /* Allocate and initialize interface methods */
  const sp_box * sp_box_acquire(void);
  /* Construct data */
  const sp_box * sp_box_ctor(const sp_box * /* self */, const char * /* name */, const sp_context * /* context */, SDL_Rect /* origin */);
  /* Destruct (dtor) data */
  const sp_box * sp_box_dtor(const sp_box * /* self */);
  /* Free interface */
  void sp_box_free(const sp_box * /* self */);
  /* Destruct and free interface */
  void sp_box_release(const sp_box * /* self */);


#if 1 == 2

#include "sp_base.h"
#include "sp_types.h"
#include "sp_gui.h"
#include "sp_wm.h"

  const int scroll_bar_width;

  typedef struct sp_box sp_box;
  typedef struct sp_box_data sp_box_data;
  typedef struct sp_box_scroll_box_item sp_box_scroll_box_item;

  typedef enum sp_box_types {
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
  } sp_box_types;

  typedef enum sp_box_scroll_box_direction {
    SBSBD_VERTICAL = 0,
    SBSBD_HORIZONTAL = 1,
    SBSBD_EOE,
  } sp_box_scroll_box_direction;

  typedef struct sp_box {
    sp_base super;
    const sp_base * (*as_base)(const sp_box * self);
    const sp_box * (*ctor)(const sp_box * self, const sp_context * context, const sp_wm * wm, const sp_box * parent, SDL_Window * window, SDL_Renderer * renderer, const char * name, SDL_Rect rect, bool is_visible, sp_box_types box_type);
    const sp_box * (*dtor)(const sp_box * self);
    void (*free)(const sp_box * self);
    void (*release)(const sp_box * self);

    void (*set_z_order)(const sp_box * self, int z_order);
    int (*get_z_order)(const sp_box * self);

    const char * (*get_name)(const sp_box * self);
    const SDL_Rect * (*get_rect)(const sp_box * self);
    const sp_box * (*attach_box)(const sp_box * self, const sp_box * box);
    //const sp_box * (*attach_select_box_item)(const sp_box * self, const char * text);
    sp_box_types (*get_box_type)(const sp_box * self);

    void (*set_x)(const sp_box * self, int x);
    int (*get_x)(const sp_box * self);
    void (*set_y)(const sp_box * self, int y);
    int (*get_y)(const sp_box * self);

    void (*set_w)(const sp_box * self, int w);
    int (*get_w)(const sp_box * self);

    void (*set_h)(const sp_box * self, int h);
    int (*get_h)(const sp_box * self);

    const sp_box_scroll_box_item * (*attach_scroll_box_item_text)(const sp_box * self, const char * text, const char * description);
    const sp_box_scroll_box_item * (*attach_scroll_box_item_image)(const sp_box * self, const char * text, const char * description, const char * path);
    void (*set_direction)(const sp_box * self, sp_box_scroll_box_direction direction);

    sp_box_data * data;
  } sp_box_interface;

  const sp_base * sp_box_as_base(const sp_box * self);

  const sp_box_interface sp_box_class;

  /* Allocate (malloc) interface */
  const sp_box * sp_box_alloc();
  const sp_box * sp_box_scroll_box_alloc();

  /* Initialize interface methods */
  const sp_box * sp_box_init(sp_box * self);
  const sp_box * sp_box_scroll_box_init(sp_box * self);

  /* Allocate and initialize interface methods */
  const sp_box * sp_box_acquire();
  const sp_box * sp_box_scroll_box_acquire();

  /* Construct data */
  const sp_box * sp_box_ctor(const sp_box * self, const sp_context * context, const sp_wm * wm, const sp_box * parent, SDL_Window * window, SDL_Renderer * renderer, const char * name, SDL_Rect rect, bool is_visible, sp_box_types box_type);

  const sp_box * sp_box_scroll_box_ctor(const sp_box * self, const sp_context * context, const sp_wm * wm, const sp_box * parent, SDL_Window * window, SDL_Renderer * renderer, const char * name, SDL_Rect rect, bool is_visible, sp_box_types box_type);

  /* Destruct (dtor) data */
  const sp_box * sp_box_dtor(const sp_box * self);
  const sp_box * sp_box_scroll_box_dtor(const sp_box * self);

  /* Free interface */
  void sp_box_free(const sp_box * self);
  /* Destruct and free interface */
  void sp_box_release(const sp_box * self);

  inline bool sp_box_intersect(const SDL_Rect * const r, int x, int y) {
    return (x >= r->x) && (x < r->x + r->w) && (y >= r->y) && (y <= r->y + r->h);
  }

#endif /* if 1 == 2 */

#ifdef __cplusplus
}
#endif

#endif /* SP_BOX__H */

