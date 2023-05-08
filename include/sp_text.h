#pragma once

#ifndef SP_TEXT__H
#define SP_TEXT__H

#ifdef __cplusplus
extern "C" {
#endif

#include "sp_base.h"
#include "sp_context.h"

  struct sp_text_data;
  typedef struct sp_text sp_text;

  typedef struct sp_text {
    sp_base super;

    const sp_base * (*as_base)(const sp_text * /* self */);
    const sp_text * (*ctor)(const sp_text * /* self */, const char * name, const sp_context * /* context */, SDL_Renderer * /* renderer */);
    const sp_text * (*dtor)(const sp_text * /* self */);
    void (*free)(const sp_text * /* self */);
    void (*release)(const sp_text * /* self */);

    struct sp_text_data * data;
  } sp_text;

  const sp_base * sp_text_as_base(const sp_text * /* self */);
  const sp_text * sp_text_init(sp_text * /* self */);
  const sp_text * sp_text_alloc(void);
  const sp_text * sp_text_acquire(void);
  const sp_text * sp_text_ctor(const sp_text * /* self */, const char * name, const sp_context * /* context */, SDL_Renderer * /* renderer */);
  const sp_text * sp_text_dtor(const sp_text * /* self */);

  void sp_text_free(const sp_text * /* self */);
  void sp_text_release(const sp_text * /* self */);

#ifdef __cplusplus
}
#endif

#endif /* SP_TEXT__H */

