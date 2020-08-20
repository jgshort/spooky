#ifndef SPOOKY_PACK__H
#define SPOOKY_PACK__H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct spooky_pack_version spooky_pack_version;

bool spooky_pack_create(FILE * /* fp */);
long spooky_pack_get_offset(FILE * /* fp */);
errno_t spooky_pack_is_valid_pak_file(FILE * /* fp */, long * /* pak_offset */, uint64_t * /* content_offset */, uint64_t * /* content_len */, uint64_t * /* index_offset */, uint64_t * /* index_len */);
errno_t spooky_pack_verify(FILE * /* fp */);
errno_t spooky_pack_upgrade(FILE * /* fp */, const spooky_pack_version * /* from */, const spooky_pack_version * /* to */);
errno_t spooky_pack_print_resources(FILE * /* dest */, FILE * /* fp */);
void spooky_pack_tests();

#endif /* SPOOKY_PACK__H */

