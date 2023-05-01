#pragma once

#ifndef SP_TEXT__H
#define SP_TEXT__H

#include "sp_base.h"
#include "sp_context.h"

struct spooky_text_data;
typedef struct spooky_text spooky_text;

typedef struct spooky_text {
  spooky_base super;

  const spooky_base * (*as_base)(const spooky_text * /* self */);
  const spooky_text * (*ctor)(const spooky_text * /* self */, const char * name, const spooky_context * /* context */, SDL_Renderer * /* renderer */);
  const spooky_text * (*dtor)(const spooky_text * /* self */);
  void (*free)(const spooky_text * /* self */);
  void (*release)(const spooky_text * /* self */);

  struct spooky_text_data * data;
} spooky_text;

const spooky_base * spooky_text_as_base(const spooky_text * /* self */);
const spooky_text * spooky_text_init(spooky_text * /* self */);
const spooky_text * spooky_text_alloc(void);
const spooky_text * spooky_text_acquire(void);
const spooky_text * spooky_text_ctor(const spooky_text * /* self */, const char * name, const spooky_context * /* context */, SDL_Renderer * /* renderer */);
const spooky_text * spooky_text_dtor(const spooky_text * /* self */);

void spooky_text_free(const spooky_text * /* self */);
void spooky_text_release(const spooky_text * /* self */);

#endif /* SP_TEXT__H */

