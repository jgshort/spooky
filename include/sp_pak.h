#ifndef SPOOKY_PACK__H
#define SPOOKY_PACK__H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "sp_hash.h"

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct spooky_pack_version spooky_pack_version;

  typedef struct spooky_pack_item_file {
    char * data;
    size_t data_len;
  } spooky_pack_item_file;

  typedef struct spooky_pack_content_entry {
    const char * path;
    const char * name;
  } spooky_pack_content_entry;

  bool spooky_pack_create(FILE * /* fp */, const spooky_pack_content_entry * /* content */, size_t /* content_len */);
  long spooky_pack_get_offset(FILE * /* fp */);
  errno_t spooky_pack_is_valid_pak_file(FILE * /* fp */, long * /* pak_offset */, uint64_t * /* content_offset */, uint64_t * /* content_len */, uint64_t * /* index_entries */, uint64_t * /* index_offset */, uint64_t * /* index_len */);
  errno_t spooky_pack_verify(FILE * /* fp */, const spooky_hash_table * /* hash */);
  errno_t spooky_pack_upgrade(FILE * /* fp */, const spooky_pack_version * /* from */, const spooky_pack_version * /* to */);
  errno_t spooky_pack_print_resources(FILE * /* dest */, FILE * /* fp */);
  void spooky_pack_tests();

  bool spooky_write_uint32(uint32_t /* value */, FILE * /* fp */, uint64_t * /* content_len */);
  bool spooky_read_uint32(FILE * /* fp */, uint32_t * /* value */);

#ifdef __cplusplus
}
#endif


#endif /* SPOOKY_PACK__H */

