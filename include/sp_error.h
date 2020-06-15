#ifndef SP_ERROR__H
#define SP_ERROR__H

/*! @file sp_error.h
 * @brief spooky error handling types and variables.
 *
 * @p sp_error defines the common spooky error types and values.
 */

#ifndef __STDC_LIB_EXT1__
/** typedef for int.
 *
 * A typedef for the type int, used to self-document functions that return 
 * errno values in the C standard library. Defined here if @a __STDC_LIB_EXT1__
 * is not defined, otherwise we use the definition included in errno.h.
 */
typedef int errno_t;
#else
#include <errno.h>
#endif

/** Return value indicating success. */
extern const errno_t SP_SUCCESS;

/** Return value indicating failure. */
extern const errno_t SP_FAILURE;

int spooky_is_sdl_error(const char * msg);

#endif /* SP_ERROR__H */

