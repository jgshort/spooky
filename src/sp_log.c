#define _POSIX_C_SOURCE 200809L 

#include <assert.h>
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sp_log.h"

const size_t spooky_log_max_display_lines = 1024;
const size_t spooky_log_max_entry_capacity = 1024;

typedef struct spooky_log_entry spooky_log_entry;
typedef struct spooky_log_entry {
  spooky_log_entry * next;
  spooky_log_entry * prev;
  spooky_log_severity severity;
  char padding[4]; /* not portable */
  char * line;
  size_t line_len;
} spooky_log_entry;

typedef struct spooky_log_entries {
  spooky_log_entry * head;
  size_t count;
} spooky_log_entries;

typedef struct spooky_log_impl {
  spooky_log interface;
  spooky_log_entries * entries;
} spooky_log_impl;

static void spooky_log_prepend(const spooky_log * self, const char * line, spooky_log_severity severity);
static void spooky_log_list_prepend(spooky_log_entry * head, spooky_log_entry * line);
static void spooky_log_dump(const spooky_log * self, const spooky_console * console);
static size_t spooky_log_get_entries_count(const spooky_log * self);

static const spooky_log spooky_log_funcs = {
  .ctor = &spooky_log_ctor,
  .dtor = &spooky_log_dtor,
  .free = &spooky_log_free,
  .release = &spooky_log_release,
  .prepend = &spooky_log_prepend,
  .dump = &spooky_log_dump,
  .get_entries_count = &spooky_log_get_entries_count
};

const spooky_log * spooky_log_alloc() {
  const spooky_log_impl * self = calloc(1, sizeof * self);
  return (const spooky_log *)self;
}

const spooky_log * spooky_log_init(spooky_log * self) {
  assert(self != NULL);
  return memmove(self, &spooky_log_funcs, sizeof spooky_log_funcs);
}

const spooky_log * spooky_log_acquire() {
  return spooky_log_init((spooky_log *)(uintptr_t)spooky_log_alloc());
}

const spooky_log * spooky_log_ctor(const spooky_log * self) {
  spooky_log_impl * impl = (spooky_log_impl *)(uintptr_t)self;
  impl->entries = calloc(1, sizeof * impl->entries);
  return self;
}

const spooky_log * spooky_log_dtor(const spooky_log * self) {
  if(self) {
    spooky_log_impl * impl = (spooky_log_impl *)(uintptr_t)self;

    if(impl->entries->head != NULL) {
      spooky_log_entry * t = impl->entries->head->next;
      while(t != impl->entries->head) {
        if(t->line != NULL) {
          free(t->line), t->line = NULL;
        }
        spooky_log_entry * old = t;
        t = t->next;
        free(old), old = NULL;
      }
      free(impl->entries->head), impl->entries->head = NULL;
    }
    free(impl->entries), impl->entries = NULL;
  }
  return self;
}

void spooky_log_free(const spooky_log * self) {
  if(self) {
    free((void *)(uintptr_t)self), self = NULL;
  }
}

void spooky_log_release(const spooky_log * self) {
  self->free(self->dtor(self));
}

void spooky_log_list_prepend(spooky_log_entry * head, spooky_log_entry * line) {
  assert(line != NULL && head != NULL);
  line->next = head;
  line->prev = head->prev;
  head->prev->next = line;
  head->prev = line;
}

void spooky_log_prepend(const spooky_log * self, const char * line, spooky_log_severity severity) {
  spooky_log_impl * impl = (spooky_log_impl *)(uintptr_t)self;
   
  spooky_log_entry * entry = calloc(1, sizeof * entry);
  entry->line = strndup(line, spooky_log_max_entry_capacity);
  entry->line_len = strnlen(line, spooky_log_max_entry_capacity);
  entry->severity = severity;
  if(impl->entries->count > spooky_log_max_display_lines && impl->entries->head->next != NULL) {
    spooky_log_entry * deleted = impl->entries->head->next;
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
    spooky_log_list_prepend(impl->entries->head, entry);
  }
  impl->entries->count++;
}

void spooky_log_dump(const spooky_log * self, const spooky_console * console) {
  spooky_log_impl * impl = (spooky_log_impl *)(uintptr_t)self;
  if(impl->entries->head != NULL) {
    console->push_str(console, impl->entries->head->line);
    spooky_log_entry * t = impl->entries->head->next;
    while(t != impl->entries->head) {
      if(t->line != NULL) {
        console->push_str(console, t->line); 
      }
      t = t->next; 
    }
  }
}

size_t spooky_log_get_entries_count(const spooky_log * self) {
  spooky_log_impl * impl = (spooky_log_impl *)(uintptr_t)self;
  return impl->entries->count;
}

