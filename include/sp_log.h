#ifndef SP_LOG__H
#define SP_LOG__H

#include "sp_console.h"

typedef enum spooky_log_severity {
  SLS_INFO,
  SLS_DEBUG,
  SLS_WARN,
  SLS_ERROR,
  SLS_CRITICAL
} spooky_log_severity;

typedef struct spooky_log spooky_log;
typedef struct spooky_log {
  const spooky_log * (*ctor)(const spooky_log * self);
  const spooky_log * (*dtor)(const spooky_log * self);
  void (*free)(const spooky_log * self);
  void (*release)(const spooky_log * self);

  void (*prepend)(const spooky_log * self, const char * line, spooky_log_severity severity);
  void (*dump)(const spooky_log * self, const spooky_console * console);
} spooky_log;

const spooky_log * spooky_log_init(spooky_log * self);
const spooky_log * spooky_log_alloc(void);
const spooky_log * spooky_log_acquire(void);
const spooky_log * spooky_log_ctor(const spooky_log * self);
const spooky_log * spooky_log_dtor(const spooky_log * self);
void spooky_log_free(const spooky_log * self);
void spooky_log_release(const spooky_log * self);

void spooky_log_startup(void);
void spooky_log_prepend_formatted(spooky_log_severity severity, const char * format, ...);
void spooky_log_shutdown(void);
void spooky_log_dump_to_console(const spooky_console * console);
size_t spooky_log_get_global_entries_count(void);

#define SP_LOG(sev, ...) do { spooky_log_prepend_formatted((sev), __VA_ARGS__); } while(0)

#endif /* SP_LOG__H */

