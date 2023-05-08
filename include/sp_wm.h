#ifndef SPOOKY_WM__H
#define SPOOKY_WM__H

#ifdef __cplusplus
extern "C" {
#endif

#include "sp_base.h"
#include "sp_context.h"
#include "sp_iter.h"

  typedef struct spooky_wm spooky_wm;
  typedef struct spooky_wm {
    spooky_base super;

    const spooky_base * (*as_base)(const spooky_wm * /* self */);
    const spooky_wm * (*ctor)(const spooky_wm * /* self */, const char * /* name */, const spooky_context * /* context */);
    const spooky_wm * (*dtor)(const spooky_wm * /* self */);
    void (*free)(const spooky_wm * /* self */);
    void (*release)(const spooky_wm * /* self */);

    void (*register_window)(spooky_wm const * /* self */, const spooky_base * /* active_object */);
    void (*activate_window)(spooky_wm const * /* self */, const spooky_base * /* active_object */);
    //const spooky_iter * (*window_iter)(spooky_wm const * /* self */);

    const spooky_base * (*get_active_object)(const spooky_wm * /* self */);

    void (*set_active_object)(const spooky_wm * /* self */, const spooky_base * /* active_object */);
    int (*get_max_z_order)(const spooky_wm * /* self */);
    struct spooky_wm_data * data;
  } spooky_wm;

  const spooky_wm * spooky_wm_alloc(void);
  const spooky_wm * spooky_wm_init(spooky_wm * /* self */);
  const spooky_wm * spooky_wm_acquire(void);
  const spooky_wm * spooky_wm_ctor(const spooky_wm * /* self */, const char * /* name */, const spooky_context * /* context */);
  const spooky_wm * spooky_wm_dtor(const spooky_wm * /* self */);
  void spooky_wm_free(const spooky_wm * /* self */);
  void spooky_wm_release(const spooky_wm * /* self */);

#ifdef __cplusplus
}
#endif


#endif /* SPOOKY_WM__H */

