#include <assert.h>
#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <zlib.h>

#include "../include/sp_z.h"

#define SPOOKY_Z_CHUNK 16384

errno_t spooky_inflate_file(FILE * source, FILE * dest, size_t * dest_len) {
  static_assert(sizeof(int) == sizeof(errno_t), "Unexpected size delta in errno_t");
  assert(SP_SUCCESS == Z_OK);

  /* From: http://www.zlib.net/zlib_how.html */
/* Decompress from file source to file dest until stream ends or EOF.
   inf() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_DATA_ERROR if the deflate data is
   invalid or incomplete, Z_VERSION_ERROR if the version of zlib.h and
   the version of the library linked do not match, or Z_ERRNO if there
   is an error reading or writing the files. */

  int ret = 0;
  unsigned long have = 0;
  unsigned char in[SPOOKY_Z_CHUNK] = { 0 };
  unsigned char out[SPOOKY_Z_CHUNK] = { 0 };

  /* allocate inflate state */
  z_stream strm = {
    .zalloc = Z_NULL,
    .zfree = Z_NULL,
    .opaque = Z_NULL,
    .avail_in = 0,
    .next_in = Z_NULL
  };

  ret = inflateInit(&strm);
  if(ret != Z_OK) { return ret; }

  /* decompress until deflate stream ends or end of file */
  size_t written = 0;
  do {
    unsigned long read = fread(in, 1, SPOOKY_Z_CHUNK, source);
    assert(read <= UINT_MAX);
    if(read > UINT_MAX) {
      inflateEnd(&strm);
      return Z_ERRNO;
    }

    strm.avail_in = (unsigned int)read;
    if(ferror(source)) {
      inflateEnd(&strm);
      return Z_ERRNO;
    }

    if(strm.avail_in == 0) { break; }

    int flush = 0;
    strm.next_in = in;
    flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
    /* run inflate() on input until output buffer not full */
    do {
      strm.avail_out = SPOOKY_Z_CHUNK;
      strm.next_out = out;
      ret = inflate(&strm, flush);
      assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
      switch(ret) {
        case Z_NEED_DICT:
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
          inflateEnd(&strm);
          if(ret == Z_NEED_DICT) { ret = Z_DATA_ERROR; }
          if(ret == Z_MEM_ERROR) { fprintf(stderr, "Inflate memory error.\n"); }
          if(ret == Z_DATA_ERROR) { fprintf(stderr, "Inflate data read.\n"); }
          return ret;
        default:
          break;
      }

      have = SPOOKY_Z_CHUNK - strm.avail_out;
      size_t extracted = 0;
      if((extracted = fwrite(out, sizeof out[0], have, dest)) != have || ferror(dest)) {
        inflateEnd(&strm);
        return Z_ERRNO;
      }
      written += extracted;
    } while(strm.avail_out == 0);

    /* done when inflate() says it's done */
  } while(ret != Z_STREAM_END);

  if(dest_len) { *dest_len = written; }

  /* clean up and return */
  inflateEnd(&strm);
  return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

errno_t spooky_deflate_file(FILE * source, FILE * dest, size_t * dest_len) {
  assert(SP_SUCCESS == Z_OK);

  const int level = Z_DEFAULT_COMPRESSION;
  unsigned char in[SPOOKY_Z_CHUNK] = { 0 };
  unsigned char out[SPOOKY_Z_CHUNK] = { 0 };

  /* allocate deflate state */
  z_stream strm = { 0 };
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;

  int ret = deflateInit(&strm, level);
  if (ret != Z_OK) { return ret; }

  int flush = -1;
  unsigned int have = 0;

  size_t written = 0;
  do {
    unsigned long len = fread(in, 1, SPOOKY_Z_CHUNK, source);
    assert(len <= UINT_MAX);
    if(len > UINT_MAX) { goto err0; }
    strm.avail_in = (unsigned int)len;
    if(ferror(source)) { goto err0; }

    flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
    strm.next_in = in;
    do {
      strm.avail_out = SPOOKY_Z_CHUNK;
      strm.next_out = out;
      ret = deflate(&strm, flush);
      assert(ret != Z_STREAM_ERROR);
      have = SPOOKY_Z_CHUNK - strm.avail_out;
      if(fwrite(out, 1, have, dest) != have || ferror(dest)) {
        fprintf(stderr, "Deflate file write error.\n");
        goto err0;
      }
      written += have;
    } while(strm.avail_out == 0);
    assert(strm.avail_in == 0);
  } while(flush != Z_FINISH);
  assert(ret == Z_STREAM_END);

  /* clean up and return */
  deflateEnd(&strm);
  if(dest_len) { *dest_len = written; }

  return Z_OK;

err0:
  deflateEnd(&strm);
  return Z_ERRNO;
}
