#include <stdlib.h>

#include "sp_error.h"

const errno_t SP_SUCCESS = 0;
const errno_t SP_FAILURE = !SP_SUCCESS;

int spooky_is_sdl_error(const char * msg) {
  /* SDL_GetError() will return an empty string if it hasn't been set */
  return msg != NULL && msg[0] != '\0';
}
