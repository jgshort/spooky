#include <stdlib.h>

#include "sp_config.h"

typedef struct spooky_config_data {
  const char * font_name;
  int font_size;
  char padding[4]; /* not portable */
} spooky_config_data;

static spooky_config_data global_config_data = {
  .font_name = "deja.sans",
  .font_size = 30
};

static const char * spooky_config_get_font_name(const spooky_config * config);
static int spooky_config_get_font_size(const spooky_config * config);

static const spooky_config global_config = {
  .ctor = &spooky_config_ctor,
  .dtor = &spooky_config_dtor,
  .free = &spooky_config_free,
  .release = &spooky_config_release,

  .get_font_name = &spooky_config_get_font_name,
  .get_font_size = &spooky_config_get_font_size,
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
  (void)self;
  return &global_config;
}

void spooky_config_free(const spooky_config * self) {
  (void)self;
}

void spooky_config_release(const spooky_config * self) {
  (void)self;
}

static const char * spooky_config_get_font_name(const spooky_config * config) {
  return config->data->font_name;
}

static int spooky_config_get_font_size(const spooky_config * config) {
  return config->data->font_size;
}
