#ifndef SP_TIME__H
#define SP_TIME__H

#ifdef __cplusplus
extern "C" {
#endif

#include "sp_gui.h"

int64_t sp_get_time_in_us();

void sp_sleep(uint32_t /* delay_in_ms */);

#ifdef __cplusplus
}
#endif

#endif /* SP_TIME__H */

