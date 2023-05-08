#ifndef SP_ITER__H
#define SP_ITER__H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

  typedef struct sp_iter sp_iter;

  typedef struct sp_iter {
    bool (*next)(const sp_iter * it);
    const void * (*current)(const sp_iter * it);
    void (*reset)(const sp_iter * it);
    void (*reverse)(const sp_iter * it);
    void (*free)(const sp_iter * it);
  } sp_iter;

#ifdef __cplusplus
}
#endif

#endif /* SP_ITER__H */

