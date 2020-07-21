#ifndef SP_ATOM__H
#define SP_ATOM__H

#ifdef __cplusplus
extern "C" {
#endif

#include "sp_str.h"

struct spooky_atom_impl;
typedef struct spooky_atom spooky_atom;
typedef struct spooky_atom {
  const spooky_atom * (*ctor)(const spooky_atom * /* self */, const char * /* str */);
  const spooky_atom * (*dtor)(const spooky_atom * /* self */);
  void (*free)(const spooky_atom * /* self */);
  void (*release)(const spooky_atom * /* self */);

  unsigned long (*get_id)(const spooky_atom * /* self */);
  unsigned long (*get_hash)(const spooky_atom * /* self */);
  size_t (*get_ref_count)(const spooky_atom * /* self */);
  void (*inc_ref_count)(const spooky_atom * /* self */);
  void (*dec_ref_count)(const spooky_atom * /* self */);
  const spooky_str * (*get_str)(const spooky_atom * /* self */);

  struct spooky_atom_impl * impl;
} spooky_atom; 

const spooky_atom * spooky_atom_init(spooky_atom * /* self */);
const spooky_atom * spooky_atom_alloc();
const spooky_atom * spooky_atom_acquire();
const spooky_atom * spooky_atom_ctor(const spooky_atom * /* self */, const char * /* str */);
const spooky_atom * spooky_atom_dtor(const spooky_atom * /* self */);
void spooky_atom_free(const spooky_atom * /* self */);
void spooky_atom_release(const spooky_atom * /* self */);

#ifdef __cplusplus
}
#endif

#endif /* SP_ATOM__H */

