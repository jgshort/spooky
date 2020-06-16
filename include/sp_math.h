#ifndef SPOOKY_MATH__H
#define SPOOKY_MATH__H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

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

