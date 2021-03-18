#ifndef SP_DB__H
#define SP_DB__H

#ifdef __cplusplus
extern "C" {
#endif

#include "sp_types.h"

typedef struct spooky_db spooky_db;
typedef struct spooky_db_data spooky_db_data;
typedef struct spooky_db {
  const spooky_db * (*ctor)(const spooky_db * /* self */, const char * /* context_name */);
  const spooky_db * (*dtor)(const spooky_db * /* self */);
  void (*free)(const spooky_db * /* self */);
  void (*release)(const spooky_db * /* self */);

  int (*open)(const spooky_db * /* self */);
  int (*close)(const spooky_db * /* self */);
  int (*create)(const spooky_db * /* self */);
  int (*get_table_count)(const spooky_db * /* self */, const char * /* table_name */);
  int (*get_last_row_id)(const spooky_db * /* self */, const char * /* table_name */);

  int (*load_game)(const spooky_db * /* self */, const char * /* name */, spooky_save_game * /* state */);
  int (*save_game)(const spooky_db * /* self */, const char * /* name */, const spooky_save_game * /* state */);

  spooky_db_data * data;
} spooky_db;

const spooky_db * spooky_db_alloc();
const spooky_db * spooky_db_init(spooky_db * /* self */);
const spooky_db * spooky_db_acquire();
const spooky_db * spooky_db_ctor(const spooky_db * /* self */, const char * /* context_name */);
const spooky_db * spooky_db_dtor(const spooky_db * /* self */);

void spooky_db_free(const spooky_db * /* self */);
void spooky_db_release(const spooky_db * /* self */);

#ifdef __cplusplus
}
#endif

#endif /* SP_DB__H */
