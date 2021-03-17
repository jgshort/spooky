#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <inttypes.h>
#include <time.h>
#include <stdbool.h>

#include "sp_time.h"

#ifndef CLOCK_MONOTONIC
typedef int clockid_t;
#define CLOCK_MONOTONIC (clockid_t)1
#endif

void sp_sleep(uint32_t delay_in_ms) {
	SDL_Delay(delay_in_ms);
}

int64_t sp_get_time_in_us() {
	struct timespec tp;
	int status = clock_gettime(CLOCK_MONOTONIC, &tp);

	assert(!status);
	if(status) { abort(); }

	int64_t result = (int64_t)tp.tv_sec * (1000 * 1000 * 1000) + (int64_t)tp.tv_nsec;

	return result;
}
