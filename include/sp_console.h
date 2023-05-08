#pragma once

#ifndef SP_CONSOLE__H
#define SP_CONSOLE__H

#ifdef __cplusplus
extern "C" {
#endif

#include "sp_base.h"
#include "sp_context.h"

  struct spooky_console_impl;
  typedef struct spooky_console spooky_console;

  typedef struct spooky_console {
    spooky_base super;

    const spooky_base * (*as_base)(const spooky_console * self);
    const spooky_console * (*ctor)(const spooky_console * self, const char * name, const spooky_context * context, SDL_Renderer * renderer);
    const spooky_console * (*dtor)(const spooky_console * self);
    void (*free)(const spooky_console * self);
    void (*release)(const spooky_console * self);

    void (*push_str)(const spooky_console * self, const char * str);

    const char * (*get_current_command)(const spooky_console * self);
    void (*clear_current_command)(const spooky_console * self);
    void (*clear_console)(const spooky_console * self);

    struct spooky_console_impl * impl;
  } spooky_console;

  const spooky_base * spooky_console_as_base(const spooky_console * self);
  const spooky_console * spooky_console_init(spooky_console * self);
  const spooky_console * spooky_console_alloc(void);
  const spooky_console * spooky_console_acquire(void);
  const spooky_console * spooky_console_ctor(const spooky_console * self, const char * name, const spooky_context * context, SDL_Renderer * renderer);
  const spooky_console * spooky_console_dtor(const spooky_console * self);
  void spooky_console_free(const spooky_console * self);
  void spooky_console_release(const spooky_console * self);

#ifdef __cplusplus
}
#endif


#endif /* SP_CONSOLE__H */

