#ifndef SP_LOG__H
#define SP_LOG__H

#include "sp_console.h"

typedef struct spooky_log spooky_log;

typedef enum spooky_log_severity {
  SLS_INFO,
  SLS_DEBUG,
  SLS_WARN,
  SLS_ERROR,
  SLS_CRITICAL
} spooky_log_severity;

typedef struct spooky_log {
  const spooky_log * (*ctor)(const spooky_log * self);
  const spooky_log * (*dtor)(const spooky_log * self);
  void (*free)(const spooky_log * self);
  void (*release)(const spooky_log * self);

  void (*prepend)(const spooky_log * self, const char * line, spooky_log_severity severity);
  void (*dump)(const spooky_log * self, const spooky_console * console);
  size_t (*get_entries_count)(const spooky_log * self);
} spooky_log;


const spooky_log * spooky_log_init(spooky_log * self);
const spooky_log * spooky_log_alloc();
const spooky_log * spooky_log_acquire();
const spooky_log * spooky_log_ctor(const spooky_log * self);
const spooky_log * spooky_log_dtor(const spooky_log * self);
void spooky_log_free(const spooky_log * self);
void spooky_log_release(const spooky_log * self);

#endif /* SP_LOG__H */

