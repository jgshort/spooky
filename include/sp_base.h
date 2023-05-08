#ifndef SP_BASE__H
#define SP_BASE__H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "sp_iter.h"
#include "sp_gui.h"


  struct sp_base_data;
  typedef struct sp_base sp_base;
  typedef struct sp_base {
    const sp_base * (*ctor)(const sp_base * /* self */, const char * /* name */, SDL_Rect /* origin */);
    const sp_base * (*dtor)(const sp_base * /* self */);
    void (*free)(const sp_base * /* self */);
    void (*release)(const sp_base * /* self */);

    const char * (*get_name)(const sp_base * /* self */);

    bool (*handle_event)(const sp_base * /* self */, SDL_Event * /* event */);
    void (*handle_delta)(const sp_base * /* self */, const SDL_Event * /* event */, uint64_t /* last_update_time */, double /* interpolation */);
    void (*render)(const sp_base * /* self */, SDL_Renderer * /* renderer */);

    const SDL_Rect * (*get_rect)(const sp_base * /* self */);
    errno_t (*set_rect)(const sp_base * /* self */, const SDL_Rect * /* rect */, const sp_ex ** /* ex */);

    int (*get_x)(const sp_base * /* self */);
    void (*set_x)(const sp_base * /* self */, int /* x */);
    int (*get_y)(const sp_base * /* self */);
    void (*set_y)(const sp_base * /* self */, int /* y */);
    int (*get_w)(const sp_base * /* self */);
    void (*set_w)(const sp_base * /* self */, int /* w */);
    int (*get_h)(const sp_base * /* self */);
    void (*set_h)(const sp_base * /* self */, int /* h */);

    errno_t (*set_rect_relative)(const sp_base * /* self */, const SDL_Rect * /* from_rect */, const sp_ex ** /* ex */);
    errno_t (*get_rect_relative)(const sp_base * /* self */, const SDL_Rect * /* from_rect */, SDL_Rect * /* out_rect */, const sp_ex ** /* ex */);

    const sp_iter * (*get_iterator)(const sp_base * /* self */);
    size_t (*get_children_count)(const sp_base * /* self */);
    size_t (*get_children_capacity)(const sp_base * /* self */);

    errno_t (*add_child)(const sp_base * /* self */, const sp_base * /* child */, const sp_ex ** /* ex */);
    errno_t (*children_iter)(const sp_base * /* self */, const sp_iter ** /* out_it */, const sp_ex ** /* ex */);
    void (*set_z_order)(const sp_base * /* self */, size_t /* z_order */);
    size_t (*get_z_order)(const sp_base * /*self */);

    errno_t (*get_bounds)(const sp_base * /* self */, SDL_Rect * /* out_bounds */, const sp_ex ** /* ex */);

    bool (*get_focus)(const sp_base * /* self */);
    void (*set_focus)(const sp_base * /* self */, bool /* is_focus */);

    bool (*get_is_modal)(const sp_base * /* self */);
    void (*set_is_modal)(const sp_base * /* self */, bool /* is_modal */);

    struct sp_base_data * data;
  } sp_base;

  const sp_base * sp_base_init(sp_base * /* self */);
  const sp_base * sp_base_alloc(void);
  const sp_base * sp_base_acquire(void);
  const sp_base * sp_base_ctor(const sp_base * /* self */, const char * /* name */, SDL_Rect /* origin */);
  const sp_base * sp_base_dtor(const sp_base * /* self */);
  void sp_base_free(const sp_base * /* self */);
  void sp_base_release(const sp_base * /* self */);

  void sp_base_z_sort(const sp_base ** /* items */, size_t /* items_count */);

#ifdef __cplusplus
}
#endif

#endif /* SP_BASE__H */

