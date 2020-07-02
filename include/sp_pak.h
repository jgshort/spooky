#ifndef SPOOKY_PACK__H
#define SPOOKY_PACK__H

#include <stdbool.h>

bool spooky_build_pack(FILE * /* fp */);
void spooky_verify_pack(const char * /* pack_path */);

void spooky_pack_tests();

#endif /* SPOOKY_PACK__H */

