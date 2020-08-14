#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sodium.h>
#include <assert.h>
#include <memory.h>
#include <zlib.h>
#include <sodium.h>

#include "sp_limits.h"
#include "sp_error.h"
#include "sp_pak.h"
#include "sp_math.h"

const unsigned long SPOOKY_CONTENT_OFFSET = 0x100;

const uint16_t SPOOKY_PACK_MAJOR_VERSION = 0;
const uint16_t SPOOKY_PACK_MINOR_VERSION = 0;
const uint16_t SPOOKY_PACK_REVISION_VERSION = 1;
const uint16_t SPOOKY_PACK_SUBREVISION_VERSION = 0;

/* A spooky pak file (SPDB) is a serialization format for SPOOKY resources:
 *
 *   An SPDB file starts and ends with the SPOOKY_HEADER and SPOOKY_FOOTER vectors,
 *   defined below with the following structure: 
 *
 *   | offset | size (bytes) | info              | data
 *   |   0x00 |           16 | spooky header     | { 0xf0, 0x9f, 0x8e, 0x83, 'SPOOKY!', 0xf0, 0x9f, 0x8e, 0x83, '\0' }
 *   |   0x10 |            8 | version (4x16bit) | { 0x0001, 0x0002, 0x0003, 0x0004 }
 *   |   0x1e |            8 | content length    | length of the pak file binary-encoded content excluding the header, version, hash, and footer  
 *   |   0x26 |           32 | content hash      | hash of the binary-encoded content 
 *   |   0x46 |            ? | content entries   | binary-encoded content 
 *   |    EOF |           16 | spooky footer     | { 0xf0, 0x9f, 0x8e, 0x83, '!YKOOPS', 0xf0, 0x9f, 0x8e, 0x83, '\0' }
 * 
 * Data saved in little endian format 
 *
 * unpack example: 
 *  i = (data[0]<<0) | (data[1]<<8) | (data[2]<<16) | (data[3]<<24);
*/

#define SPOOKY_HEADER_LEN 16
#define SPOOKY_FOOTER_LEN 16

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
#define SPOOKY_SET_BINARY_MODE(file) { setmode(fileno((file)), O_BINARY) }
#else
#define SPOOKY_SET_BINARY_MODE(file)
#endif /* >> if defined(MSDOS) || ... */

static const size_t MAX_PACK_STRING_LEN = 4096;
static const unsigned char SPOOKY_PUMPKIN[4] = { 0xf0, 0x9f, 0x8e, 0x83 };

static const unsigned char SPOOKY_HEADER[SPOOKY_HEADER_LEN] = { 0xf0, 0x9f, 0x8e, 0x83, 'S', 'P', 'O', 'O', 'K', 'Y', '!', 0xf0, 0x9f, 0x8e, 0x83, '\0' };
static const unsigned char SPOOKY_FOOTER[SPOOKY_FOOTER_LEN] = { 0xf0, 0x9f, 0x8e, 0x83, '!', 'Y', 'K', 'O', 'O', 'P', 'S', 0xf0, 0x9f, 0x8e, 0x83, '\0' };

static const uint64_t SPOOKY_ITEM_MAGIC = 0x00706b6e616e6d65;

static errno_t spooky_deflate_file(FILE * source, FILE * dest, size_t * dest_len);

static void spooky_pack_dump_hash(FILE * fp, const unsigned char * c, size_t len) {
  assert(fp && c && len > 0);
  for(size_t i = 0; i < len; i++) {
    fprintf(fp, "%x", c[i]);
  }
}

typedef enum spooky_pack_item_type /* unsigned char */ {
  spit_unspecified = 0,
  spit_null = 1,
/**/ spit_bool = 2,

/**/ spit_byte = 10, /* char  */
/**/ spit_uint8 = 20,
/**/ spit_uint16 = 21,
/**/ spit_uint32 = 22,
/**/ spit_uint64 = 23,
 
/**/ spit_int8 = 30,
/**/ spit_int16 = 31,
/**/ spit_int32 = 32,
/**/ spit_int64 = 33,

/**/ spit_string = 40,
  spit_array = 41,

  spit_bin_file = 76,

  spit_signature = 80,
  spit_pubkey = 81,
  spit_hash = 82,

/**/ spit_float = 90,
  spit_double = 91,

  spit_bits128 = 95,
  spit_bits256 = 96,

/**/ spit_raw = 128,

  spit_eof = UCHAR_MAX 
} spooky_pack_item_type;

typedef struct spooky_pack_version {
  uint16_t major;
  uint16_t minor;
  uint16_t revision;
  uint16_t subrevision;
} spooky_pack_version;

typedef struct spooky_pack_file {
  unsigned char header[SPOOKY_HEADER_LEN];
  spooky_pack_version version;
  uint64_t content_len;
  unsigned char hash[crypto_generichash_BYTES];
} spooky_pack_file;

/* Writers */
static bool spooky_write_raw(void * value, size_t len, FILE * fp);
static bool spooky_write_item_type(spooky_pack_item_type type, FILE * fp, uint64_t * content_len);
static bool spooky_write_file(const char * file_path, const char * key, FILE * fp, uint64_t * content_len);
static bool spooky_write_char(char value, FILE * fp, uint64_t * content_len);
static bool spooky_write_uint8(uint8_t value, FILE * fp, uint64_t * content_len);
static bool spooky_write_int8(int8_t value, FILE * fp, uint64_t * content_len);
static bool spooky_write_uint16(uint16_t value, FILE * fp, uint64_t * content_len);
static bool spooky_write_int16(int16_t value, FILE * fp, uint64_t * content_len);
static bool spooky_write_uint32(uint32_t value, FILE * fp, uint64_t * content_len);
static bool spooky_write_int32(int32_t value, FILE * fp, uint64_t * content_len);
static bool spooky_write_uint64(uint64_t value, FILE * fp, uint64_t * content_len);
static bool spooky_write_int64(int64_t value, FILE * fp, uint64_t * content_len);
static bool spooky_write_bool(bool value, FILE * fp, uint64_t * content_len);
static bool spooky_write_float(float value, FILE * fp, uint64_t * content_len);
static bool spooky_write_string(const char * value, FILE * fp, uint64_t * content_len);
static bool spooky_write_fixed_width_string(const char * value, size_t fixed_width, FILE * fp, uint64_t * content_len);
static bool spooky_write_version(spooky_pack_version version, FILE * fp, uint64_t * content_len);

static bool spooky_write_hash(const unsigned char * buf, size_t buf_len, FILE * fp, uint64_t * content_len);

/* Readers */
static bool spooky_read_item_type(FILE * fp, spooky_pack_item_type * item_type);
static bool spooky_read_raw(FILE * fp, size_t len, void * buf);
static bool spooky_read_uint8(FILE * fp, uint8_t * value);
static bool spooky_read_int8(FILE * fp, int8_t * value);
static bool spooky_read_uint16(FILE * fp, uint16_t * value);
static bool spooky_read_int16(FILE * fp, int16_t * value);
static bool spooky_read_uint32(FILE * fp, uint32_t * value);
static bool spooky_read_int32(FILE * fp, int32_t * value);
static bool spooky_read_uint64(FILE * fp, uint64_t * value);
static bool spooky_read_int64(FILE * fp, int64_t * value);
static bool spooky_read_bool(FILE * fp, bool * value);
/* static bool spooky_read_float(FILE * fp, float * value); */
static bool spooky_read_string(FILE * fp, char ** value, size_t * value_len);
static bool spooky_read_header(FILE * fp);
static bool spooky_read_version(FILE * fp, spooky_pack_version *version);
static bool spooky_read_footer(FILE * fp);

static bool spooky_write_raw(void * value, size_t len, FILE * fp) {
  assert(fp && value);
  assert(len > 0);

  size_t res = fwrite(value, sizeof(unsigned char), len, fp);
  if(res != len) { abort(); }
  assert(ferror(fp) == 0);

  assert((size_t)res == sizeof(unsigned char) * len);
  return ((size_t)res == sizeof(unsigned char) * len) && ferror(fp) == 0;
}

static bool spooky_read_raw(FILE * fp, size_t len, void * buf) {
	assert(fp != NULL && buf != NULL);
  assert(len > 0);

  if(feof(fp) != 0) return false;
	size_t hir = fread(buf, sizeof(unsigned char), len, fp);
	assert(hir);

  return hir > 0 && ferror(fp) == 0;
}

static bool spooky_write_hash(const unsigned char * buf, size_t buf_len, FILE * fp, uint64_t * content_len) {
  unsigned char hash[crypto_generichash_BYTES] = { 0 };

  if(!spooky_write_item_type(spit_hash, fp, content_len)) { abort(); }
  /* generate hash from buffer: */
  crypto_generichash(hash, crypto_generichash_BYTES, buf, buf_len, NULL, 0);
  
  /* write generated hash to fp: */
  bool res = spooky_write_raw(hash, crypto_generichash_BYTES, fp);
  if(res) {
    if(content_len != NULL) { (*content_len) += crypto_generichash_BYTES; }
  }

  return res;
}

static bool spooky_read_hash(FILE * fp, unsigned char * buf, size_t buf_len) {
  assert(buf && buf_len == crypto_generichash_BYTES);
  spooky_pack_item_type type = spit_unspecified;
  spooky_read_item_type(fp, &type);

  assert(type == spit_hash);
  if(type != spit_hash) { abort(); }

  assert(buf_len == crypto_generichash_BYTES);
  
  return spooky_read_raw(fp, buf_len, buf);
}

static bool spooky_write_item_type(spooky_pack_item_type type, FILE * fp, uint64_t * content_len) {
	assert(fp != NULL);
  assert(type >= spit_unspecified && type <= spit_eof);
  assert(type >= 0 && type <= UCHAR_MAX);
  assert(type >= 0);
  return spooky_write_uint8((unsigned char)type, fp, content_len);
}

static bool spooky_read_item_type(FILE * fp, spooky_pack_item_type * type) {
  unsigned char value = 0;
  bool res = spooky_read_uint8(fp, &value);
  assert(res);
  *type = (spooky_pack_item_type)value;
  return res;
}

int spooky_inflate_file(FILE * source, FILE * dest, size_t * dest_len) {
/* From: http://www.zlib.net/zlib_how.html */
/* Decompress from file source to file dest until stream ends or EOF.
   inf() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_DATA_ERROR if the deflate data is
   invalid or incomplete, Z_VERSION_ERROR if the version of zlib.h and
   the version of the library linked do not match, or Z_ERRNO if there
   is an error reading or writing the files. */

#define CHUNK 16384

  int ret = 0;
  unsigned long have = 0;
  unsigned char in[CHUNK] = { 0 };
  unsigned char out[CHUNK] = { 0 };

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
    unsigned long read = fread(in, 1, CHUNK, source);
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
      strm.avail_out = CHUNK;
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
      
      have = CHUNK - strm.avail_out;
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
#undef CHUNK
}

errno_t spooky_deflate_file(FILE * source, FILE * dest, size_t * dest_len) {
#define CHUNK 16384

  const int level = Z_DEFAULT_COMPRESSION;
  unsigned char in[CHUNK] = { 0 };
  unsigned char out[CHUNK] = { 0 };

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
    unsigned long len = fread(in, 1, CHUNK, source);
    assert(len <= UINT_MAX);
    if(len > UINT_MAX) { goto err0; }
    strm.avail_in = (unsigned int)len;
    if(ferror(source)) { goto err0; }
      
    flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
    strm.next_in = in;
    do {
      strm.avail_out = CHUNK;
      strm.next_out = out;
      ret = deflate(&strm, flush);
      assert(ret != Z_STREAM_ERROR);
      have = CHUNK - strm.avail_out;
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
#undef CHUNK
#undef SET_BINARY_MODE 
}

static bool spooky_read_string(FILE * fp, char ** value, size_t * value_len) {
  assert(fp && value && value_len);

  uint64_t len = 0;
  if(!spooky_read_uint64(fp, &len)) { return false; }
  assert(len <= SIZE_MAX);
  assert(len <= SPOOKY_MAX_STRING_LEN);
  if(len >= SPOOKY_MAX_STRING_LEN) { abort(); }

  *value = NULL;
  *value_len = (size_t)len;
  if(len > 0) { 
    *value = calloc(len, sizeof ** value);
    assert(*value);
    if(!*value) { abort(); }
    if(!spooky_read_raw(fp, len + 1 /* NULL terminator */, *value)) { return false; }
  }

  return true;
}

static bool spooky_read_file(FILE * fp, char ** buf, size_t * buf_len) {
  assert(fp);
  
  /* READ TYPE: (assert spit_bin_file)
   * READ DECOMPRESSED LEN (uint64_t)
   * READ COMPRESSED LEN (uint64_t)
   * READ DECOMPRESSED CONTENT HASH
   * READ COMPRESSED CONTENT HASH
   * READ FILE PATH
   * READ KEY
   * READ CONTENT
   */

  unsigned char decompressed_hash[crypto_generichash_BYTES] = { 0 };
  unsigned char compressed_hash[crypto_generichash_BYTES] = { 0 };
  unsigned char read_compressed_hash[crypto_generichash_BYTES] = { 0 };
  unsigned char read_decompressed_hash[crypto_generichash_BYTES] = { 0 };

  spooky_pack_item_type type = spit_unspecified;
  /* write the type (bin-file) to the stream: */
  if(!spooky_read_item_type(fp, &type)) { return false; }
  assert(type == spit_bin_file);
  if(type != spit_bin_file) { return false; }

  uint64_t decompressed_len = 0, compressed_len = 0;

  if(!spooky_read_uint64(fp, &decompressed_len)) { return false; }
  if(!spooky_read_uint64(fp, &compressed_len)) { return false; }

  /* READ DECOMPRESSED CONTENT HASH */
  if(!spooky_read_hash(fp, decompressed_hash, crypto_generichash_BYTES)) { return false; };

  /* READ COMPRESSED CONTENT HASH */
  if(!spooky_read_hash(fp, compressed_hash, crypto_generichash_BYTES)) { return false; };

  /* READ FILE PATH */
  char * file_path = NULL; /* need to free */
  size_t file_path_len = 0;
  if(!spooky_read_string(fp, &file_path, &file_path_len)) { return false; }

  /* READ KEY */
  char * key = NULL; /* need to free */
  size_t key_len = 0;
  if(!spooky_read_string(fp, &key, &key_len)) { return false; }

  /* READ CONTENT */
  unsigned char * compressed_data = calloc(compressed_len, sizeof * compressed_data);
  if(!compressed_data) { abort(); }
  unsigned char * compressed_data_head = compressed_data;
  
  if(!spooky_read_raw(fp, compressed_len, compressed_data)) {
    free(compressed_data), compressed_data = NULL;
    return false;
  }
  
  crypto_generichash(read_compressed_hash, crypto_generichash_BYTES, compressed_data_head, compressed_len, NULL, 0);

  for(size_t i = 0; i < crypto_generichash_BYTES; i++) {
    if(read_compressed_hash[i] != compressed_hash[i]) { 
      fprintf(stderr, "Failed to verify compressed content hash.\n");
      return false;
    }
  }

  unsigned char * decompressed_data = calloc(decompressed_len, sizeof * decompressed_data);
  if(!decompressed_data) { 
    free(compressed_data), compressed_data = NULL;
    abort();
  }
  unsigned char * decompressed_data_head = decompressed_data;

  FILE * deflated_fp = fmemopen(compressed_data, compressed_len, "rb");
  SPOOKY_SET_BINARY_MODE(deflated_fp);

  {
    FILE * inflated_fp = fmemopen(decompressed_data, decompressed_len, "rb+");
    SPOOKY_SET_BINARY_MODE(inflated_fp);
    size_t inflated_buf_len = 0;
    
    if(spooky_inflate_file(deflated_fp, inflated_fp, &inflated_buf_len) != Z_OK) { 
      fprintf(stderr, "Failed to inflate [%s] at '%s' (%lu, %lu) <", key, file_path, (size_t)compressed_len, (size_t)decompressed_len);
      spooky_pack_dump_hash(stderr, compressed_hash, sizeof compressed_hash);
      fprintf(stderr, ">\n");
      fflush(stderr);
      abort();
    }

    fseek(inflated_fp, 0, SEEK_SET);
    assert(inflated_buf_len == decompressed_len);

    crypto_generichash(read_decompressed_hash, crypto_generichash_BYTES, decompressed_data_head, decompressed_len, NULL, 0);

    for(size_t i = 0; i < sizeof read_decompressed_hash / sizeof read_decompressed_hash[0]; i++) {
      if(read_decompressed_hash[i] != decompressed_hash[i]) {
        fprintf(stderr, "Failed to verify decompressed content hash.\n");
        return false;
      }
    }

    fseek(inflated_fp, 0, SEEK_SET);
    char * decompressed_data_copy = calloc(decompressed_len, sizeof * decompressed_data_copy );
    if(!decompressed_data_copy) { abort(); }
    memmove(decompressed_data_copy, decompressed_data, decompressed_len);

    *buf = decompressed_data_copy;
    *buf_len = decompressed_len;

    fclose(inflated_fp);
  } 
  fclose(deflated_fp);

  free(compressed_data), compressed_data = NULL;
  free(decompressed_data), decompressed_data = NULL;

  return true;
}

static bool spooky_write_file(const char * file_path, const char * key, FILE * fp, uint64_t * content_len) {
  assert(file_path != NULL && fp != NULL);
  
  unsigned char * inflated_buf = NULL;
  unsigned char * deflated_buf = NULL;

  FILE * src_file = fopen(file_path, "rb");
  SPOOKY_SET_BINARY_MODE(src_file);
  if(src_file != NULL) {
    if(fseek(src_file, 0L, SEEK_END) == 0) {
      long inflated_buf_len = ftell(src_file);
      if(inflated_buf_len == -1) { abort(); }
      assert(inflated_buf_len < LONG_MAX);

      inflated_buf = calloc((size_t)inflated_buf_len, sizeof * inflated_buf);
      if(!inflated_buf) { abort(); }
      deflated_buf = calloc((size_t)inflated_buf_len, sizeof * deflated_buf);
      if(!deflated_buf) { abort(); }

      if(fseek(src_file, 0L, SEEK_SET) != 0) { abort(); }

      size_t new_len = fread(inflated_buf, sizeof(unsigned char), (size_t)inflated_buf_len, src_file);
      assert((size_t)new_len == (size_t)inflated_buf_len);
      if(ferror(src_file) != 0) {
        abort();
      } else {
        {
          FILE * inflated_fp = fmemopen(inflated_buf, new_len, "rb");
          SPOOKY_SET_BINARY_MODE(inflated_fp);
          fseek(inflated_fp, 0, SEEK_SET);
          {
            unsigned char * deflated_buf_head = deflated_buf;
            FILE * deflated_fp = fmemopen(deflated_buf, new_len, "rb+");
            SPOOKY_SET_BINARY_MODE(deflated_fp);
            fseek(deflated_fp, 0, SEEK_SET);
            size_t deflated_buf_len = 0;
           
            /* compress the file: */
            if(spooky_deflate_file(inflated_fp, deflated_fp, &deflated_buf_len) != Z_OK) { abort(); };
            
            /* File format: 
             * - Type (spit_bin_file)
             * - Decompressed len (uint64_t)
             * - Compressed len (uint64_t)
             * - Decompressed content hash
             * - Compresed content hash
             * - Path (string)
             * - Key (string)
             * - Content
             */

            /* TYPE: */
            /* write the type (bin-file) to the stream: */
            spooky_write_item_type(spit_bin_file, fp, content_len);

            /* DECOMPRESSED SIZE: */
            /* write the decompressed (inflated) file size to the stream */
            spooky_write_uint64(new_len, fp, content_len);

            /* COMPRESSED SIZE: */
            /* write the compressed file length to the stream: */
            spooky_write_uint64(deflated_buf_len, fp, content_len);

            fseek(deflated_fp, 0, SEEK_SET);

            /* DECOMPRESSED HASH: */
            spooky_write_hash(inflated_buf, new_len, fp, content_len);
           

            /* COMPRESSED HASH: */
            spooky_write_hash(deflated_buf_head, (size_t)deflated_buf_len, fp, content_len);

            /* FILE PATH: */
            spooky_write_string(file_path, fp, content_len);
           
            /* KEY: */
            spooky_write_string(key, fp, content_len);

            /* CONTENT */
            fseek(deflated_fp, 0, SEEK_SET);
            spooky_write_raw(deflated_buf_head, deflated_buf_len, fp);
            if(content_len != NULL) { *content_len += deflated_buf_len; }

            fclose(deflated_fp);
          }
          fclose(inflated_fp);
        }
      }
    }
    fclose(src_file);
  }

  free(inflated_buf), inflated_buf = NULL;
  free(deflated_buf), deflated_buf = NULL;

  return true;
}

static bool spooky_write_char(char value, FILE * fp, uint64_t * content_len) {
  if(content_len != NULL) { *content_len += sizeof value; }
  return spooky_write_raw(&value, sizeof value, fp);
}

static bool spooky_write_uint8(uint8_t value, FILE * fp, uint64_t * content_len) {
  if(content_len != NULL) { *content_len += sizeof value; }
  return spooky_write_raw(&value, sizeof value, fp);
}

static bool spooky_write_int8(int8_t value, FILE * fp, uint64_t * content_len) {
  if(content_len != NULL) { *content_len += sizeof value; }
  return spooky_write_raw(&value, sizeof value, fp);
}

static bool spooky_write_uint16(uint16_t value, FILE * fp, uint64_t * content_len) {
  if(content_len != NULL) { *content_len += sizeof value; }
  uint8_t bytes[sizeof value] = { 0 };
  
  /* little endian */
  bytes[0] = (uint8_t)(value & 0x00ff);
  bytes[1] = (uint8_t)(value >> 8);

  return spooky_write_raw(bytes, sizeof value, fp);
}

static bool spooky_write_int16(int16_t value, FILE * fp, uint64_t * content_len) {
  if(content_len != NULL) { *content_len += sizeof value; }
  
  int8_t bytes[sizeof value] = { 0 };

  /* little endian */
  bytes[0] = (int8_t)(value & 0x00ff);
  bytes[1] = (int8_t)(value >> 8);

  return spooky_write_raw(bytes, sizeof value, fp);
}

static bool spooky_write_uint32(uint32_t value, FILE * fp, uint64_t * content_len) {
  if(content_len != NULL) { *content_len += sizeof value; }
  uint16_t lo = value & 0xffff;
  uint16_t hi = (uint16_t)((value >> 16) & 0xffff);

  /* little endian */
  return spooky_write_raw(&lo, sizeof lo, fp)
    && spooky_write_raw(&hi, sizeof hi, fp);
}

static bool spooky_write_int32(int32_t value, FILE * fp, uint64_t * content_len) {
  if(content_len != NULL) { *content_len += sizeof value; }
  int16_t lo = (int16_t)(value & 0xffff);
  int16_t hi = (int16_t)((value >> 16) & 0xffff);

  /* little endian */
  return spooky_write_raw(&lo, sizeof lo, fp)
    && spooky_write_raw(&hi, sizeof hi, fp);
}

static bool spooky_write_uint64(uint64_t value, FILE * fp, uint64_t * content_len) {
  if(content_len != NULL) { *content_len += sizeof value; }
  uint32_t lo = value & 0xffffffff;
  uint32_t hi =(uint32_t)((value >> 32) & 0xffffffff);
 
  /* little endian */
  return spooky_write_raw(&lo, sizeof lo, fp)
    && spooky_write_raw(&hi, sizeof hi, fp);
}

static bool spooky_write_int64(int64_t value, FILE * fp, uint64_t * content_len) {
  if(content_len != NULL) { *content_len += sizeof value; }
  int32_t lo = (int32_t)(value & 0xffffffff);
  int32_t hi = (int32_t)((value >> 32) & 0xffffffff);

  /* little endian */
  return spooky_write_raw(&lo, sizeof lo, fp)
    && spooky_write_raw(&hi, sizeof hi, fp);
}

static bool spooky_write_bool(bool value, FILE * fp, uint64_t * content_len) {
  static const uint32_t spooky_true = 0x11111111;
  static const uint32_t spooky_false = 0x00000000;

  return spooky_write_uint32(value ? spooky_true : spooky_false, fp, content_len);
}

static bool spooky_read_bool(FILE * fp, bool * value) {
  static const uint32_t spooky_true = 0x11111111;

  uint32_t b;
  bool res = spooky_read_uint32(fp, &b);
  assert(res);
  
  *value = b == spooky_true;

  return res;
}

static bool spooky_write_string(const char * value, FILE * fp, uint64_t * content_len) {
  static const char * empty = "";
  static const char nullstr = '\0';

  /* String Format:
   * - Len (size_t)
   * - String (null-terminated)
   */
  int expected = 0;
  size_t res = 0, len = 0;
  if(value) {
    len = strnlen(value, MAX_PACK_STRING_LEN);
    res += fwrite(&len, sizeof(char), sizeof len, fp);
    if(ferror(fp) != 0) return false;
    
    expected += (int)((sizeof len) * 1);
    if(len > 0) {
      res += fwrite(value, sizeof(char), len, fp);
      if(ferror(fp) != 0) return false;

      expected += (int)(sizeof(char) * len);
    } else {
      res += fwrite(empty, sizeof(char), len, fp);
      if(ferror(fp) != 0) return false;

      expected += (int)(sizeof(char) * len);
    }
  } else {
    res += fwrite(&len, sizeof(char), len, fp);
    if(ferror(fp) != 0) return false;

    expected += (int)((sizeof(char) * len));
  }
  res += fwrite(&nullstr, sizeof(char), sizeof nullstr, fp);
  if(ferror(fp) != 0) return false;

  expected += (int)((sizeof nullstr) * 1);
  assert(res == (size_t)expected);

  if(content_len != NULL) { *content_len += (uint64_t)expected; }

  return res == (size_t)expected && ferror(fp) == 0;
}

static bool spooky_write_fixed_width_string(const char * value, size_t fixed_width, FILE * fp, uint64_t * content_len) {
  static char nullstr = '\0';

 	assert(fixed_width > 0 && fixed_width <= MAX_PACK_STRING_LEN);
	assert(value);

	char * buf = malloc(fixed_width * sizeof(char));
  if(!buf) goto err0;

	memset(buf, 0, fixed_width * sizeof(char));
	size_t len = strnlen(value, MAX_PACK_STRING_LEN);
	size_t min = (fixed_width < len ? fixed_width : len);
	memcpy(buf, value, min * sizeof(char));

  size_t rmin = 0, rbuf = 0, rnull = 0;

  rmin = fwrite(&min, sizeof(char), sizeof min, fp);
  if(rmin != (sizeof min) * 1) goto err1;

	rbuf = fwrite(buf, sizeof(char), min, fp);	
  if((size_t)rbuf != sizeof(char) * min) goto err1;

	rnull =  fwrite(&nullstr, sizeof nullstr, 1, fp);	
  if(rnull != 1) goto err1;
  
  free(buf), buf = NULL;

  if(content_len != NULL) { *content_len += rmin + rbuf + rnull; }

  assert((size_t)(rmin + rbuf + rnull) == (sizeof min) + (sizeof(char)) * min + (1));
  
  return true;

err1:
  free(buf), buf = NULL;

err0:
  return false;
}

static bool spooky_write_float(float value, FILE * fp, uint64_t * content_len) {
#define BUF_MAX 256 
  char buf[BUF_MAX] = { 0 };
  int len = snprintf((char *)buf, BUF_MAX - 1, "%.9g", value); 
  assert(len > 0 && len < BUF_MAX);
#undef BUF_MAX
  buf[len] = '\0';

  return spooky_write_string(buf, fp, content_len);
}

static bool spooky_read_uint8(FILE * fp, uint8_t * value) {
  if(feof(fp) != 0) return false;

	uint8_t byte;
  size_t hir = fread(&byte, sizeof byte, 1, fp);
  assert(hir == sizeof *value);

	*value = byte;
 
  return hir == sizeof *value && ferror(fp) == 0;
}

static bool spooky_read_int8(FILE * fp, int8_t * value) {
  int8_t byte;

  bool res = spooky_read_uint8(fp, (uint8_t *)&byte);
  assert(res);

  *value = byte;

  return res;
}

static bool spooky_read_uint16(FILE * fp, uint16_t * value) {
  if(feof(fp) != 0) return false;

  uint8_t bytes[sizeof(uint16_t)] = { 0 } ;
  size_t hir = fread(&bytes, sizeof bytes, 1, fp);
  assert(hir);
  
  /* little endian */
  *value = (uint16_t)(((uint16_t)(bytes[0] << 0)) | (uint16_t)(bytes[1] << 8));

  return ferror(fp) == 0;
}

static bool spooky_read_int16(FILE * fp, int16_t * value) {
  if(feof(fp) != 0) return false;

  int8_t bytes[sizeof(int16_t)] = { 0 } ;
  size_t hir = fread(&bytes, sizeof bytes, 1, fp);
  assert(hir);
  
  /* little endian */
  *value = (int16_t)(((int16_t)bytes[0] << 0) | ((int16_t)bytes[1] << 8));

  return ferror(fp) == 0;
}

static bool spooky_read_uint32(FILE * fp, uint32_t * value) {
	if(feof(fp) != 0) return false;

  uint8_t bytes[sizeof(uint32_t)] = { 0 } ;
  size_t hir = fread(&bytes, sizeof bytes, 1, fp);
  assert(hir);
  
  /* little endian */
  *value = (uint32_t)((uint32_t)(bytes[0] << 0)) | ((uint32_t)(bytes[1] << 8)) | (uint32_t)(bytes[2] << 16) | (uint32_t)(bytes[3] << 24);

  return ferror(fp) == 0;
}

static bool spooky_read_int32(FILE * fp, int32_t * value) {
	if(feof(fp) != 0) return false;

  int8_t bytes[sizeof(int32_t)] = { 0 } ;
  size_t hir = fread(&bytes, sizeof bytes, 1, fp);
  assert(hir);
  
  /* little endian */
  *value = (bytes[0] << 0) | (bytes[1] << 8) | bytes[2] << 16 | bytes[3] << 24;

  return ferror(fp) == 0;
}

static bool spooky_read_uint64(FILE * fp, uint64_t * value) {
	if(feof(fp) != 0) return false;

  uint8_t bytes[sizeof(uint64_t)] = { 0 } ;
  size_t hir = fread(&bytes, sizeof(uint8_t), sizeof * value, fp);
  assert(hir == sizeof(uint8_t) * sizeof * value);
  
  /* little endian */
  *value = ((uint64_t)bytes[0] <<  0) | ((uint64_t)bytes[1] <<  8) | ((uint64_t)bytes[2] << 16) | ((uint64_t)bytes[3] << 24)
				 | ((uint64_t)bytes[4] << 32) | ((uint64_t)bytes[5] << 40) | ((uint64_t)bytes[6] << 48) | ((uint64_t)bytes[7] << 56);

  return ferror(fp) == 0;
}

static bool spooky_read_int64(FILE * fp, int64_t * value) {
	if(feof(fp) != 0) return false;

  uint8_t bytes[sizeof *value] = { 0 } ;
  size_t hir = fread(&bytes, sizeof(uint8_t), sizeof *value,  fp);
  assert(hir == sizeof(uint8_t) * sizeof *value);
  
  /* little endian */
  *value = ((int64_t)bytes[0] <<  0) | ((int64_t)bytes[1] <<  8) | ((int64_t)bytes[2] << 16) | ((int64_t)bytes[3] << 24)
				 | ((int64_t)bytes[4] << 32) | ((int64_t)bytes[5] << 40) | ((int64_t)bytes[6] << 48) | ((int64_t)bytes[7] << 56);

  return ferror(fp) == 0;
}

static bool spooky_read_header(FILE * fp) {
  unsigned char header[SPOOKY_HEADER_LEN] = { 0 };

  if(feof(fp) != 0) { return false; }

  size_t r = fread(&header, sizeof(unsigned char), SPOOKY_HEADER_LEN, fp);
  assert(r == (sizeof header) * 1);
  
  int eq = strncmp((const char *)header, (const char *)SPOOKY_HEADER, SPOOKY_HEADER_LEN) == 0;
  assert(eq);

  return eq;
}

static bool spooky_read_version(FILE * fp, spooky_pack_version *version) {
    if(feof(fp) != 0) return false;

    if(!spooky_read_uint16(fp, &(version->major))) return false;
    if(!spooky_read_uint16(fp, &(version->minor))) return false;
    if(!spooky_read_uint16(fp, &(version->revision))) return false;
    if(!spooky_read_uint16(fp, &(version->subrevision))) return false;

    return true;
}

static bool spooky_write_version(spooky_pack_version version, FILE * fp, uint64_t * content_len) {
    if(!spooky_write_uint16(version.major, fp, content_len)) return false;
    if(!spooky_write_uint16(version.minor, fp, content_len)) return false;
    if(!spooky_write_uint16(version.revision, fp, content_len)) return false;
    if(!spooky_write_uint16(version.subrevision, fp, content_len)) return false;

    return true;
}

static bool spooky_read_footer(FILE * fp) {
  unsigned char footer[SPOOKY_FOOTER_LEN] = { 0 };

  size_t r = fread(&footer, sizeof(unsigned char), SPOOKY_FOOTER_LEN, fp);
  fflush(stdout);
  
  assert(r == (sizeof footer) * 1);
  
  fflush(stdout);
  
  bool eq = strncmp((const char *)footer, (const char *)SPOOKY_FOOTER, sizeof SPOOKY_FOOTER) == 0;
  assert(eq);

  return eq;
}

bool spooky_pack_create(FILE * fp) {
  const unsigned char * P = SPOOKY_PUMPKIN;
  SPOOKY_SET_BINARY_MODE(fp);
  spooky_pack_file spf = {
    .header = { P[0], P[1], P[2], P[3], 'S', 'P', 'O', 'O', 'K', 'Y', '!', P[0], P[1], P[2], P[3], '\0' },
    .version = { 
      .major = SPOOKY_PACK_MAJOR_VERSION,
      .minor = SPOOKY_PACK_MINOR_VERSION,
      .revision = SPOOKY_PACK_REVISION_VERSION,
      .subrevision = SPOOKY_PACK_SUBREVISION_VERSION
    },
    .content_len = 0, /* content length calculated after content added */
    .hash = { 0 } /* hash calculated after content added */
  };

  assert(SPOOKY_ITEM_MAGIC == 0x00706b6e616e6d65);

  bool ret = false;
  if(fp) {
    /* Header */
    fwrite(&spf.header, sizeof(unsigned char), sizeof spf.header, fp);
    /* Version */
    spooky_write_version(spf.version, fp, NULL);
    /* Content offset */
    spooky_write_uint64(SPOOKY_CONTENT_OFFSET, fp, NULL);
    /* Content length */
    spooky_write_uint64(spf.content_len, fp, NULL);

    /* placeholder for content hash */
    spooky_write_hash(spf.hash, sizeof spf.hash / sizeof spf.hash[0], fp, NULL);

    /* Content */
    fseek(fp, SPOOKY_CONTENT_OFFSET, SEEK_SET);
    spooky_write_file("res/fonts/PRNumber3.ttf", "foo", fp, &spf.content_len);
    spooky_write_file("res/fonts/PrintChar21.ttf", "bar", fp, &spf.content_len);
    spooky_write_file("res/fonts/DejaVuSansMono.ttf", "baz",  fp, &spf.content_len);
    spooky_write_file("res/fonts/SIL Open Font License.txt", "buz", fp, &spf.content_len);

    /* Update Content Length */
    fseek(fp, sizeof spf.header + sizeof spf.version + sizeof SPOOKY_CONTENT_OFFSET, SEEK_SET);
    spooky_write_uint64(spf.content_len, fp, NULL);

    /* Write an empty hash to the hash offset */
    spooky_write_hash(spf.hash, sizeof spf.hash / sizeof spf.hash[0], fp, NULL);
      
    fseek(fp, 0, SEEK_SET);
    fseek(fp, SPOOKY_CONTENT_OFFSET, SEEK_SET);

    { /* generate content hash */
      unsigned char * buf = calloc(spf.content_len, sizeof * buf);
      if(!buf) { abort(); }
     
      size_t hir = fread(buf, sizeof * buf, spf.content_len, fp);
      assert(hir);
      if(!(hir > 0 && ferror(fp) == 0)) { abort(); }
 
      assert(hir == spf.content_len);

      fseek(fp, 0, SEEK_SET);
      size_t hash_offset = sizeof spf.header + sizeof spf.version + sizeof SPOOKY_CONTENT_OFFSET + sizeof spf.content_len;
      assert(hash_offset < LONG_MAX);
      fseek(fp, (long)hash_offset, SEEK_SET);

      spooky_write_hash(buf, (size_t)spf.content_len, fp, NULL);
      
      free(buf), buf = NULL;
    } 

    /* Footer */
    fseek(fp, 0, SEEK_END);
    fwrite(SPOOKY_FOOTER, sizeof(unsigned char), sizeof SPOOKY_FOOTER, fp);
    ret = true;
  }
 
  return ret;
}

errno_t spooky_pack_verify(FILE * fp) {
  SPOOKY_SET_BINARY_MODE(fp);

  spooky_pack_version version = { 0 };
  uint64_t content_len = 0;
  uint64_t content_offset = 0;  
  
  unsigned char content_hash[crypto_generichash_BYTES] = { 0 };
  unsigned char read_content_hash[crypto_generichash_BYTES] = { 0 };

  if(fp) {
    if(!spooky_read_header(fp)) goto err0;
    /*const uint16_t SPOOKY_PACK_MAJOR_VERSION = 0;
const uint16_t SPOOKY_PACK_MINOR_VERSION = 0;
const uint16_t SPOOKY_PACK_REVISION_VERSION = 1;
const uint16_t SPOOKY_PACK_SUBREVISION_VERSION = 0;
*/
    if(!spooky_read_version(fp, &version)) goto err1;
    assert(version.major == SPOOKY_PACK_MAJOR_VERSION 
        && version.minor == SPOOKY_PACK_MINOR_VERSION
        && version.revision == SPOOKY_PACK_REVISION_VERSION
        && version.subrevision == SPOOKY_PACK_SUBREVISION_VERSION
        );

    if(!spooky_read_uint64(fp, &content_offset)) goto err2;
    assert(content_offset == SPOOKY_CONTENT_OFFSET);
    assert(content_offset > 0 && content_offset <= LONG_MAX);

    if(!spooky_read_uint64(fp, &content_len)) goto err2;
    assert(content_len > 0 && content_len <= LONG_MAX);

    if(!spooky_read_hash(fp, content_hash, crypto_generichash_BYTES)) goto err2;  
  
    fseek(fp, (long)content_offset, SEEK_SET);
    { 
      /* generate read content hash */
      unsigned char * buf = calloc(content_len, sizeof * buf);
      if(!buf) { abort(); }
      
      size_t hir = fread(buf, sizeof * buf, content_len, fp);
      assert(hir && hir == content_len);

      if(!(hir > 0 && ferror(fp) == 0)) { abort(); }

      crypto_generichash(read_content_hash, crypto_generichash_BYTES, buf, (size_t)content_len, NULL, 0);

      free(buf), buf = NULL;
    }

    for(size_t i = 0; i < crypto_generichash_BYTES; i++) {
      if(content_hash[i] != read_content_hash[i]) {
        fprintf(stderr, "Unable to validate resource content. This is a fatal error :(\n");
        abort();
      }
    }

    fseek(fp, (long)content_offset, SEEK_SET);

    char * data = NULL;
    size_t data_len = 0;

    /* TODO: Remove. Test file read: */
    if(!spooky_read_file(fp, &data, &data_len)) goto err2;

    fseek(fp, (long)((content_offset + content_len)), SEEK_SET);
    if(!spooky_read_footer(fp)) goto err3;
  }

  fprintf(stdout, "\nValid SPOOKY! database v%hu.%hu.%hu.%hu: ", version.major, version.minor, version.revision, version.subrevision);

  size_t max_len = sodium_base64_ENCODED_LEN(sizeof content_hash / sizeof content_hash[0], sodium_base64_VARIANT_ORIGINAL_NO_PADDING);
  char * out = malloc(max_len);
  sodium_bin2base64(out, max_len, content_hash, sizeof content_hash / sizeof content_hash[0], sodium_base64_VARIANT_ORIGINAL_NO_PADDING);
  fprintf(stdout, "<%s>\n", out);
  free(out), out = NULL;

  return SP_SUCCESS;

err0:
  fprintf(stderr, "Invalid header\n");
  return SP_FAILURE;
err1:
  fprintf(stderr, "Invalid version\n");
  return SP_FAILURE;
err2:
  fprintf(stderr, "Invalid content length\n");
  return SP_FAILURE;
err3:
  fprintf(stderr, "Invalid footer\n");
  return SP_FAILURE;
}

#define MAX_TEST_STACK_BUFFER_SZ 1024

static void spooky_write_char_tests() {
	unsigned char buf[MAX_TEST_STACK_BUFFER_SZ] = { 0 };

  FILE * fp = fmemopen(buf, sizeof(unsigned char) * 2, "rb+");
  assert(fp);
 
  uint64_t content_len = 0;
  spooky_write_uint8(0xff, fp, &content_len);
  spooky_write_char(0x01, fp, &content_len);

  assert(fflush(fp) == 0);
  assert(fclose(fp) == 0);

	assert(buf[0] == 0xff);
  assert(buf[1] == 0x01);

  assert(content_len == sizeof(uint8_t) + sizeof(char));
}

static void spooky_write_uint8_tests() {
  unsigned char buf[MAX_TEST_STACK_BUFFER_SZ] = { 0 };

  FILE * fp = fmemopen(buf, sizeof(uint8_t), "rb+");
  assert(fp);
 
  uint64_t content_len = 0;
  spooky_write_uint8(0xff, fp, &content_len);

  assert(fflush(fp) == 0);
  assert(fclose(fp) == 0);

  /* assert little endian order */
  assert(buf[0] == 0xff);
  assert(content_len == sizeof(uint8_t));
}

static void spooky_write_int8_tests() {
  char buf[MAX_TEST_STACK_BUFFER_SZ] = { 0 };

  FILE * fp = fmemopen(buf, sizeof(int8_t), "rb+");
  assert(fp);
 
  uint64_t content_len = 0;
  spooky_write_int8((int8_t)0xff, fp, &content_len);

  assert(fflush(fp) == 0);
  assert(fclose(fp) == 0);

  /* assert little endian order */
  assert(buf[0] == (int8_t)0xff);
  assert(content_len == sizeof(int8_t));
}

static void spooky_write_uint16_tests() {
  char buf[MAX_TEST_STACK_BUFFER_SZ] = { 0 };

  FILE * fp = fmemopen(buf, sizeof(uint16_t), "rb+");
  assert(fp);
 
  uint64_t content_len = 0;
  bool res = spooky_write_uint16(0x0d0c, fp, &content_len);
  assert(res);
  assert(fflush(fp) == 0);
  assert(fclose(fp) == 0);

  /* assert little endian order */
  assert(buf[0] == 0x0c);
  assert(buf[1] == 0x0d);
  assert(content_len == sizeof(uint16_t));
}

static void spooky_write_int16_tests() {
  char buf[MAX_TEST_STACK_BUFFER_SZ] = { 0 };

  FILE * fp = fmemopen(buf, sizeof(int16_t), "rb+");
  assert(fp);
 
  uint64_t content_len = 0;
  bool res = spooky_write_int16(0x0d0c, fp, &content_len);
  assert(res);
  assert(fflush(fp) == 0);
  assert(fclose(fp) == 0);

  /* assert little endian order */
  assert(buf[0] == 0x0c);
  assert(buf[1] == 0x0d);
  assert(content_len == sizeof(int16_t));
}

static void spooky_write_uint32_tests() {
  char buf[MAX_TEST_STACK_BUFFER_SZ] = { 0 };

  FILE * fp = fmemopen(buf, sizeof(uint32_t), "rb+");
  assert(fp);
 
  uint64_t content_len = 0;
  spooky_write_uint32(0x0d0c0b0a, fp, &content_len);

  assert(fflush(fp) == 0);
  assert(fclose(fp) == 0);

  /* assert little endian order */
  assert(buf[0] == 0x0a);
  assert(buf[1] == 0x0b);
  assert(buf[2] == 0x0c);
  assert(buf[3] == 0x0d);
  assert(content_len == sizeof(uint32_t));
}

static void spooky_write_int32_tests() {
  char buf[MAX_TEST_STACK_BUFFER_SZ] = { 0 };

  FILE * fp = fmemopen(buf, sizeof(int32_t), "rb+");
  assert(fp);
 
  uint64_t content_len = 0;
  spooky_write_int32(0x0d0c0b0a, fp, &content_len);

  assert(fflush(fp) == 0);
  assert(fclose(fp) == 0);

  /* assert little endian order */
  assert(buf[0] == 0x0a);
  assert(buf[1] == 0x0b);
  assert(buf[2] == 0x0c);
  assert(buf[3] == 0x0d);
  assert(content_len == sizeof(int32_t));
}

static void spooky_write_uint64_tests() {
  unsigned char buf[MAX_TEST_STACK_BUFFER_SZ] = { 0 };

  FILE * fp = fmemopen(buf, sizeof(uint64_t), "rb+");
  assert(fp);
 
  uint64_t content_len = 0;
  spooky_write_uint64(0x0d0c0b0affeeddcc, fp, &content_len);

  assert(fflush(fp) == 0);
  assert(fclose(fp) == 0);

  /* assert little endian order */
  assert(buf[0] == 0xcc);
  assert(buf[1] == 0xdd);
  assert(buf[2] == 0xee);
  assert(buf[3] == 0xff);
  assert(buf[4] == 0x0a);
  assert(buf[5] == 0x0b);
  assert(buf[6] == 0x0c);
  assert(buf[7] == 0x0d);
  assert(content_len == sizeof(uint64_t));
}

static void spooky_write_int64_tests() {
  char buf[MAX_TEST_STACK_BUFFER_SZ] = { 0 };

  FILE * fp = fmemopen(buf, sizeof(int64_t), "rb+");
  assert(fp);
 
  uint64_t content_len = 0;
  spooky_write_int64(0x0d0c0b0affeeddcc, fp, &content_len);

  assert(fflush(fp) == 0);
  assert(fclose(fp) == 0);

  /* assert little endian order */
  assert(buf[0] == (char)0xcc);
  assert(buf[1] == (char)0xdd);
  assert(buf[2] == (char)0xee);
  assert(buf[3] == (char)0xff);
  assert(buf[4] == 0x0a);
  assert(buf[5] == 0x0b);
  assert(buf[6] == 0x0c);
  assert(buf[7] == 0x0d);
  assert(content_len == sizeof(int64_t));
}

static void spooky_write_string_tests() {
  static const char hello_world[] = "Hello, world!";
  
  char buf[MAX_TEST_STACK_BUFFER_SZ] = { 0 };
  FILE * fp = fmemopen(buf, MAX_TEST_STACK_BUFFER_SZ, "rb+");
  assert(fp);
 
  uint64_t content_len = 0;
  spooky_write_string(hello_world, fp, &content_len);
  assert(fflush(fp) == 0);
  assert(fclose(fp) == 0);

  assert(*(buf + sizeof(size_t) + strlen(hello_world)) == '\0');

  size_t len;
  memcpy(&len, buf, sizeof len);
  assert(len == strlen(hello_world));
  
  char * p = (buf + sizeof(size_t));

  char * result = malloc((sizeof(char) * len) + 1);
  memcpy(result, p, (sizeof(char) * len) + 1);
  result[len] = '\0';

  assert(result); 
  assert(strcmp(hello_world, result) == 0);

  free(result), result = NULL;
}

static void spooky_write_string_empty_tests() {
  static const char * empty = (const char *)"";
  
  char buf[MAX_TEST_STACK_BUFFER_SZ] = { 0 };
  FILE * fp = fmemopen(buf, MAX_TEST_STACK_BUFFER_SZ, "rb+");
  assert(fp);
  
  uint64_t content_len = 0;
  spooky_write_string(empty, fp, &content_len);
  assert(fflush(fp) == 0);
  assert(fclose(fp) == 0);

  assert(*(buf + sizeof(size_t) + strlen((const char *)empty)) == '\0');

  size_t len;
  memcpy(&len, buf, sizeof len);
  assert(len == 0);
  assert(len == strlen((const char *)empty));
  
  char * p = &buf[sizeof(size_t)];
  char * result = (char *)strdup(p);
  assert(result); 
  assert(strcmp((const char *)empty, (const char *)result) == 0);

  free(result), result = NULL;
}

static void spooky_write_fixed_width_string_tests() {
  static const char * hello_world = (const char *)"Hello, world!";
  
  char buf[MAX_TEST_STACK_BUFFER_SZ] = { 0 };
  FILE * fp = fmemopen(buf, MAX_TEST_STACK_BUFFER_SZ, "rb+");
  assert(fp);
 
  uint64_t content_len = 0;
  bool res = spooky_write_fixed_width_string(hello_world, 128, fp, &content_len);
  assert(res);
  assert(fflush(fp) == 0);
  assert(fclose(fp) == 0);

  assert(*(buf + sizeof(size_t) + strlen((const char *)hello_world)) == '\0');

  size_t len;
  memcpy(&len, buf, sizeof len);
  assert(len == strlen(hello_world));
  
  char * p = buf + sizeof(size_t);
  char * result = malloc(sizeof(char) * len + 1);
  memcpy(result, p, sizeof(char) * len + 1);
  result[len] = '\0';

  assert(result); 
  assert(strcmp(hello_world, result) == 0);

  free(result), result = NULL;
}

static void spooky_write_bool_tests() {
  uint32_t buf = 0;
  FILE * fp = fmemopen(&buf, sizeof buf, "rb+");
  assert(fp);

  uint64_t content_len = 0;
  spooky_write_bool(true, fp, &content_len);
  assert(fflush(fp) == 0);
  assert(fclose(fp) == 0);

  assert(buf == 0x11111111);
  assert(content_len == sizeof(uint32_t));

  fp = fmemopen(&buf, sizeof buf, "rb+");
  assert(fp);

  content_len = 0;
  spooky_write_bool(false, fp, &content_len);
  assert(fflush(fp) == 0);
  assert(fclose(fp) == 0);

  assert(buf == 0x00000000);
  assert(content_len == sizeof(uint32_t));
}

static void spooky_read_bool_tests() {
  uint32_t buf = 0x11111111;
  FILE * fp = fmemopen(&buf, sizeof buf, "rb");
  assert(fp);

  bool b;
  bool res = spooky_read_bool(fp, &b);
  assert(fflush(fp) == 0);
  assert(fclose(fp) == 0);

  assert(res && b);
}

static void spooky_write_float_tests() {
  const float expected = 10.0f / 7.0f;
  char buf[MAX_TEST_STACK_BUFFER_SZ] = { 0 };
  FILE * fp = fmemopen(buf, MAX_TEST_STACK_BUFFER_SZ, "rb+");
  assert(fp);
 
  uint64_t content_len = 0;
  spooky_write_float(expected, fp, &content_len);
  assert(fflush(fp) == 0);
  assert(fclose(fp) == 0);

	/* uses string serializer; needs to skip length bits */
  char * p = buf + sizeof(size_t);
  char * e = NULL;
  float result = strtof(p, &e);

  assert(spooky_float_equal(result, expected, SMATH_FLT_EPSILON));
  assert(content_len > 0);
}

static void spooky_read_uint8_tests() {
  const uint8_t expected = 0xd0;

  uint8_t buf = 0xd0;
  FILE * fp = fmemopen(&buf, sizeof buf, "rb");
  
  uint8_t result;
  bool ret = spooky_read_uint8(fp, &result);
	assert(fflush(fp) == 0);
  assert(fclose(fp) == 0);

  assert(ret);
  assert(expected == result);
}

void spooky_read_int8_tests() {
  const uint8_t expected = 0x70;

  int8_t buf = 0x70;
  FILE * fp = fmemopen(&buf, sizeof buf, "rb");
  
  int8_t result;
  bool ret = spooky_read_int8(fp, &result);
	assert(fflush(fp) == 0);
  assert(fclose(fp) == 0);

  assert(ret);
  assert(expected == result);
}

static void spooky_read_uint16_tests() {
  const uint16_t expected = 0x0d0c;

  /* uint16_ts stored in little endian; flip the bits */
  char buf[2] = { 0x0c, 0x0d };
  FILE * fp = fmemopen(&buf, 2, "rb");
  
  uint16_t result;
  bool ret = spooky_read_uint16(fp, &result);
  assert(fclose(fp) == 0);

  assert(ret);
  assert(expected == result);
}

static void spooky_read_int16_tests() {
  const int16_t expected = 0x0d0c;

  /* int16_ts stored in little endian; flip the bits */
  char buf[2] = { 0x0c, 0x0d };
  FILE * fp = fmemopen(&buf, 2, "rb");
  
  int16_t result;
  bool ret = spooky_read_int16(fp, &result);
  assert(fclose(fp) == 0);

  assert(ret);
  assert(expected == result);
}

static void spooky_read_uint32_tests() {
  const uint32_t expected = 0x0d0c0b0a;

  /* uint16_ts stored in little endian; flip the bits */
  char buf[sizeof(uint32_t)] = { 0x0a, 0x0b, 0x0c, 0x0d };
  FILE * fp = fmemopen(&buf, sizeof(uint32_t), "rb");
  
  uint32_t result;
  bool ret = spooky_read_uint32(fp, &result);
  assert(fclose(fp) == 0);

  assert(ret);
  assert(expected == result);
}

static void spooky_read_int32_tests() {
  const int32_t expected = 0x0d0c0b0a;

  /* uint16_ts stored in little endian; flip the bits */
  char buf[sizeof(int32_t)] = { 0x0a, 0x0b, 0x0c, 0x0d };
  FILE * fp = fmemopen(&buf, sizeof buf, "rb");
  
  int32_t result;
  bool ret = spooky_read_int32(fp, &result);
  assert(fclose(fp) == 0);

  assert(ret);
  assert(expected == result);
}

static void spooky_read_uint64_tests() {
  const uint64_t expected = 0x0d0c0b0a99887766;

  /* uint16_ts stored in little endian; flip the bits */
  unsigned char buf[sizeof(uint64_t)] = { 0x66, 0x77, 0x88, 0x99, 0x0a, 0x0b, 0x0c, 0x0d };
  FILE * fp = fmemopen(&buf, sizeof(uint64_t), "rb");
  
  uint64_t result;
  bool ret = spooky_read_uint64(fp, &result);
  assert(fclose(fp) == 0);

  assert(ret);
  assert(expected == result);
}

static void spooky_read_int64_tests() {
	const int64_t expected = 0x0d0c0b0affeeddcc;

  /* stored in little endian; flip the bits */
  unsigned char buf[sizeof(int64_t)] = { 0xcc, 0xdd, 0xee, 0xff, 0x0a, 0x0b, 0x0c, 0x0d };
  FILE * fp = fmemopen(&buf, sizeof buf, "rb");
  
  int64_t result;
  bool ret = spooky_read_int64(fp, &result);

  assert(fclose(fp) == 0);

  assert(ret);
  assert(expected == result);
}

void spooky_pack_tests() {
	spooky_write_char_tests();

  spooky_write_uint8_tests();
  spooky_write_int8_tests();

  spooky_write_uint16_tests();
  spooky_write_int16_tests();

  spooky_write_uint32_tests();
  spooky_write_int32_tests();

  spooky_write_uint64_tests();
  spooky_write_int64_tests();

  spooky_write_string_tests(); 
  spooky_write_string_empty_tests();
	spooky_write_fixed_width_string_tests();

  spooky_write_float_tests();
  
  spooky_write_bool_tests();
  spooky_read_bool_tests();

	spooky_read_uint8_tests();
  spooky_read_int8_tests();
  spooky_read_uint16_tests();
  spooky_read_int16_tests();
 	spooky_read_uint32_tests();
 	spooky_read_int32_tests();
 	spooky_read_uint64_tests();
 	spooky_read_int64_tests();
}

