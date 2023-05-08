#ifndef SP_PACK__H
#define SP_PACK__H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "sp_hash.h"

  typedef struct sp_pack_version sp_pack_version;

  typedef struct sp_pack_item_file {
    char * data;
    size_t data_len;
  } sp_pack_item_file;

  typedef struct sp_pack_content_entry {
    const char * path;
    const char * name;
  } sp_pack_content_entry;

  bool sp_pack_create(FILE * /* fp */, const sp_pack_content_entry * /* content */, size_t /* content_len */);
  long sp_pack_get_offset(FILE * /* fp */);
  errno_t sp_pack_is_valid_pak_file(FILE * /* fp */, long * /* pak_offset */, uint64_t * /* content_offset */, uint64_t * /* content_len */, uint64_t * /* index_entries */, uint64_t * /* index_offset */, uint64_t * /* index_len */);
  errno_t sp_pack_verify(FILE * /* fp */, const sp_hash_table * /* hash */);
  errno_t sp_pack_upgrade(FILE * /* fp */, const sp_pack_version * /* from */, const sp_pack_version * /* to */);
  errno_t sp_pack_print_resources(FILE * /* dest */, FILE * /* fp */);
  void sp_pack_tests();

  bool sp_write_uint32(uint32_t /* value */, FILE * /* fp */, uint64_t * /* content_len */);
  bool sp_read_uint32(FILE * /* fp */, uint32_t * /* value */);

#ifdef __cplusplus
}
#endif


#endif /* SP_PACK__H */

