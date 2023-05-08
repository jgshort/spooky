#ifndef SP_DB__H
#define SP_DB__H

#ifdef __cplusplus
extern "C" {
#endif

#include "sp_types.h"

  typedef struct sp_db sp_db;
  typedef struct sp_db_data sp_db_data;
  typedef struct sp_db {
    const sp_db * (*ctor)(const sp_db * /* self */, const char * /* context_name */);
    const sp_db * (*dtor)(const sp_db * /* self */);
    void (*free)(const sp_db * /* self */);
    void (*release)(const sp_db * /* self */);

    int (*open)(const sp_db * /* self */);
    int (*close)(const sp_db * /* self */);
    int (*create)(const sp_db * /* self */);
    int (*get_table_count)(const sp_db * /* self */, const char * /* table_name */);
    int (*get_last_row_id)(const sp_db * /* self */, const char * /* table_name */);

    int (*load_game)(const sp_db * /* self */, const char * /* name */, sp_save_game * /* state */);
    int (*save_game)(const sp_db * /* self */, const char * /* name */, const sp_save_game * /* state */);

    sp_db_data * data;
  } sp_db;

  const sp_db * sp_db_alloc();
  const sp_db * sp_db_init(sp_db * /* self */);
  const sp_db * sp_db_acquire();
  const sp_db * sp_db_ctor(const sp_db * /* self */, const char * /* context_name */);
  const sp_db * sp_db_dtor(const sp_db * /* self */);

  void sp_db_free(const sp_db * /* self */);
  void sp_db_release(const sp_db * /* self */);

#ifdef __cplusplus
}
#endif

#endif /* SP_DB__H */

