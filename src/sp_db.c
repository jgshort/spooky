#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sqlite3.h>

#include "sp_db.h"

typedef struct spooky_db_data {
  sqlite3 * db;
  bool is_open;
  char padding[7];
} spooky_db_data;

static int spooky_db_open_storage(const spooky_db * db, char const * context_name);
static int spooky_db_close_storage(const spooky_db * db);

static char const * spooky_db_get_user_home() {
	char const * home = getenv("HOME");
	if(!home) {
		struct passwd * pw = getpwuid(getuid());
		home = pw->pw_dir;
	}

	return home;
}

static char const * spooky_db_get_config_path() {
	static char * config_path = NULL;
	if(!config_path) {
		char const * home = spooky_db_get_user_home();
		asprintf(&config_path, "%s/.config/spooky", home);
	}

	return config_path;
}

static void spooky_db_ensure_config_path() {
	char const * config_path = spooky_db_get_config_path();

	struct stat st = {0};
	if (stat(config_path, &st) == -1) {
		mkdir(config_path, 0700);
	}
}

static char * spooky_db_alloc_concat_path(char const * root_path, char const * path) {
	char * full_path = malloc(strnlen(root_path, 1024) + strnlen(path, 1024) + sizeof(char) + sizeof(char));
	if (!full_path) { abort(); }
	snprintf(full_path, 2048, "%s/%s", root_path, path);

	return full_path;
}

const spooky_db * spooky_db_alloc() {
  spooky_db * db = calloc(1, sizeof * db);

	return spooky_db_init((spooky_db *)db);
}

const spooky_db * spooky_db_init(spooky_db * self) {
  spooky_db_data * data = calloc(1, sizeof * data);

  self->ctor = &spooky_db_ctor;
  self->dtor = &spooky_db_dtor;
  self->release = &spooky_db_release;
  self->free = &spooky_db_free;

  self->open = &spooky_db_open_storage;
  self->create = &spooky_db_create_storage;
  self->close = &spooky_db_close_storage;

  data->db = NULL;
  data->is_open = false;

  self->data = data;

  return self;
}

const spooky_db * spooky_db_acquire() {
  return NULL;
}

const spooky_db * spooky_db_ctor(const spooky_db * self) {
  return self;
}

const spooky_db * spooky_db_dtor(const spooky_db * self) {
  return self;
}

void spooky_db_free(const spooky_db * self) {
  if(!self) { return; }

	self->close(self);
	if(self->data) {
		free(self->data);
	}
	free((void *)(uintptr_t)self), self = NULL;
}

void spooky_db_release(const spooky_db * self) {
  (void)self;
}

static int spooky_db_close_storage(const spooky_db * self) {
  int ret = -1;
	if(self && self->data) {
		spooky_db_data * data = self->data;
		if(data->is_open) {
			sqlite3 * db = data->db;
			if(db) {
				ret = sqlite3_close(db);
			}
			data->is_open = false;
		}
	}

	return ret == SQLITE_OK;;
}

static int spooky_db_open_storage(const spooky_db * self, const char * context_name) {
  if(!context_name) { context_name = "spooky.db"; }

	char * path = spooky_db_alloc_concat_path(spooky_db_get_config_path(), context_name);

	int rc = sqlite3_open(path, &(self->data->db));
	if(rc != SQLITE_OK) goto err0;

err0:
	free(path), path = NULL;
	return rc;
}

int spooky_db_create_storage(const char * context_name) {
  (void)spooky_db_open_storage;
  (void)spooky_db_close_storage;

  sqlite3_initialize();
  spooky_db_ensure_config_path();
  char * err_msg = NULL;

	if(!context_name) { context_name = "spooky.db"; }

	char const * config_path = spooky_db_get_config_path();
	char * path = spooky_db_alloc_concat_path(config_path, context_name);

  char * sql = NULL;
	sqlite3 * db = NULL;
	int rc = sqlite3_open(path, &db);
	if(rc != SQLITE_OK) {
		(void)db;
		goto err0;
	}

  sql = calloc(1024, sizeof * sql);

	snprintf(sql, 1024,
		"pragma encoding = utf8;" \
		"create table if not exists spooky_saves ("\
			"id integer primary key, name text unique not null, turns integer"\
		"); " \
		"create index if not exists spooky_saves_name_inx on spooky_saves(name);"
  );

	rc = sqlite3_exec(db, sql, NULL, 0, &err_msg);
	if(rc != SQLITE_OK) goto err1;

	goto err0;

err1:
	fprintf(stderr, "Failed to create context %s, '%s'\n", context_name, err_msg);

err0:
	sqlite3_close(db);
	free(path), path = NULL;

	if(sql) {
		free(sql), sql = NULL;
	}
  sqlite3_shutdown();
	return rc;
}
