#ifndef SP_STR__H
#define SP_STR__H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include "sp_error.h"

typedef struct spooky_str {
  char * str;
  size_t len;
} spooky_str;

errno_t spooky_alloc_str(const char * /* s */, size_t /* len */, spooky_str ** /* out_str */, const spooky_ex ** /* ex */);
const char * spooky_strcpy(const char * /* start */, const char * /* end */, size_t * /* text_len */);

#ifdef __cplusplus
}
#endif

#endif /* SP_STR__H */

