#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "sp_config.h"
#include "sp_io.h"

typedef struct spooky_config_data {
  const char * font_name;
  const char * disable_high_dpi;
  const char * data_path;

  int font_size;
  int window_width;
  int window_height;
  int canvas_width;
  int canvas_height;
  char padding[4]; /* not portable */
} spooky_config_data;

static spooky_config_data global_config_data = {
  .font_name = "deja.sans",
  .font_size = 18,
  .disable_high_dpi = "0",
  // 2048, 1536 retina: 2880 x 1800
  .window_width = 1024,
  .window_height = 768,
  .canvas_width = 1024,
  .canvas_height = 768,

  .data_path = NULL
};

static const char * spooky_config_get_font_name(const spooky_config * self);
static const char * spooky_config_get_disable_high_dpi(const spooky_config * self);
static int spooky_config_get_font_size(const spooky_config * self);

static int spooky_config_get_window_width(const spooky_config * self);
static int spooky_config_get_window_height(const spooky_config * self);
static int spooky_config_get_canvas_width(const spooky_config * self);
static int spooky_config_get_canvas_height(const spooky_config * self);
static const char * spooky_config_get_data_path(const spooky_config * self);

static const spooky_config global_config = {
  .ctor = &spooky_config_ctor,
  .dtor = &spooky_config_dtor,
  .free = &spooky_config_free,
  .release = &spooky_config_release,

  .get_disable_high_dpi = &spooky_config_get_disable_high_dpi,
  .get_font_name = &spooky_config_get_font_name,
  .get_font_size = &spooky_config_get_font_size,

  .get_window_width = &spooky_config_get_window_width,
  .get_window_height = &spooky_config_get_window_height,
  .get_canvas_width = &spooky_config_get_canvas_width,
  .get_canvas_height = &spooky_config_get_canvas_height,
  .get_data_path = &spooky_config_get_data_path,

  .data = &global_config_data
};

const spooky_config * spooky_config_alloc() {
  return &global_config;
}

const spooky_config * spooky_config_init(spooky_config * self) {
  (void)self;
  return &global_config;
}

const spooky_config * spooky_config_acquire() {
  return &global_config;
}

const spooky_config * spooky_config_ctor(const spooky_config * self) {
  (void)self;
  return &global_config;
}

const spooky_config * spooky_config_dtor(const spooky_config * self) {
  if(self && self->data) {
    free((char *)(uintptr_t)self->data->data_path), self->data->data_path = NULL;
  }

  return &global_config;
}

void spooky_config_free(const spooky_config * self) {
  (void)self;
}

void spooky_config_release(const spooky_config * self) {
  (void)self;
}

static const char * spooky_config_get_font_name(const spooky_config * self) {
  return self->data->font_name;
}

static int spooky_config_get_font_size(const spooky_config * self) {
  return self->data->font_size;
}

static const char * spooky_config_get_disable_high_dpi(const spooky_config * self) {
  return self->data->disable_high_dpi;
}

static int spooky_config_get_window_width(const spooky_config * self) {
  return self->data->window_width;
}

static int spooky_config_get_window_height(const spooky_config * self) {
  return self->data->window_height;
}

static int spooky_config_get_canvas_width(const spooky_config * self) {
  return self->data->canvas_width;
}

static int spooky_config_get_canvas_height(const spooky_config * self) {
  return self->data->canvas_height;
}

static const char * spooky_config_get_data_path(const spooky_config * self) {
  if(!(self->data->data_path)) {
    char * base_config_path = spooky_io_alloc_config_path();
    spooky_io_ensure_path(base_config_path, 0700);

    self->data->data_path = spooky_io_alloc_concat_path(base_config_path, "/data");
    spooky_io_ensure_path(self->data->data_path, 0700);

    free(base_config_path), base_config_path = NULL;
    fprintf(stdout, "Configuration path set to %s\n", self->data->data_path);
  }

  return self->data->data_path;
}
