#ifndef SPOOKY_WM__H
#define SPOOKY_WM__H

#include "sp_base.h"
#include "sp_context.h"
#include "sp_iter.h"

typedef struct spooky_box {
  int x;
} spooky_box;

typedef struct spooky_wm spooky_wm;
typedef struct spooky_wm {
  spooky_base super;

  const spooky_wm * (*ctor)(const spooky_wm * /* self */, const spooky_context * /* context */);
  const spooky_wm * (*dtor)(const spooky_wm * /* self */);
  void (*free)(const spooky_wm * /* self */);
  void (*release)(const spooky_wm * /* self */);

  void (*register_window)(spooky_wm const * /* self */, const spooky_box * /* active_box */);
  void (*activate_window)(spooky_wm const * /* self */, const spooky_box * /* active_box */);
  const spooky_iter * (*window_iter)(spooky_wm const * /* self */);

  const spooky_box * (*get_active_box)(const spooky_wm * /* self */);

  void (*set_active_box)(const spooky_wm * /* self */, const spooky_box * /* active_box */);
  int (*get_max_z_order)(const spooky_wm * /* self */);
	struct spooky_wm_data * data;
} spooky_wm;

const spooky_wm * spooky_wm_alloc();
const spooky_wm * spooky_wm_init(spooky_wm * /* self */);
const spooky_wm * spooky_wm_acquire();
const spooky_wm * spooky_wm_ctor(const spooky_wm * /* self */, const spooky_context * /* context */);
const spooky_wm * spooky_wm_dtor(const spooky_wm * /* self */);
void spooky_wm_free(const spooky_wm * /* self */);
void spooky_wm_release(const spooky_wm * /* self */);

#endif /* SPOOKY_WM__H */

