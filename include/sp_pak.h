#ifndef SPOOKY_PACK__H
#define SPOOKY_PACK__H

#include <stdio.h>
#include <stdbool.h>

bool spooky_pack_create(FILE * /* fp */);
errno_t spooky_pack_verify(FILE * /* fp */);

void spooky_pack_tests();

#endif /* SPOOKY_PACK__H */

