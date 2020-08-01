#ifndef SP_STR__H
#define SP_STR__H

#ifdef __cplusplus
extern "C" {
#endif

#include "sp_error.h"

typedef struct spooky_str spooky_str;
typedef struct spooky_str {
  size_t len;
  size_t ref_count;
  unsigned long hash;
  const char * str;
  spooky_str * next;
  spooky_str * prev;
} spooky_str;

void spooky_str_init();
void spooky_str_quit();

void spooky_str_copy(spooky_str ** /* dest */, const spooky_str * /* src */);
void spooky_str_swap(spooky_str ** /* left */, spooky_str ** /* right */);
void spooky_str_list_swap(spooky_str * /* left */, spooky_str * /* right */);

unsigned long spooky_hash_str(const char * restrict /* s */, size_t /* s_len */);
errno_t spooky_str_ref(const char * /* s */, size_t /* len */, unsigned long /* hash */, spooky_str * /* out_str */);

unsigned long spooky_str_get_hash(const spooky_str * /* self */);
const char * spooky_str_get_str(const spooky_str * /* self */);
size_t spooky_str_get_ref_count(const spooky_str * /* self */);
void spooky_str_inc_ref_count(spooky_str * /* self */);
void spooky_str_dec_ref_count(spooky_str * /* self */);

errno_t spooky_str_isspace(int /* c */, bool * /* out_space */);
errno_t spooky_str_trim(const char * /* str */, size_t /* str_len */, size_t /* n_max */, char ** /* out_str */, size_t * /* out_str_len */);

int spooky_str_hash_compare(const void * a, const void * b);

#ifdef __cplusplus
}
#endif

#endif /* SP_STR__H */

