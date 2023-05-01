#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

#include "../include/sp_error.h"

#define SPOOKY_EX_MAX_ALLOCS 2048

const errno_t SP_SUCCESS = 0;
const errno_t SP_FAILURE = !SP_SUCCESS;

//static spooky_ex * spooky_ex_allocated_exes[SPOOKY_EX_MAX_ALLOCS];
//static size_t spooky_ex_next_alloc = 0;

static spooky_ex spooky_ex_stack[SPOOKY_EX_MAX_ALLOCS] = { 0 };
static size_t spooky_ex_stack_len = 0;

int spooky_is_sdl_error(const char * msg) {
  /* SDL_GetError() will return an empty string if it hasn't been set */
  return msg != NULL && msg[0] != '\0';
}

errno_t spooky_ex_new(long line, const char * file, const int code, const char * msg, const spooky_ex * inner, const spooky_ex ** out_ex) {
  spooky_ex * temp = &(spooky_ex_stack[spooky_ex_stack_len++]);

  temp->code = code;
  temp->line = line;
  temp->file = file;
  temp->msg = msg;
  temp->inner = inner;

  *out_ex = temp;

  return SP_SUCCESS;
}

errno_t spooky_ex_print(const spooky_ex * ex) {
  if(ex) {
    int nest = 0;
    do {
      int i = nest;
      nest++;

      while(i-- > 0) {
        fprintf(stderr, "-> ");
      }

      fprintf(stderr, "%#010x: %s\n", ex->code, ex->msg);
    } while((ex = ex->inner));
  }

  return SP_SUCCESS;
}

