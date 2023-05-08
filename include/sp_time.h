#ifndef SP_TIME__H
#define SP_TIME__H

#ifdef __cplusplus
extern "C" {
#endif

#include "sp_gui.h"

	inline void sp_sleep(uint32_t delay_in_ms) {
		SDL_Delay(delay_in_ms);
	}

	inline uint64_t sp_get_time_in_ms(void) {
		return SDL_GetTicks64();
	}

#ifdef __cplusplus
}
#endif

#endif /* SP_TIME__H */

