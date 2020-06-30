#ifndef SP_HELP__H
#define SP_HELP__H

#include "sp_base.h"
#include "sp_context.h"

struct spooky_help_data;
typedef struct spooky_help spooky_help;
typedef struct spooky_help {
  spooky_base super;

  const spooky_help * (*ctor)(const spooky_help * self, const spooky_context * context);
  const spooky_help * (*dtor)(const spooky_help * self);
  void (*free)(const spooky_help * self);
  void (*release)(const spooky_help * self);

  struct spooky_help_data * data;
} spooky_help;

const spooky_help * spooky_help_init(spooky_help * self);
const spooky_help * spooky_help_alloc();
const spooky_help * spooky_help_acquire();

const spooky_help * spooky_help_ctor(const spooky_help * self, const spooky_context * context);
const spooky_help * spooky_help_dtor(const spooky_help * self);
void spooky_help_free(const spooky_help * self);
void spooky_help_release(const spooky_help * self);

void spooky_help_handle_event(const spooky_base * self, SDL_Event * event);
void spooky_help_handle_delta(const spooky_base * self, double interpolation);
void spooky_help_render(const spooky_base * self, SDL_Renderer * renderer);
 
#endif

