#ifndef SP_Z__H
#define SP_Z__H

#ifdef __cplusplus
extern "C" {
#endif

#include "sp_error.h"

  /* From: http://www.zlib.net/zlib_how.html */
  /* This is an ugly hack required to avoid corruption of the input and output
   * data on Windows/MS-DOS systems. Without this, those systems would assume
   * that the input and output files are text, and try to convert the end-of-line
   * characters from one standard to another. That would corrupt binary data, and
   * in particular would render the compressed data unusable. This sets the input
   * and output to binary which suppresses the end-of-line conversions.
   * SET_BINARY_MODE() will be used later on stdin and stdout, at the beginning
   * of main():
   */
#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#include <fcntl.h>
#include <io.h>
#define SP_SET_BINARY_MODE(file) { setmode(fileno((file)), O_BINARY) }
#else
#define SP_SET_BINARY_MODE(file)
#endif /* >> if defined(MSDOS) || ... */

#include <stdio.h>

  errno_t spooky_inflate_file(FILE * /* source */, FILE * /* dest */, size_t * /* dest_len */);
  errno_t spooky_deflate_file(FILE * /* source */, FILE * /* dest */, size_t * /* dest_len */);

#ifdef __cplusplus
}
#endif

#endif /* SP_Z__H */

