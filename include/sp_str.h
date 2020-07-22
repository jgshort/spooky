#ifndef SP_STR__H
#define SP_STR__H

#ifdef __cplusplus
extern "C" {
#endif

#include "sp_error.h"

typedef struct spooky_str {
  size_t len;
  unsigned long hash;
  const char * str;
} spooky_str;

void spooky_str_init();
void spooky_str_quit();
 
unsigned long spooky_hash_str(const char * restrict /* str */);
errno_t spooky_str_ref(const char * s, size_t len, const spooky_str ** out_str, const spooky_ex ** ex);

#ifdef __cplusplus
}
#endif

#endif /* SP_STR__H */

