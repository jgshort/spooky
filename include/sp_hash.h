#ifndef SP_HASH__H
#define SP_HASH__H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include "sp_str.h"

typedef struct spooky_hash_table_impl spooky_hash_table_impl;

typedef struct spooky_hash_table spooky_hash_table;
typedef struct spooky_hash_table {
  const spooky_hash_table * (*ctor)(const spooky_hash_table * /* self */);
  const spooky_hash_table * (*dtor)(const spooky_hash_table * /* self */);
  void (*free)(const spooky_hash_table * /* self */);
  void (*release)(const spooky_hash_table * /* self */);

  errno_t (*ensure)(const spooky_hash_table * /* self */, const char * /* s */, size_t /* s_len */, spooky_str ** /* str */);
  errno_t (*find)(const spooky_hash_table * /* self */, const char * /* s */, size_t /* s_len */, spooky_str ** /* atom */);

  char * (*print_stats)(const spooky_hash_table * /* self */);
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

