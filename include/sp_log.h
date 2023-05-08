#ifndef SP_LOG__H
#define SP_LOG__H

#ifdef __cplusplus
extern "C" {
#endif

#include "sp_console.h"

  typedef enum sp_log_severity {
    SLS_INFO,
    SLS_DEBUG,
    SLS_WARN,
    SLS_ERROR,
    SLS_CRITICAL
  } sp_log_severity;

  typedef struct sp_log sp_log;
  typedef struct sp_log {
    const sp_log * (*ctor)(const sp_log * self);
    const sp_log * (*dtor)(const sp_log * self);
    void (*free)(const sp_log * self);
    void (*release)(const sp_log * self);

    void (*prepend)(const sp_log * self, const char * line, sp_log_severity severity);
    void (*dump)(const sp_log * self, const sp_console * console);
  } sp_log;

  const sp_log * sp_log_init(sp_log * self);
  const sp_log * sp_log_alloc(void);
  const sp_log * sp_log_acquire(void);
  const sp_log * sp_log_ctor(const sp_log * self);
  const sp_log * sp_log_dtor(const sp_log * self);
  void sp_log_free(const sp_log * self);
  void sp_log_release(const sp_log * self);

  void sp_log_startup(void);

  void sp_log_prepend_formatted(sp_log_severity severity, const char * format, ...)
#ifdef __linux__
    __attribute__ ((format (printf, 2, 3)))
#endif
    ;

  void sp_log_shutdown(void);
  void sp_log_dump_to_console(const sp_console * console);
  size_t sp_log_get_global_entries_count(void);

#define SP_LOG(sev, ...) do { sp_log_prepend_formatted((sev), __VA_ARGS__); } while(0)

#ifdef __cplusplus
}
#endif

#endif /* SP_LOG__H */

