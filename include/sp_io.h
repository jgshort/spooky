#pragma once

#ifndef SP_IO__H
#define SP_IO__H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <fcntl.h>

#include "./sp_error.h"

  char * spooky_io_alloc_config_path(void);
  char * spooky_io_alloc_concat_path(char const * /* root_path */, char const * /* path */);
  void spooky_io_ensure_path(const char * /* path */, mode_t /* mode */);

  FILE * spooky_io_open_binary_file_for_reading(const char * /* path */, int * /* fd_out */);
  FILE * spooky_io_open_or_create_binary_file_for_writing(const char * /* path */, int * /* fd_out */);

  errno_t spooky_io_read_buffer_from_file(const char * /* path */, char ** /* buffer */, const spooky_ex ** /* ex */);

#ifdef __cplusplus
}
#endif

#endif /* SP_IO__H */

