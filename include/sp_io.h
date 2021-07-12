#pragma once

#ifndef SPOOKY_IO__H
#define SPOOKY_IO__H

#ifdef __cplusplus
extern "C" {
#endif

#include <fcntl.h>

char * spooky_io_alloc_config_path();
char * spooky_io_alloc_concat_path(char const * /* root_path */, char const * /* path */);
void spooky_io_ensure_path(const char * /* path */, mode_t /* mode */);

FILE * spooky_io_open_binary_file_for_reading(const char * /* path */, int * /* fd_out */);
FILE * spooky_io_open_or_create_binary_file_for_writing(const char * /* path */, int * /* fd_out */);

#ifdef __cplusplus
}
#endif

#endif /* SPOOKY_IO__H */
