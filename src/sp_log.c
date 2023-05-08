#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../include/sp_log.h"

const size_t sp_log_max_display_lines = 1024;
const size_t sp_log_max_entry_capacity = 1024;

typedef struct sp_log_entry sp_log_entry;
typedef struct sp_log_entry {
  sp_log_entry * next;
  sp_log_entry * prev;
  sp_log_severity severity;
  char padding[4]; /* not portable */
  char * line;
  size_t line_len;
} sp_log_entry;

typedef struct sp_log_entries {
  sp_log_entry * head;
  size_t count;
} sp_log_entries;

typedef struct sp_log_impl {
  sp_log interface;
  sp_log_entries * entries;
} sp_log_impl;

static void sp_log_prepend(const sp_log * self, const char * line, sp_log_severity severity);
static void sp_log_list_prepend(sp_log_entry * head, sp_log_entry * line);
static void sp_log_dump(const sp_log * self, const sp_console * console);
static size_t sp_log_get_entries_count(const sp_log * self);

static const sp_log sp_log_funcs = {
  .ctor = &sp_log_ctor,
  .dtor = &sp_log_dtor,
  .free = &sp_log_free,
  .release = &sp_log_release,
  .prepend = &sp_log_prepend,
  .dump = &sp_log_dump
};

const sp_log * sp_log_global_log = NULL;

void sp_log_startup(void) {
  if(!sp_log_global_log) {
    const sp_log * log = sp_log_acquire();
    log = log->ctor(log);
    sp_log_global_log = log;
  }
}

void sp_log_prepend_formatted(sp_log_severity severity, const char * format, ...) {
  char buffer[1024];
  va_list args;
  va_start(args, format);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
  int written = vsnprintf(buffer, sizeof buffer, format, args);
  if(written < 0) { abort(); }
#pragma GCC diagnostic pop
  va_end (args);

  sp_log_prepend(sp_log_global_log, buffer, severity);
}

void sp_log_shutdown(void) {
  if(sp_log_global_log) {
    sp_log_release(sp_log_global_log);
  }
}

const sp_log * sp_log_alloc(void) {
  const sp_log_impl * self = calloc(1, sizeof * self);
  return (const sp_log *)self;
}

const sp_log * sp_log_init(sp_log * self) {
  assert(self != NULL);
  return memmove(self, &sp_log_funcs, sizeof sp_log_funcs);
}

const sp_log * sp_log_acquire(void) {
  return sp_log_init((sp_log *)(uintptr_t)sp_log_alloc());
}

const sp_log * sp_log_ctor(const sp_log * self) {
  sp_log_impl * impl = (sp_log_impl *)(uintptr_t)self;
  impl->entries = calloc(1, sizeof * impl->entries);
  return self;
}

const sp_log * sp_log_dtor(const sp_log * self) {
  if(self) {
    sp_log_impl * impl = (sp_log_impl *)(uintptr_t)self;

    if(impl->entries->head != NULL) {
      sp_log_entry * t = impl->entries->head->next;
      while(t != impl->entries->head) {
        if(t->line != NULL) {
          free(t->line), t->line = NULL;
        }
        sp_log_entry * old = t;
        t = t->next;
        free(old), old = NULL;
      }
      free(impl->entries->head), impl->entries->head = NULL;
    }
    free(impl->entries), impl->entries = NULL;
  }
  return self;
}

void sp_log_free(const sp_log * self) {
  if(self) {
    free((void *)(uintptr_t)self), self = NULL;
  }
}

void sp_log_release(const sp_log * self) {
  self->free(self->dtor(self));
}

void sp_log_list_prepend(sp_log_entry * head, sp_log_entry * line) {
  assert(line != NULL && head != NULL);
  line->next = head;
  line->prev = head->prev;
  head->prev->next = line;
  head->prev = line;
}

void sp_log_prepend(const sp_log * self, const char * line, sp_log_severity severity) {
  sp_log_impl * impl = (sp_log_impl *)(uintptr_t)self;

  sp_log_entry * entry = calloc(1, sizeof * entry);
  entry->line = strndup(line, sp_log_max_entry_capacity);
  entry->line_len = strnlen(line, sp_log_max_entry_capacity);
  entry->severity = severity;
  if(impl->entries->count > sp_log_max_display_lines && impl->entries->head->next != NULL) {
    sp_log_entry * deleted = impl->entries->head->next;
    assert(deleted != NULL && deleted->next != NULL);
    if(deleted->next != NULL) {
      deleted->next->prev = impl->entries->head;
      impl->entries->head->next = deleted->next;
    }
    free(deleted), deleted = NULL;
    impl->entries->count--;
  }
  if(impl->entries->head == NULL) {
    entry->next = entry;
    entry->prev = entry;
    impl->entries->head = entry;
  } else {
    sp_log_list_prepend(impl->entries->head, entry);
  }
  impl->entries->count++;
}

void sp_log_dump(const sp_log * self, const sp_console * console) {
  sp_log_impl * impl = (sp_log_impl *)(uintptr_t)self;
  if(impl->entries->head != NULL) {
    console->push_str(console, impl->entries->head->line);
    sp_log_entry * t = impl->entries->head->next;
    while(t != impl->entries->head) {
      if(t->line != NULL) {
        console->push_str(console, t->line);
      }
      t = t->next;
    }
  }
}

size_t sp_log_get_entries_count(const sp_log * self) {
  sp_log_impl * impl = (sp_log_impl *)(uintptr_t)self;
  return impl->entries->count;
}

void sp_log_dump_to_console(const sp_console * console) {
  sp_log_global_log->dump(sp_log_global_log, console);
}

size_t sp_log_get_global_entries_count(void) {
  return sp_log_get_entries_count(sp_log_global_log);
}

