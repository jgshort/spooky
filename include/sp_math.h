#ifndef SP_MATH__H
#define SP_MATH__H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

  typedef struct sp_vector {
    uint32_t x;
    uint32_t y;
    uint32_t z;
  } sp_vector;

  typedef struct sp_vector_3df {
    double x;
    double y;
    double z;
  } sp_vector_3df;

  extern const float SMATH_FLT_EPSILON;

  bool sp_float_equal(float /* left */, float /* right */, float /* max_rel_diff */ /*SMATH_FLT_EPSILON*/);

  int sp_int_min3(int /* a */, int /* b */, int /* c */);
  int sp_int_max3(int /* a */, int /* b */, int /* c */);

  int sp_int_max(int /* left */, int /* right */);
  int sp_int_min(int /* left */, int /* right */);

  float sp_round_up(float /* n */, int /* by_m */);

#ifdef __cplusplus
}
#endif

#endif /* SP_MATH__H */

