#pragma once

#ifndef SP_CONSOLE__H
#define SP_CONSOLE__H

#ifdef __cplusplus
extern "C" {
#endif

#include "sp_base.h"
#include "sp_context.h"

  struct sp_console_impl;
  typedef struct sp_console sp_console;

  typedef struct sp_console {
    sp_base super;

    const sp_base * (*as_base)(const sp_console * self);
    const sp_console * (*ctor)(const sp_console * self, const char * name, const sp_context * context, SDL_Renderer * renderer);
    const sp_console * (*dtor)(const sp_console * self);
    void (*free)(const sp_console * self);
    void (*release)(const sp_console * self);

    void (*push_str)(const sp_console * self, const char * str);

    const char * (*get_current_command)(const sp_console * self);
    void (*clear_current_command)(const sp_console * self);
    void (*clear_console)(const sp_console * self);

    struct sp_console_impl * impl;
  } sp_console;

  const sp_base * sp_console_as_base(const sp_console * self);
  const sp_console * sp_console_init(sp_console * self);
  const sp_console * sp_console_alloc(void);
  const sp_console * sp_console_acquire(void);
  const sp_console * sp_console_ctor(const sp_console * self, const char * name, const sp_context * context, SDL_Renderer * renderer);
  const sp_console * sp_console_dtor(const sp_console * self);
  void sp_console_free(const sp_console * self);
  void sp_console_release(const sp_console * self);

#ifdef __cplusplus
}
#endif


#endif /* SP_CONSOLE__H */

