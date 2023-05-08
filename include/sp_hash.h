#ifndef SP_HASH__H
#define SP_HASH__H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include "sp_str.h"

  typedef struct sp_hash_table_impl sp_hash_table_impl;
  typedef void (*sp_hash_free_item)(void * /* item */);

  typedef struct sp_hash_table sp_hash_table;
  typedef struct sp_hash_table {
    const sp_hash_table * (*ctor)(const sp_hash_table * /* self */);
    const sp_hash_table * (*dtor)(const sp_hash_table * /* self */, const sp_hash_free_item /* free_item_fn */);
    void (*free)(const sp_hash_table * /* self */);
    void (*release)(const sp_hash_table * /* self */, const sp_hash_free_item /* free_item_fn */);

    errno_t (*ensure)(const sp_hash_table * /* self */, const char * /* s */, size_t /* s_len */, void * /* value */, sp_str ** /* str */);
    errno_t (*find)(const sp_hash_table * /* self */, const char * /* s */, size_t /* s_len */, void ** /* value */);

    char * (*print_stats)(const sp_hash_table * /* self */);
    double (*get_load_factor)(const sp_hash_table * /* self */);

    size_t (*get_bucket_length)(const sp_hash_table * /* self */);
    size_t (*get_bucket_capacity)(const sp_hash_table * /* self */);
    size_t (*get_key_count)(const sp_hash_table * /* self */);

    sp_hash_table_impl * impl;
  } sp_hash_table;

  const sp_hash_table * sp_hash_table_alloc();
  const sp_hash_table * sp_hash_table_init(sp_hash_table * /* self */);
  const sp_hash_table * sp_hash_table_acquire();
  const sp_hash_table * sp_hash_table_ctor(const sp_hash_table * /* self */);
  const sp_hash_table * sp_hash_table_dtor(const sp_hash_table * /* self */, const sp_hash_free_item /* free_item_fn */);
  void sp_hash_table_free(const sp_hash_table * /* self */);
  void sp_hash_table_release(const sp_hash_table * /* self */, const sp_hash_free_item /* free_item_fn */);

#ifdef __cplusplus
}
#endif

#endif /* SP_HASH__H */

