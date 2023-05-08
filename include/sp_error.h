#pragma once

#ifndef SP_ERROR__H
#define SP_ERROR__H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

  /*! @file sp_error.h
   * @brief spooky error handling types and variables.
   *
   * @p sp_error defines the common spooky error types and values.
   */

#ifndef __STDC_LIB_EXT1__
  /** typedef for int.
   *
   * A typedef for the type int, used to self-document functions that return
   * errno values in the C standard library. Defined here if @a __STDC_LIB_EXT1__
   * is not defined, otherwise we use the definition included in errno.h.
   */
  typedef int errno_t;
#else
#include <errno.h>
#endif

  /** Return value indicating success. */
  extern const errno_t SP_SUCCESS;

  /** Return value indicating failure. */
  extern const errno_t SP_FAILURE;

  int sp_is_sdl_error(const char * /* msg */);

  typedef struct sp_ex sp_ex;
  typedef struct sp_ex {
    int code;
    char padding[4];
    long line;
    const char * file;
    const char * msg;
    const sp_ex * inner;
  } sp_ex;

  errno_t sp_ex_new(long line, const char * file, const int code, const char * msg, const sp_ex * inner, const sp_ex ** out_ex);
  errno_t sp_ex_print(const sp_ex * ex);

#ifdef __cplusplus
}
#endif

#endif /* SP_ERROR__H */

