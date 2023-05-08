#ifndef SP_ITER__H
#define SP_ITER__H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct spooky_iter spooky_iter;

  typedef struct spooky_iter {
    bool (*next)(const spooky_iter * it);
    const void * (*current)(const spooky_iter * it);
    void (*reset)(const spooky_iter * it);
    void (*reverse)(const spooky_iter * it);
    void (*free)(const spooky_iter * it);
  } spooky_iter;

#ifdef __cplusplus
}
#endif

#endif /* SP_ITER__H */

