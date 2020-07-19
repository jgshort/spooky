#ifndef SP_STR__H
#define SP_STR__H

#ifdef __cplusplus
extern "C" {
#endif

#include "sp_error.h"

typedef struct spooky_str {
  char * str;
  size_t len;
  unsigned long hash;
} spooky_str;

unsigned long spooky_hash_str(const char * restrict /* str */);

errno_t spooky_str_alloc(const char * /* s */, size_t /* len */, spooky_str ** /* out_str */, const spooky_ex ** /* ex */);

#ifdef __cplusplus
}
#endif

#endif /* SP_STR__H */

