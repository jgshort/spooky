#include <math.h>
#include <float.h>
#include <stdbool.h>

#include "../include/sp_math.h"

const float SMATH_FLT_EPSILON = FLT_EPSILON;

bool sp_float_equal(float left, float right, float max_rel_diff /*SMATH_FLT_EPSILON*/)
{
  const float diff = fabsf(left - right);
  left = fabsf(left);
  right = fabsf(right);
  const float scaled_epsilon = max_rel_diff * fmaxf(left, right);

  return diff <= scaled_epsilon;
}

int sp_int_min3(int a, int b, int c) {
  return (a < b) ? (a < c ? a : c) : (b < c ? b : c);
}

int sp_int_max3(int a, int b, int c) {
  return (a > b) ? (a > c ? a : c) : (b > c ? b : c);
}

int sp_int_max(int left, int right) {
  return left > right ? left : right;
}

int sp_int_min(int left, int right) {
  return left < right ? left : right;
}
