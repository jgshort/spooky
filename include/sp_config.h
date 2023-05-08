#pragma once

#ifndef SP_CONFIG__H
#define SP_CONFIG__H

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct sp_config sp_config;
  typedef struct sp_config_data sp_config_data;

  typedef struct sp_config {
    const sp_config * (*ctor)(const sp_config * /* self */);
    const sp_config * (*dtor)(const sp_config * /* self */);
    void (*free)(const sp_config * /* self */);
    void (*release)(const sp_config * /* self */);

    int (*get_font_size)(const sp_config * /* self */);
    const char * (*get_font_name)(const sp_config * /* self */);
    const char * (*get_disable_high_dpi)(const sp_config * /* self */);

    int (*get_window_width)(const sp_config * /* self */);
    int (*get_window_height)(const sp_config * /* self */);
    int (*get_canvas_width)(const sp_config * /* self */);
    int (*get_canvas_height)(const sp_config * /* self */);
    const char * (*get_data_path)(const sp_config * /* self */);

    sp_config_data * data;
  } sp_config;

  /* Allocate (malloc) interface */
  const sp_config * sp_config_alloc(void);
  /* Initialize interface methods */
  const sp_config * sp_config_init(sp_config * /* self */);
  /* Allocate and initialize interface methods */
  const sp_config * sp_config_acquire(void);
  /* Construct data */
  const sp_config * sp_config_ctor(const sp_config * /* self */);
  /* Destruct (dtor) data */
  const sp_config * sp_config_dtor(const sp_config * /* self */);
  /* Free interface */
  void sp_config_free(const sp_config * /* self */);
  /* Destruct and free interface */
  void sp_config_release(const sp_config * /* self */);

#ifdef __cplusplus
}
#endif

#endif /* SP_CONFIG__H */

