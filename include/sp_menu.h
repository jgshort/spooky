#pragma once

#ifndef SPOOKY_MENU__H
#define SPOOKY_MENU__H

#include "sp_gui.h"
#include "sp_context.h"
#include "sp_font.h"

#ifdef __cplusplus
extern "C" {
#endif

  typedef enum spooky_menu_types {
    SMT_NULL,
    SMT_MAIN_MENU,
    SMT_MAIN_MENU_ITEM,
    SMT_MAIN_MENU_ACTION,
    SMT_EOE
  } spooky_menu_types;

  typedef struct spooky_menu_data spooky_menu_data;
  typedef struct spooky_menu spooky_menu;

  typedef struct spooky_menu {
    spooky_base super;

    const spooky_menu * (*ctor)(const spooky_menu * self, const spooky_font * font, const spooky_context * context, const char * name, SDL_Rect rect, spooky_menu_types menu_type);
    const spooky_menu * (*dtor)(const spooky_menu * self);
    void (*free)(const spooky_menu * self);
    void (*release)(const spooky_menu * self);

    const spooky_base * (*as_base)(const spooky_menu * self);

    const char * (*get_name)(const spooky_menu * self);

    const spooky_menu * (*attach_item)(const spooky_menu * self, const spooky_menu * menu, const spooky_menu ** out_copy);
    spooky_menu_types (*get_menu_type)(const spooky_menu * self);

    void (*set_x)(const spooky_menu * self, int x);
    int (*get_x)(const spooky_menu * self);
    void (*set_y)(const spooky_menu * self, int y);
    int (*get_y)(const spooky_menu * self);

    void (*set_w)(const spooky_menu * self, int w);
    int (*get_w)(const spooky_menu * self);

    void (*set_h)(const spooky_menu * self, int h);
    int (*get_h)(const spooky_menu * self);

    const spooky_menu * (*get_active_menu)(const spooky_menu * self);
    void (*set_active_menu)(const spooky_menu * self, const spooky_menu * active_menu);

    void (*set_parent)(const spooky_menu * self, const spooky_menu * parent);
    bool (*get_is_active)(const spooky_menu * self);

    int (*get_name_width)(const spooky_menu * self);
    int (*get_name_height)(const spooky_menu * self);
    void (*set_is_expanded)(const spooky_menu * self, bool value);

    spooky_menu_data * data;
  } spooky_menu;

  /* Allocate (malloc) interface */
  const spooky_menu * spooky_menu_alloc(void);
  /* Initialize interface methods */
  const spooky_menu * spooky_menu_init(spooky_menu * self);
  /* Allocate and initialize interface methods */
  const spooky_menu * spooky_menu_acquire(void);

  /* Construct data */
  const spooky_menu * spooky_menu_ctor(const spooky_menu * self, const spooky_font * font, const spooky_context * context, const char * name, SDL_Rect rect, spooky_menu_types menu_type);

  /* Destruct (dtor) data */
  const spooky_menu * spooky_menu_dtor(const spooky_menu * self);

  /* Free interface */
  void spooky_menu_free(const spooky_menu * self);
  /* Destruct and free interface */
  void spooky_menu_release(const spooky_menu * self);

  const spooky_menu * spooky_menu_load_from_file(const spooky_context * /* context */, SDL_Rect /* rect */, const char * /* path */);

#ifdef __cplusplus
}
#endif

#endif /* SPOOKY_MENU__H */

