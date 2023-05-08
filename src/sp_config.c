#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "../include/sp_config.h"
#include "../include/sp_io.h"

typedef struct sp_config_data {
  const char * font_name;
  const char * disable_high_dpi;
  const char * data_path;

  int font_size;
  int window_width;
  int window_height;
  int canvas_width;
  int canvas_height;
  char padding[4]; /* not portable */
} sp_config_data;

static sp_config_data global_config_data = {
  .font_name = "print.char",
  .font_size = 18,
  .disable_high_dpi = "0",
  // 2048, 1536 retina: 2880 x 1800
  .window_width = 1024,
  .window_height = 768,
  .canvas_width = 1024,
  .canvas_height = 768,

  .data_path = NULL
};

static const char * sp_config_get_font_name(const sp_config * self);
static const char * sp_config_get_disable_high_dpi(const sp_config * self);
static int sp_config_get_font_size(const sp_config * self);

static int sp_config_get_window_width(const sp_config * self);
static int sp_config_get_window_height(const sp_config * self);
static int sp_config_get_canvas_width(const sp_config * self);
static int sp_config_get_canvas_height(const sp_config * self);
static const char * sp_config_get_data_path(const sp_config * self);

static const sp_config global_config = {
  .ctor = &sp_config_ctor,
  .dtor = &sp_config_dtor,
  .free = &sp_config_free,
  .release = &sp_config_release,

  .get_disable_high_dpi = &sp_config_get_disable_high_dpi,
  .get_font_name = &sp_config_get_font_name,
  .get_font_size = &sp_config_get_font_size,

  .get_window_width = &sp_config_get_window_width,
  .get_window_height = &sp_config_get_window_height,
  .get_canvas_width = &sp_config_get_canvas_width,
  .get_canvas_height = &sp_config_get_canvas_height,
  .get_data_path = &sp_config_get_data_path,

  .data = &global_config_data
};

const sp_config * sp_config_alloc() {
  return &global_config;
}

const sp_config * sp_config_init(sp_config * self) {
  (void)self;
  return &global_config;
}

const sp_config * sp_config_acquire() {
  return &global_config;
}

const sp_config * sp_config_ctor(const sp_config * self) {
  (void)self;
  return &global_config;
}

const sp_config * sp_config_dtor(const sp_config * self) {
  if(self && self->data) {
    free((char *)(uintptr_t)self->data->data_path), self->data->data_path = NULL;
  }

  return &global_config;
}

void sp_config_free(const sp_config * self) {
  (void)self;
}

void sp_config_release(const sp_config * self) {
  (void)self;
}

static const char * sp_config_get_font_name(const sp_config * self) {
  return self->data->font_name;
}

static int sp_config_get_font_size(const sp_config * self) {
  return self->data->font_size;
}

static const char * sp_config_get_disable_high_dpi(const sp_config * self) {
  return self->data->disable_high_dpi;
}

static int sp_config_get_window_width(const sp_config * self) {
  return self->data->window_width;
}

static int sp_config_get_window_height(const sp_config * self) {
  return self->data->window_height;
}

static int sp_config_get_canvas_width(const sp_config * self) {
  return self->data->canvas_width;
}

static int sp_config_get_canvas_height(const sp_config * self) {
  return self->data->canvas_height;
}

static const char * sp_config_get_data_path(const sp_config * self) {
  if(!(self->data->data_path)) {
    char * base_config_path = sp_io_alloc_config_path();
    sp_io_ensure_path(base_config_path, 0700);

    self->data->data_path = sp_io_alloc_concat_path(base_config_path, "/data");
    sp_io_ensure_path(self->data->data_path, 0700);

    free(base_config_path), base_config_path = NULL;
    fprintf(stdout, "Configuration path set to %s\n", self->data->data_path);
  }

  return self->data->data_path;
}

