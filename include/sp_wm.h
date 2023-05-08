#ifndef SP_WM__H
#define SP_WM__H

#ifdef __cplusplus
extern "C" {
#endif

#include "sp_base.h"
#include "sp_context.h"
#include "sp_iter.h"

  typedef struct sp_wm sp_wm;
  typedef struct sp_wm {
    sp_base super;

    const sp_base * (*as_base)(const sp_wm * /* self */);
    const sp_wm * (*ctor)(const sp_wm * /* self */, const char * /* name */, const sp_context * /* context */);
    const sp_wm * (*dtor)(const sp_wm * /* self */);
    void (*free)(const sp_wm * /* self */);
    void (*release)(const sp_wm * /* self */);

    void (*register_window)(sp_wm const * /* self */, const sp_base * /* active_object */);
    void (*activate_window)(sp_wm const * /* self */, const sp_base * /* active_object */);
    //const sp_iter * (*window_iter)(sp_wm const * /* self */);

    const sp_base * (*get_active_object)(const sp_wm * /* self */);

    void (*set_active_object)(const sp_wm * /* self */, const sp_base * /* active_object */);
    int (*get_max_z_order)(const sp_wm * /* self */);
    struct sp_wm_data * data;
  } sp_wm;

  const sp_wm * sp_wm_alloc(void);
  const sp_wm * sp_wm_init(sp_wm * /* self */);
  const sp_wm * sp_wm_acquire(void);
  const sp_wm * sp_wm_ctor(const sp_wm * /* self */, const char * /* name */, const sp_context * /* context */);
  const sp_wm * sp_wm_dtor(const sp_wm * /* self */);
  void sp_wm_free(const sp_wm * /* self */);
  void sp_wm_release(const sp_wm * /* self */);

#ifdef __cplusplus
}
#endif


#endif /* SP_WM__H */

