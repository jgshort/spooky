#ifndef SP_STR__H
#define SP_STR__H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "sp_error.h"

  typedef struct sp_str {
    size_t len;
    uint64_t hash;
    const char * str;
  } sp_str;

  const char * sp_strcpy(const char * /* start */, const char * /* end */, size_t * /* text_len */);

  void sp_str_copy(sp_str ** /* dest */, const sp_str * /* src */);
  void sp_str_swap(sp_str ** /* left */, sp_str ** /* right */);
  uint64_t sp_hash_str(const char * restrict /* s */, size_t /* s_len */);

  errno_t sp_str_new(const char * /* s */, size_t /* len */, sp_str * /* out_str */);
  errno_t sp_str_ref(const char * /* s */, size_t /* len */, uint64_t /* hash */, sp_str * /* out_str */);

  uint64_t sp_str_get_hash(const sp_str * /* self */);
  const char * sp_str_get_str(const sp_str * /* self */);

  errno_t sp_str_isspace(int /* c */, bool * /* out_space */);
  errno_t sp_str_trim(const char * /* str */, size_t /* str_len */, size_t /* n_max */, char ** /* out_str */, size_t * /* out_str_len */);

  int sp_str_hash_compare(const void * /* a */, const void * /* b */);

  int sp_str_compare(const sp_str * /* left */, const sp_str * /* right */);

#ifdef __cplusplus
}
#endif

#endif /* SP_STR__H */

