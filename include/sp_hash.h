#ifndef SP_HASH__H
#define SP_HASH__H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include "sp_str.h"

typedef struct spooky_hash_table_impl spooky_hash_table_impl;
typedef struct spooky_atom spooky_atom;
struct spooky_atom_impl;

typedef struct spooky_atom {
	struct spooky_atom_impl * impl;
} spooky_atom;

typedef struct spooky_hash_table spooky_hash_table;
typedef struct spooky_hash_table {
  const spooky_hash_table * (*ctor)(const spooky_hash_table * /* self */);
  const spooky_hash_table * (*dtor)(const spooky_hash_table * /* self */);
  void (*free)(const spooky_hash_table * /* self */);
  void (*release)(const spooky_hash_table * /* self */);

  errno_t (*ensure)(const spooky_hash_table * /* self */, const char * /* str */, const spooky_atom ** /* atom */, unsigned long * /* hash */, unsigned long * /* bucket_index */);
  errno_t (*get_atom)(const spooky_hash_table * /* self */, const char * /* str */, spooky_atom ** /* atom */);
  errno_t (*find_by_id)(const spooky_hash_table * /* self */, int /* id */, const char ** /* str */);

  spooky_hash_table_impl * impl;
} spooky_hash_table;

const spooky_hash_table * spooky_hash_table_alloc();
const spooky_hash_table * spooky_hash_table_init(spooky_hash_table * /* self */);
const spooky_hash_table * spooky_hash_table_acquire();
const spooky_hash_table * spooky_hash_table_ctor(const spooky_hash_table * /* self */);
const spooky_hash_table * spooky_hash_table_dtor(const spooky_hash_table * /* self */);
void spooky_hash_table_free(const spooky_hash_table * /* self */);
void spooky_hash_table_release(const spooky_hash_table * /* self */);

#ifdef __cplusplus
}
#endif

#endif /* SP_HASH__H */

