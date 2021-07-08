#ifndef SPOOKY_MATH__H
#define SPOOKY_MATH__H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdbool.h>

typedef struct spooky_vector {
  uint32_t x;
  uint32_t y;
  uint32_t z;
} spooky_vector;

typedef struct spooky_vector_3df {
  double x;
  double y;
  double z;
} spooky_vector_3df;

extern const float SMATH_FLT_EPSILON;

bool spooky_float_equal(float /* left */, float /* right */, float /* max_rel_diff */ /*SMATH_FLT_EPSILON*/);

int spooky_int_min3(int /* a */, int /* b */, int /* c */);
int spooky_int_max3(int /* a */, int /* b */, int /* c */);

int spooky_int_max(int /* left */, int /* right */);
int spooky_int_min(int /* left */, int /* right */);

float spooky_round_up(float /* n */, int /* by_m */);

#ifdef __cplusplus
}
#endif

#endif /* SPOOKY_MATH__H */

