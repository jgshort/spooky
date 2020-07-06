#ifndef SP_CONSOLE__H
#define SP_CONSOLE__H

#include "sp_base.h"
#include "sp_context.h"

struct spooky_console_data;
typedef struct spooky_console spooky_console;

typedef struct spooky_console {
  spooky_base super;

  const spooky_console * (*ctor)(const spooky_console * self, const spooky_context * context, SDL_Renderer * renderer);
  const spooky_console * (*dtor)(const spooky_console * self);
  void (*free)(const spooky_console * self);
  void (*release)(const spooky_console * self);

  void (*push_str)(const spooky_console * self, const char * str);

  struct spooky_console_data * data;
} spooky_console;

const spooky_console * spooky_console_init(spooky_console * self);
const spooky_console * spooky_console_alloc();
const spooky_console * spooky_console_acquire();
const spooky_console * spooky_console_ctor(const spooky_console * self, const spooky_context * context, SDL_Renderer * renderer);
const spooky_console * spooky_console_dtor(const spooky_console * self);
void spooky_console_free(const spooky_console * self);
void spooky_console_release(const spooky_console * self);

#endif /* SP_CONSOLE__H */

