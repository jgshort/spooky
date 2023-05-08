#pragma once

#ifndef SP_MENU__H
#define SP_MENU__H

#ifdef __cplusplus
extern "C" {
#endif

#include "sp_gui.h"
#include "sp_context.h"
#include "sp_font.h"

  typedef enum sp_menu_types {
    SMT_NULL,
    SMT_MAIN_MENU,
    SMT_MAIN_MENU_ITEM,
    SMT_MAIN_MENU_ACTION,
    SMT_EOE
  } sp_menu_types;

  typedef struct sp_menu_data sp_menu_data;
  typedef struct sp_menu sp_menu;

  typedef struct sp_menu {
    sp_base super;

    const sp_menu * (*ctor)(const sp_menu * self, const sp_font * font, const sp_context * context, const char * name, SDL_Rect rect, sp_menu_types menu_type);
    const sp_menu * (*dtor)(const sp_menu * self);
    void (*free)(const sp_menu * self);
    void (*release)(const sp_menu * self);

    const sp_base * (*as_base)(const sp_menu * self);

    const char * (*get_name)(const sp_menu * self);

    const sp_menu * (*attach_item)(const sp_menu * self, const sp_menu * menu, const sp_menu ** out_copy);
    sp_menu_types (*get_menu_type)(const sp_menu * self);

    void (*set_x)(const sp_menu * self, int x);
    int (*get_x)(const sp_menu * self);
    void (*set_y)(const sp_menu * self, int y);
    int (*get_y)(const sp_menu * self);

    void (*set_w)(const sp_menu * self, int w);
    int (*get_w)(const sp_menu * self);

    void (*set_h)(const sp_menu * self, int h);
    int (*get_h)(const sp_menu * self);

    const sp_menu * (*get_active_menu)(const sp_menu * self);
    void (*set_active_menu)(const sp_menu * self, const sp_menu * active_menu);

    void (*set_parent)(const sp_menu * self, const sp_menu * parent);
    bool (*get_is_active)(const sp_menu * self);

    int (*get_name_width)(const sp_menu * self);
    int (*get_name_height)(const sp_menu * self);
    void (*set_is_expanded)(const sp_menu * self, bool value);

    sp_menu_data * data;
  } sp_menu;

  /* Allocate (malloc) interface */
  const sp_menu * sp_menu_alloc(void);
  /* Initialize interface methods */
  const sp_menu * sp_menu_init(sp_menu * self);
  /* Allocate and initialize interface methods */
  const sp_menu * sp_menu_acquire(void);

  /* Construct data */
  const sp_menu * sp_menu_ctor(const sp_menu * self, const sp_font * font, const sp_context * context, const char * name, SDL_Rect rect, sp_menu_types menu_type);

  /* Destruct (dtor) data */
  const sp_menu * sp_menu_dtor(const sp_menu * self);

  /* Free interface */
  void sp_menu_free(const sp_menu * self);
  /* Destruct and free interface */
  void sp_menu_release(const sp_menu * self);

  const sp_menu * sp_menu_load_from_file(const sp_context * /* context */, SDL_Rect /* rect */, const char * /* path */);

#ifdef __cplusplus
}
#endif

#endif /* SP_MENU__H */

