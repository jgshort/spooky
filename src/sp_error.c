#include <stdlib.h>

#include <errno.h>
#include "../include/sp_error.h"

const errno_t SP_SUCCESS = 0;
const errno_t SP_FAILURE = !SP_SUCCESS;

const spooky_ex spooky_null_ref_ex = {
  .msg = "NULL reference exception.",
  .inner = NULL
};

const spooky_ex spooky_alloc_ex = {
  .msg = "Memory allocation exception.",
  .inner = NULL
};

int spooky_is_sdl_error(const char * msg) {
  /* SDL_GetError() will return an empty string if it hasn't been set */
  return msg != NULL && msg[0] != '\0';
}
