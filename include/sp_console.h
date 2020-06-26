#ifndef SP_CONSOLE__H
#define SP_CONSOLE__H

#include "sp_base.h"

typedef struct spooky_console spooky_console;

typedef struct spooky_console {
  spooky_base super;

  const spooky_console * (*ctor)(const spooky_console * self, SDL_Renderer * renderer);
  const spooky_console * (*dtor)(const spooky_console * self);
  void (*free)(const spooky_console * self);
  void (*release)(const spooky_console * self);

  struct spooky_console_data * data;
} spooky_console;

const spooky_console * spooky_console_init(spooky_console * self);
const spooky_console * spooky_console_alloc();
const spooky_console * spooky_console_acquire();
const spooky_console * spooky_console_ctor(const spooky_console * self, SDL_Renderer * renderer);
const spooky_console * spooky_console_dtor(const spooky_console * self);
void spooky_console_free(const spooky_console * self);
void spooky_console_release(const spooky_console * self);

/*
void spooky_console_handle_event(const spooky_console * console);
void spooky_console_handle_delta(const spooky_console * console, double interpolation);
void spooky_console_render(const spooky_console * console, SDL_Renderer * renderer);
*/

#endif /* SP_CONSOLE__H */

