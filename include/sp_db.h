#ifndef SP_DB__H
#define SP_DB__H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct spooky_db spooky_db;
typedef struct spooky_db_data spooky_db_data;
typedef struct spooky_db {
  const spooky_db * (*ctor)(const spooky_db * /* self */);
  const spooky_db * (*dtor)(const spooky_db * /* self */);
  void (*free)(const spooky_db * /* self */);
  void (*release)(const spooky_db * /* self */);

  int (*open)(const spooky_db * /* self */, const char * /* context_name */);
  int (*close)(const spooky_db * /* self */);
  int (*create)(const char * /* context_name */);

  spooky_db_data * data;
} spooky_db;

const spooky_db * spooky_db_alloc();
const spooky_db * spooky_db_init(spooky_db * /* self */);
const spooky_db * spooky_db_acquire();
const spooky_db * spooky_db_ctor(const spooky_db * /* self */);
const spooky_db * spooky_db_dtor(const spooky_db * /* self */);

void spooky_db_free(const spooky_db * /* self */);
void spooky_db_release(const spooky_db * /* self */);

int spooky_db_create_storage(const char * /* context_name */);

#ifdef __cplusplus
}
#endif

#endif /* SP_DB__H */

