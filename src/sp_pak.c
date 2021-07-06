#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sodium.h>
#include <assert.h>
#include <memory.h>
#include <sodium.h>

#include "sp_z.h"
#include "sp_limits.h"
#include "sp_error.h"
#include "sp_pak.h"
#include "sp_math.h"
#include "sp_hash.h"

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
 *  |  offset | size (bytes) | info              | data
 *  |   0x000 |           16 | spooky header     | { 0xf0, 0x9f, 0x8e, 0x83, 'SPOOKY!', 0xf0, 0x9f, 0x8e, 0x83, '\0' }
 *  |   0x010 |            8 | version (4x16bit) | { 0x0001, 0x0002, 0x0003, 0x0004 }
 *  |   0x018 |            8 | content offset    | offset of pak contents, defaults to 0x100
 *  |   0x020 |            8 | content length    | length of the pak file binary-encoded content excluding the header, version, hash, and footer
 *  |   0x028 |           32 | content hash      | hash of the binary-encoded content, starting at offset {content_offset} and including the magic
 *  |     ??? |          216 | [empty]           | Empty space for future properties
 *  |   0x100 |            8 | magic             | magic header preceeding content
 *  |   0x108 |          ??? | content entries   | binary-encoded content
 *  |EOF-0x18 |            8 | total pak length  | length of the complete pak file, starting from the header through the footer
 *  |EOF-0x10 |           16 | spooky footer     | { 0xf0, 0x9f, 0x8e, 0x83, '!YKOOPS', 0xf0, 0x9f, 0x8e, 0x83, '\0' }
 *
 * Data saved in little endian format
 *
 * unpack example:
 *  i = (data[0]<<0) | (data[1]<<8) | (data[2]<<16) | (data[3]<<24);
*/

#define SPOOKY_HEADER_LEN 16
#define SPOOKY_FOOTER_LEN 16

static const size_t MAX_PACK_STRING_LEN = 4096;
static const unsigned char SPOOKY_PUMPKIN[4] = { 0xf0, 0x9f, 0x8e, 0x83 };

static const unsigned char SPOOKY_HEADER[SPOOKY_HEADER_LEN] = { 0xf0, 0x9f, 0x8e, 0x83, 'S', 'P', 'O', 'O', 'K', 'Y', '!', 0xf0, 0x9f, 0x8e, 0x83, '\n' };
static const unsigned char SPOOKY_FOOTER[SPOOKY_FOOTER_LEN] = { 0xf0, 0x9f, 0x8e, 0x83, '!', 'Y', 'K', 'O', 'O', 'P', 'S', 0xf0, 0x9f, 0x8e, 0x83, '\n' };

static const uint64_t SPOOKY_ITEM_MAGIC = 0x00706b6e616e6d65;

static void spooky_pack_dump_hash(FILE * fp, const unsigned char * c, size_t len) {
  assert(fp && c && len > 0);
  for(size_t i = 0; i < len; i++) {
    fprintf(fp, "%x", c[i]);
  }
}

typedef enum spooky_pack_item_type /* unsigned char */ {
  spit_unspecified = 0,
  spit_undefined = 0,
  spit_null = 1,
  spit_string = 40,
  spit_bin_file = 76,
  spit_hash = 82,
  spit_eof = UCHAR_MAX
} spooky_pack_item_type;

typedef struct spooky_pack_item_bin_file {
  uint64_t decompressed_len;
  uint64_t compressed_len;
  unsigned char decompressed_hash[crypto_generichash_BYTES];
  unsigned char compressed_hash[crypto_generichash_BYTES];
  char * file_path;
  char * key;
  char * data;
  size_t data_len;
  spooky_pack_item_type type;
  char padding[4]; /* non-portable */
} spooky_pack_item_bin_file;

typedef struct spooky_pack_version {
  uint16_t major;
  uint16_t minor;
  uint16_t revision;
  uint16_t subrevision;
} spooky_pack_version;

typedef struct spooky_pack_file {
  unsigned char header[SPOOKY_HEADER_LEN];
  spooky_pack_version version;
  uint64_t content_offset;
  uint64_t content_len;
  uint64_t index_entries;
  uint64_t index_offset;
  uint64_t index_len;
  unsigned char hash[crypto_generichash_BYTES];
} spooky_pack_file;

typedef struct spooky_pack_index_entry {
  char * name;
  uint64_t offset;
  uint64_t len;
} spooky_pack_index_entry;


static char * spooky_pack_encode_binary_data(const unsigned char * bin_data, size_t bin_data_len);
static void spooky_pack_print_file_stats(FILE * fp);

/* Writers */
static bool spooky_write_raw(void * value, size_t len, FILE * fp);

static bool spooky_write_char(char value, FILE * fp, uint64_t * content_len);
static bool spooky_write_uint8(uint8_t value, FILE * fp, uint64_t * content_len);
static bool spooky_write_int8(int8_t value, FILE * fp, uint64_t * content_len);
static bool spooky_write_uint16(uint16_t value, FILE * fp, uint64_t * content_len);
static bool spooky_write_int16(int16_t value, FILE * fp, uint64_t * content_len);
static bool spooky_write_int32(int32_t value, FILE * fp, uint64_t * content_len);
static bool spooky_write_uint64(uint64_t value, FILE * fp, uint64_t * content_len);
static bool spooky_write_int64(int64_t value, FILE * fp, uint64_t * content_len);
static bool spooky_write_bool(bool value, FILE * fp, uint64_t * content_len);
static bool spooky_write_float(float value, FILE * fp, uint64_t * content_len);

static bool spooky_write_item_type(spooky_pack_item_type type, FILE * fp, uint64_t * content_len);
static bool spooky_write_file(const char * file_path, const char * key, FILE * fp, uint64_t * content_len);
static bool spooky_write_string(const char * value, FILE * fp, uint64_t * content_len);
static bool spooky_write_fixed_width_string(const char * value, size_t fixed_width, FILE * fp, uint64_t * content_len);
static bool spooky_write_version(spooky_pack_version version, FILE * fp, uint64_t * content_len);
static bool spooky_write_index_entry(spooky_pack_index_entry * entry, FILE * fp, uint64_t * content_len);
static bool spooky_write_hash(const unsigned char * buf, size_t buf_len, FILE * fp, uint64_t * content_len);

/* Readers */
static bool spooky_read_raw(FILE * fp, size_t len, void * buf);

static bool spooky_read_char(FILE * fp, char * value);
static bool spooky_read_uint8(FILE * fp, uint8_t * value);
static bool spooky_read_int8(FILE * fp, int8_t * value);
static bool spooky_read_uint16(FILE * fp, uint16_t * value);
static bool spooky_read_int32(FILE * fp, int32_t * value);
static bool spooky_read_uint64(FILE * fp, uint64_t * value);
static bool spooky_read_int64(FILE * fp, int64_t * value);
static bool spooky_read_bool(FILE * fp, bool * value);
// TODO: read_float

static bool spooky_read_item_type(FILE * fp, spooky_pack_item_type * item_type);
static bool spooky_read_file(FILE * fp, spooky_pack_item_bin_file * file);
static bool spooky_read_string(FILE * fp, char ** value, size_t * value_len);
// TODO: read_fixed_width_string
static bool spooky_read_version(FILE * fp, spooky_pack_version *version);
static bool spooky_read_index_entry(FILE * fp, spooky_pack_index_entry * entry);
static bool spooky_read_hash(FILE * fp, unsigned char * buf, size_t buf_len);

static bool spooky_read_footer(FILE * fp);
static bool spooky_read_header(FILE * fp);

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

static bool spooky_read_string(FILE * fp, char ** value, size_t * value_len) {
  assert(fp && value && value_len);

  uint64_t len = 0;
  if(!spooky_read_uint64(fp, &len)) { return false; }

  assert(len <= SIZE_MAX);
  assert(len <= SPOOKY_MAX_STRING_LEN);

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

static bool spooky_read_file(FILE * fp, spooky_pack_item_bin_file * file) {
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
  char * decompressed_data_copy = NULL;
  {
    FILE * inflated_fp = fmemopen(decompressed_data, decompressed_len, "rb+");
    SPOOKY_SET_BINARY_MODE(inflated_fp);
    size_t inflated_buf_len = 0;

    if(spooky_inflate_file(deflated_fp, inflated_fp, &inflated_buf_len) != SP_SUCCESS) {
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
    decompressed_data_copy = calloc(decompressed_len, sizeof * decompressed_data_copy );
    if(!decompressed_data_copy) { abort(); }
    memmove(decompressed_data_copy, decompressed_data, decompressed_len);

    fclose(inflated_fp);
  }
  fclose(deflated_fp);

  free(compressed_data), compressed_data = NULL;
  free(decompressed_data), decompressed_data = NULL;

  file->type = type;
  file->decompressed_len = decompressed_len;
  file->compressed_len = compressed_len;
  memmove(file->decompressed_hash, decompressed_hash, crypto_generichash_BYTES);
  memmove(file->compressed_hash, compressed_hash, crypto_generichash_BYTES);
  file->file_path = file_path;
  file->key = key;
  file->data = decompressed_data_copy;
  file->data_len = (size_t)decompressed_len;
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
            if(spooky_deflate_file(inflated_fp, deflated_fp, &deflated_buf_len) != SP_SUCCESS) { abort(); };

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

bool spooky_write_index_entry(spooky_pack_index_entry * entry, FILE * fp, uint64_t * content_len) {
  assert(entry && fp);

  if(!spooky_write_string(entry->name, fp, content_len)) { goto err; }
  if(!spooky_write_uint64(entry->offset, fp, content_len)) { goto err; }
  if(!spooky_write_uint64(entry->len, fp, content_len)) { goto err; }

  return true;
err:
  return false;
}

bool spooky_read_index_entry(FILE * fp, spooky_pack_index_entry * entry) {
  assert(fp && entry);

  size_t name_len = 0;
  if(!spooky_read_string(fp, &(entry->name), &name_len)) { goto err; }
  if(!spooky_read_uint64(fp, &(entry->offset))) { goto err; }
  if(!spooky_read_uint64(fp, &(entry->len))) { goto err; }

  return true;

err:
  return false;
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

bool spooky_write_uint32(uint32_t value, FILE * fp, uint64_t * content_len) {
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

static bool spooky_read_char(FILE * fp, char * value) {
  if(feof(fp) != 0) return false;

	char c;
  size_t hir = fread(&c, sizeof c, 1, fp);
  assert(hir == sizeof *value);

	*value = c;

  return hir == sizeof *value && ferror(fp) == 0;
}

static bool spooky_read_uint8(FILE * fp, uint8_t * value) {
  (void)spooky_read_char;

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

bool spooky_read_uint32(FILE * fp, uint32_t * value) {
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

  return eq;
}

bool spooky_pack_create(FILE * fp, const spooky_pack_content_entry * content, size_t content_len) {
  assert(content_len > 0 && content != NULL);

  const unsigned char * P = SPOOKY_PUMPKIN;
  SPOOKY_SET_BINARY_MODE(fp);
  spooky_pack_file spf = {
    .header = { P[0], P[1], P[2], P[3], 'S', 'P', 'O', 'O', 'K', 'Y', '!', P[0], P[1], P[2], P[3], '\n' },
    .version = {
      .major = SPOOKY_PACK_MAJOR_VERSION,
      .minor = SPOOKY_PACK_MINOR_VERSION,
      .revision = SPOOKY_PACK_REVISION_VERSION,
      .subrevision = SPOOKY_PACK_SUBREVISION_VERSION
    },
    .content_offset = 0, /* offset that content begins */
    .content_len = 0, /* content length calculated after content added */
    .index_entries = 0,
    .index_offset = 0,
    .index_len = 0,
    .hash = { 0 } /* hash calculated after content added */
  };

  assert(SPOOKY_ITEM_MAGIC == 0x00706b6e616e6d65);

  bool ret = false;
  /* Header */
  fwrite(&spf.header, sizeof(unsigned char), sizeof spf.header, fp);
  /* Version */
  spooky_write_version(spf.version, fp, NULL);
  /* Content offset */
  spooky_write_uint64(SPOOKY_CONTENT_OFFSET, fp, NULL);
  /* Content length */
  spooky_write_uint64(spf.content_len, fp, NULL);
  /* Index entries */
  long index_entries_loc = ftell(fp);
  spooky_write_uint64(0, fp, NULL);
  /* Index offset */
  long index_offset_loc = ftell(fp);
  spooky_write_uint64(0, fp, NULL);
  /* Index length */
  long index_len_loc = ftell(fp);
  spooky_write_uint64(0, fp, NULL);

  /* placeholder for content hash */
  spooky_write_hash(spf.hash, sizeof spf.hash / sizeof spf.hash[0], fp, NULL);

  /* Content */
  fseek(fp, SPOOKY_CONTENT_OFFSET, SEEK_SET);
  /* Write the magic number */
  spooky_write_uint64(SPOOKY_ITEM_MAGIC, fp, &spf.content_len);

  /* Actual pak content */
  assert(content_len < 1024); /* Arbitrary number is arbitrary */

  spooky_pack_index_entry * entries = calloc(sizeof * entries, content_len);
  spooky_pack_index_entry * entry = NULL;

  long offset = ftell(fp);
  assert(offset > 0);

  for(size_t i = 0; i < content_len; i++) {
    entry = entries + i;
    const spooky_pack_content_entry * e = content + i;
    /* write the content entry... */
    spooky_write_file(e->path, e->name, fp, &spf.content_len);
    /* ... and setup the index entry */
    entry->name = strndup(e->name, SPOOKY_MAX_STRING_LEN);
    entry->offset = (uint64_t)offset;
    entry->len = spf.content_len - (uint64_t)offset;
    offset = ftell(fp);
    assert(offset > 0);
  }

  /* Update Content Length */
  fseek(fp, sizeof spf.header + sizeof spf.version + sizeof SPOOKY_CONTENT_OFFSET, SEEK_SET);
  spooky_write_uint64(spf.content_len, fp, NULL);

  fseek(fp, 0, SEEK_SET);
  fseek(fp, sizeof spf - (sizeof spf.hash / sizeof spf.hash[0]), SEEK_SET);

  /* Write an empty hash to the hash offset */
  spooky_write_hash(spf.hash, sizeof spf.hash / sizeof spf.hash[0], fp, NULL);

  fseek(fp, 0, SEEK_SET);
  fseek(fp, SPOOKY_CONTENT_OFFSET, SEEK_SET);

  { /* generate content hash; includes magic */
    unsigned char * buf = calloc(spf.content_len, sizeof * buf);
    if(!buf) { abort(); }

    size_t hir = fread(buf, sizeof * buf, spf.content_len, fp);
    assert(hir);
    if(!(hir > 0 && ferror(fp) == 0)) { abort(); }

    assert(hir == spf.content_len);

    fseek(fp, 0, SEEK_SET);
    size_t hash_offset = sizeof spf - (sizeof spf.hash / sizeof spf.hash[0]);
    assert(hash_offset < LONG_MAX);
    fseek(fp, (long)hash_offset, SEEK_SET);

    spooky_write_hash(buf, (size_t)spf.content_len, fp, NULL);

    free(buf), buf = NULL;
  }

  /* Write index entries */
  fseek(fp, 0, SEEK_END);
  long index_offset = ftell(fp);
  uint64_t index_len = 0, index_entries = 0;
  for(size_t i = 0; i < content_len; i++) {
    spooky_pack_index_entry * e = entries + i;
    spooky_write_index_entry(e, fp, &index_len);
    /* fprintf(stdout, "Index written for %s\n", e->name); */
    free(e->name), e->name = NULL;
    (void)spooky_read_index_entry;
    index_entries++;
  }

  fseek(fp, index_entries_loc, SEEK_SET);
  spooky_write_uint64((uint64_t)index_entries, fp, NULL);
  fseek(fp, index_offset_loc, SEEK_SET);
  spooky_write_uint64((uint64_t)index_offset, fp, NULL);
  fseek(fp, index_len_loc, SEEK_SET);
  spooky_write_uint64((uint64_t)index_len, fp, NULL);

  /* SPDB length */
  fseek(fp, 0, SEEK_END);
  long file_len_offset = ftell(fp);
  spooky_write_uint64((uint64_t)file_len_offset + sizeof(uint64_t) + sizeof SPOOKY_FOOTER, fp, NULL);

  /* Footer */
  fwrite(SPOOKY_FOOTER, sizeof(unsigned char), sizeof SPOOKY_FOOTER, fp);
  fseek(fp, 0, SEEK_END);
  long spdb_len = ftell(fp);
  assert(spdb_len > 0 && spdb_len <= LONG_MAX);

  size_t expected_len =
    sizeof spf  /* header */
    + (((SPOOKY_CONTENT_OFFSET - sizeof spf)) + spf.content_len + index_len)  /* offset of content - header */
    + ((sizeof(uint64_t) + sizeof SPOOKY_FOOTER)) /* (file length + footer marker) */
    ;
  assert((size_t)spdb_len == expected_len);

  //fseek(fp, (long)file_len_offset, SEEK_SET);
  //spooky_write_uint64((size_t)spdb_len, fp, NULL);
  fseek(fp, 0, SEEK_END);

  ret = true;

  return ret;
}

errno_t spooky_pack_upgrade(FILE * fp, const spooky_pack_version * from, const spooky_pack_version * to) {
  (void)fp;
  (void)from;
  (void)to;
  return SP_SUCCESS;
}

errno_t spooky_pack_print_resources(FILE * dest, FILE * fp) {
  SPOOKY_SET_BINARY_MODE(fp);
  (void)dest;
  spooky_pack_version version = { 0 };
  uint64_t content_len = 0;
  uint64_t content_offset = 0;
  uint64_t index_entries = 0;
  uint64_t index_offset = 0;
  uint64_t index_len = 0;

  unsigned char content_hash[crypto_generichash_BYTES] = { 0 };
  unsigned char read_content_hash[crypto_generichash_BYTES] = { 0 };

  if(fp) {
    long pak_offset = spooky_pack_get_offset(fp);
    if(pak_offset < 0) { return SP_FAILURE; }
    fseek(fp, pak_offset, SEEK_SET);

    if(!spooky_read_header(fp)) goto err0;
    if(!spooky_read_version(fp, &version)) goto err1;
    /* fprintf(dest, "SPDB Version: %i.%i.%i.%i\n", version.major, version.minor, version.revision, version.subrevision); */
    if(!spooky_read_uint64(fp, &content_offset)) goto err2;
    /* fprintf(dest, "Content offset: %x\n", (unsigned int)content_offset); */

    if(!spooky_read_uint64(fp, &content_len)) goto err3;
    /* fprintf(dest, "Content length: %lu\n", (size_t)content_len); */

    if(!spooky_read_uint64(fp, &index_entries)) goto err3;
    /* fprintf(dest, "Index entries: %lu\n", (size_t)index_entries); */

    if(!spooky_read_uint64(fp, &index_offset)) goto err3;
    /* fprintf(dest, "Index offset: %lu\n", (size_t)index_offset); */

    if(!spooky_read_uint64(fp, &index_len)) goto err3;
    /* fprintf(dest, "Index length: %lu\n", (size_t)index_len); */

    if(!spooky_read_hash(fp, content_hash, crypto_generichash_BYTES)) goto err4;
    char * content_hash_out = spooky_pack_encode_binary_data(content_hash, sizeof content_hash / sizeof content_hash[0]);
    /* fprintf(dest, "Saved hash: <%s>\n", content_hash_out); */
    free(content_hash_out), content_hash_out = NULL;

    fseek(fp, pak_offset + (long)content_offset, SEEK_SET);
    {
      /* generate read content hash */
      unsigned char * buf = calloc(content_len, sizeof * buf);
      if(!buf) { abort(); }

      size_t hir = fread(buf, sizeof * buf, content_len, fp);
      assert(hir && hir == content_len);
      if(!(hir > 0 && ferror(fp) == 0)) { abort(); }

      crypto_generichash(read_content_hash, crypto_generichash_BYTES, buf, (size_t)content_len, NULL, 0);

      char * saved_content_hash_out = spooky_pack_encode_binary_data(read_content_hash, sizeof read_content_hash / sizeof read_content_hash[0]);
      /* fprintf(dest, "Calculated hash: <%s>\n", saved_content_hash_out); */
      free(saved_content_hash_out), saved_content_hash_out = NULL;

      free(buf), buf = NULL;
    }

    for(size_t i = 0; i < crypto_generichash_BYTES; i++) {
      if(content_hash[i] != read_content_hash[i]) { goto err5; }
    }

    fseek(fp, pak_offset + (long)content_offset, SEEK_SET);

    uint64_t magic = 0;
    if(!spooky_read_uint64(fp, &magic)) { goto err5; }
    if(magic != SPOOKY_ITEM_MAGIC) { goto err5; }
    assert(magic == SPOOKY_ITEM_MAGIC);

    /* fprintf(dest, "Magic: 0x%" PRIx64 "\n", magic); */

    spooky_pack_print_file_stats(fp);
    spooky_pack_print_file_stats(fp);
    //spooky_pack_print_file_stats(fp);
    spooky_pack_print_file_stats(fp);
    spooky_pack_print_file_stats(fp);

    fseek(fp, 0, SEEK_END);
    long saved_file_len = ftell(fp);
    assert(saved_file_len > 0 && saved_file_len <= LONG_MAX);
    /* fprintf(dest, "Calculated SPDB size: %lu\n", saved_file_len); */

    fseek(fp, pak_offset, SEEK_SET);
    /* read content length */
    fseek(fp, pak_offset + (long)((content_offset + content_len + index_len)), SEEK_SET);

    uint64_t file_len = 0;
    spooky_read_uint64(fp, &file_len);
    /* fprintf(dest, "Saved SPDB size: %lu\n", (size_t)file_len); */
    assert((long)file_len == saved_file_len - pak_offset);

    /* read footer */
    fseek(fp, pak_offset + (long)((content_offset + content_len + index_len + sizeof(uint64_t))), SEEK_SET);
    if(!spooky_read_footer(fp)) goto err3;
  }

  /* fprintf(dest, "Valid SPDB v%hu.%hu.%hu.%hu: ", version.major, version.minor, version.revision, version.subrevision); */
  char * out = spooky_pack_encode_binary_data(content_hash, sizeof content_hash / sizeof content_hash[0]);
  /* fprintf(dest, "<%s>\n", out); */
  free(out), out = NULL;

  return SP_SUCCESS;

err0:
  fprintf(stderr, "Invalid header\n");
  return SP_FAILURE;
err1:
  fprintf(stderr, "Invalid version\n");
  return SP_FAILURE;
err2:
  fprintf(stderr, "Invalid content offset\n");
  return SP_FAILURE;
err3:
  fprintf(stderr, "Invalid content length\n");
  return SP_FAILURE;
err4:
  fprintf(stderr, "Invalid footer\n");
  return SP_FAILURE;
err5:
  fprintf(stderr, "Unable to validate resource content. This is a fatal error :(\n");
  return SP_FAILURE;
}

char * spooky_pack_encode_binary_data(const unsigned char * bin_data, size_t bin_data_len) {
  size_t max_len = sodium_base64_ENCODED_LEN(bin_data_len, sodium_base64_VARIANT_ORIGINAL_NO_PADDING);
  char * out = malloc(max_len);
  sodium_bin2base64(out, max_len, bin_data, bin_data_len, sodium_base64_VARIANT_ORIGINAL_NO_PADDING);
  return out;
}

void spooky_pack_print_file_stats(FILE * fp) {
  spooky_pack_item_bin_file file;
  if(!spooky_read_file(fp, &file)) goto err;

  char * encoded_decompressed_hash = spooky_pack_encode_binary_data(file.decompressed_hash, sizeof file.decompressed_hash / sizeof file.decompressed_hash[0]);
  char * encoded_compressed_hash = spooky_pack_encode_binary_data(file.compressed_hash, sizeof file.compressed_hash / sizeof file.compressed_hash[0]);

  fprintf(stdout, "%s@%s [%lu -> %lu] <%s>; <%s>\n", file.key, file.file_path, (size_t)file.decompressed_len, (size_t)file.compressed_len, encoded_decompressed_hash, encoded_compressed_hash);

  free(encoded_decompressed_hash), encoded_decompressed_hash = NULL;
  free(encoded_compressed_hash), encoded_compressed_hash = NULL;

  return;

err:
  fprintf(stderr, "Pack contents corrupt. Skipping.\n");
  return;
}

errno_t spooky_pack_is_valid_pak_file(FILE * fp, long * pak_offset, uint64_t * content_offset, uint64_t * content_len, uint64_t * index_entries,  uint64_t * index_offset, uint64_t * index_len) {
  SPOOKY_SET_BINARY_MODE(fp);

  if(!content_offset || !content_len) { abort(); }
  spooky_pack_file spf = {
    .header = { 0 },
    .version = { 0 },
    .content_offset = 0,
    .content_len = 0,
    .index_entries = 0,
    .index_offset = 0,
    .index_len = 0,
    .hash = { 0 }
  };

  long pak_file_offset = spooky_pack_get_offset(fp);
  if(pak_file_offset < 0) { goto err; }
  if(pak_offset) { *pak_offset = pak_file_offset; }

  fseek(fp, *pak_offset, SEEK_SET);

  unsigned char content_hash[crypto_generichash_BYTES] = { 0 };
  unsigned char read_content_hash[crypto_generichash_BYTES] = { 0 };

  if(!spooky_read_header(fp)) goto err;
  if(!spooky_read_version(fp, &spf.version)) goto err;
  assert(spf.version.major == SPOOKY_PACK_MAJOR_VERSION
      && spf.version.minor == SPOOKY_PACK_MINOR_VERSION
      && spf.version.revision == SPOOKY_PACK_REVISION_VERSION
      && spf.version.subrevision == SPOOKY_PACK_SUBREVISION_VERSION
      );

  if(!spooky_read_uint64(fp, &spf.content_offset)) goto err;
  assert(spf.content_offset == SPOOKY_CONTENT_OFFSET);
  assert(spf.content_offset > 0 && spf.content_offset <= LONG_MAX);

  if(!spooky_read_uint64(fp, &spf.content_len)) goto err;
  assert(spf.content_len > 0 && spf.content_len <= LONG_MAX);

  if(!spooky_read_uint64(fp, &spf.index_entries)) goto err;
  if(!spooky_read_uint64(fp, &spf.index_offset)) goto err;
  if(!spooky_read_uint64(fp, &spf.index_len)) goto err;

  if(!spooky_read_hash(fp, content_hash, crypto_generichash_BYTES)) goto err;

  fseek(fp, *pak_offset + (long)(spf.content_offset), SEEK_SET);
  {
    /* generate read content hash */
    unsigned char * buf = calloc(spf.content_len, sizeof * buf);
    if(!buf) { abort(); }

    size_t hir = fread(buf, sizeof * buf, spf.content_len, fp);
    assert(hir && hir == spf.content_len);

    if(!(hir > 0 && ferror(fp) == 0)) { abort(); }

    crypto_generichash(read_content_hash, crypto_generichash_BYTES, buf, (size_t)(spf.content_len), NULL, 0);

    free(buf), buf = NULL;
  }

  for(size_t i = 0; i < crypto_generichash_BYTES; i++) {
    if(content_hash[i] != read_content_hash[i]) { goto err; }
  }

  uint64_t magic = 0;
  fseek(fp, *pak_offset + (long)(spf.content_offset), SEEK_SET);
  spooky_read_uint64(fp, &magic);
  if(magic != SPOOKY_ITEM_MAGIC) { goto err; }
  assert(magic == SPOOKY_ITEM_MAGIC);

  *content_offset = spf.content_offset;
  *content_len = spf.content_len;
  *index_entries = spf.index_entries;
  *index_offset = spf.index_offset;
  *index_len = spf.index_len;

  return SP_SUCCESS;

err:
  return SP_FAILURE;
}

long spooky_pack_get_offset(FILE * fp) {
  fseek(fp, 0, SEEK_END);

  long file_len = ftell(fp);
  assert(file_len >= 0 && (size_t)file_len <= SIZE_MAX);

  fseek(fp, file_len - (long)(sizeof SPOOKY_FOOTER), SEEK_SET);
  if(!spooky_read_footer(fp)) goto err;

  fseek(fp, file_len - (long)(sizeof SPOOKY_FOOTER + sizeof(uint64_t)), SEEK_SET);

  uint64_t pak_file_len = 0;
  if(!spooky_read_uint64(fp, &pak_file_len)) goto err;
  assert(pak_file_len <= LONG_MAX);

  long pak_file_offset = file_len - (long)pak_file_len;

  return pak_file_offset;

err:
  return -1;
}

errno_t spooky_pack_verify(FILE * fp, const spooky_hash_table * hash) {
  if(!fp) { return SP_FAILURE; }
  if(!hash) { return SP_FAILURE; }

  SPOOKY_SET_BINARY_MODE(fp);

  uint64_t content_offset = 0;
  uint64_t content_len = 0;

  uint64_t index_entries = 0;
  uint64_t index_offset = 0;
  uint64_t index_len = 0;

  long pak_offset = -1;
  errno_t is_valid = spooky_pack_is_valid_pak_file(fp, &pak_offset, &content_offset, &content_len, &index_entries, &index_offset, &index_len);
  if(is_valid != SP_SUCCESS) { goto err5; }

  fseek(fp, pak_offset + (long)content_offset, SEEK_SET);

  uint64_t magic = 0;
  if(!spooky_read_uint64(fp, &magic)) { goto err2; }
  if(magic != SPOOKY_ITEM_MAGIC) { goto err2; }
  assert(magic == SPOOKY_ITEM_MAGIC);

  for(int i = 0; i < 4; i++) {
    spooky_pack_item_bin_file file = { 0 };
    spooky_pack_item_file * pub = calloc(1, sizeof * pub);

    if(!spooky_read_file(fp, &file)) { goto err2; }
    pub->data = file.data;
    pub->data_len = file.decompressed_len;
    hash->ensure(hash, file.key, strnlen(file.key, SPOOKY_MAX_STRING_LEN), pub, NULL);
  }

  spooky_pack_index_entry entry;
  for(uint64_t i = 0; i < index_entries; i++) {
    spooky_read_index_entry(fp, &entry);
  }

  fseek(fp, 0, SEEK_END);
  long saved_file_len = ftell(fp);
  assert(saved_file_len > 0 && saved_file_len <= LONG_MAX);

  /* read content length */
  fseek(fp, pak_offset + (long)((content_offset + content_len + index_len)), SEEK_SET);

  uint64_t file_len = 0;
  spooky_read_uint64(fp, &file_len);
  assert((long)file_len == saved_file_len - pak_offset);

  /* read footer */
  fseek(fp, pak_offset + (long)((content_offset + content_len + index_len + sizeof(uint64_t))), SEEK_SET);
  if(!spooky_read_footer(fp)) { goto err3; }

  return SP_SUCCESS;

err2:
  fprintf(stderr, "Invalid content offset\n");
  return SP_FAILURE;
err3:
  fprintf(stderr, "Invalid content length\n");
  return SP_FAILURE;

err5:
  fprintf(stderr, "Unable to validate resource content. This is a fatal error :(\n");
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

