#ifndef SP_POOL__H
#define SP_POOL__H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include "sp_str.h"

typedef struct spooky_pool_impl spooky_pool_impl;

typedef struct spooky_pool spooky_pool;
typedef struct spooky_pool {
  const spooky_pool * (*ctor)(const spooky_pool * /* self */);
  const spooky_pool * (*dtor)(const spooky_pool * /* self */);
  void (*free)(const spooky_pool * /* self */);
  void (*release)(const spooky_pool * /* self */);
  spooky_pool_impl * impl;
} spooky_pool;

const spooky_pool * spooky_pool_alloc();
const spooky_pool * spooky_pool_init(spooky_pool * /* self */);
const spooky_pool * spooky_pool_acquire();
const spooky_pool * spooky_pool_ctor(const spooky_pool * /* self */);
const spooky_pool * spooky_pool_dtor(const spooky_pool * /* self */);
void spooky_pool_free(const spooky_pool * /* self */);
void spooky_pool_release(const spooky_pool * /* self */);

#ifdef __cplusplus
}
#endif

#endif /* SP_POOL__H */

