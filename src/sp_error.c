#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

#include "../include/sp_error.h"

#define sp_EX_MAX_ALLOCS 2048

const errno_t SP_SUCCESS = 0;
const errno_t SP_FAILURE = !SP_SUCCESS;

//static sp_ex * sp_ex_allocated_exes[sp_EX_MAX_ALLOCS];
//static size_t sp_ex_next_alloc = 0;

static sp_ex sp_ex_stack[sp_EX_MAX_ALLOCS] = { 0 };
static size_t sp_ex_stack_len = 0;

int sp_is_sdl_error(const char * msg) {
  /* SDL_GetError() will return an empty string if it hasn't been set */
  return msg != NULL && msg[0] != '\0';
}

errno_t sp_ex_new(long line, const char * file, const int code, const char * msg, const sp_ex * inner, const sp_ex ** out_ex) {
  sp_ex * temp = &(sp_ex_stack[sp_ex_stack_len++]);

  temp->code = code;
  temp->line = line;
  temp->file = file;
  temp->msg = msg;
  temp->inner = inner;

  *out_ex = temp;

  return SP_SUCCESS;
}

errno_t sp_ex_print(const sp_ex * ex) {
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

