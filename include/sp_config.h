#pragma once

#ifndef SP_CONFIG__H
#define SP_CONFIG__H

typedef struct spooky_config spooky_config;
typedef struct spooky_config_data spooky_config_data;

typedef struct spooky_config {
  const spooky_config * (*ctor)(const spooky_config * /* self */);
  const spooky_config * (*dtor)(const spooky_config * /* self */);
  void (*free)(const spooky_config * /* self */);
  void (*release)(const spooky_config * /* self */);

  int (*get_font_size)(const spooky_config * /* self */);
  const char * (*get_font_name)(const spooky_config * /* self */);
  const char * (*get_disable_high_dpi)(const spooky_config * /* self */);

  int (*get_window_width)(const spooky_config * /* self */);
  int (*get_window_height)(const spooky_config * /* self */);
  int (*get_canvas_width)(const spooky_config * /* self */);
  int (*get_canvas_height)(const spooky_config * /* self */);


  spooky_config_data * data;
} spooky_config;

/* Allocate (malloc) interface */
const spooky_config * spooky_config_alloc();
/* Initialize interface methods */
const spooky_config * spooky_config_init(spooky_config * /* self */);
/* Allocate and initialize interface methods */
const spooky_config * spooky_config_acquire();
/* Construct data */
const spooky_config * spooky_config_ctor(const spooky_config * /* self */);
/* Destruct (dtor) data */
const spooky_config * spooky_config_dtor(const spooky_config * /* self */);
/* Free interface */
void spooky_config_free(const spooky_config * /* self */);
/* Destruct and free interface */
void spooky_config_release(const spooky_config * /* self */);

#endif /* SP_CONFIG__H */
