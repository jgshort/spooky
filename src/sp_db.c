#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <memory.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sqlite3.h>

#include "sp_db.h"
#include "sp_types.h"

static const size_t spooky_db_max_path_len = 65536;

typedef struct spooky_db_data {
  const char * context_name;
  const char * config_path;
  const char * context_path;
  sqlite3 * db;
  bool is_open;
  char padding[7];
} spooky_db_data;

static int spooky_db_create_storage(const spooky_db * self);
static int spooky_db_open_storage(const spooky_db * self);
static int spooky_db_close_storage(const spooky_db * self);

static bool spooky_db_validate_table_name(const char * table_name);
static int spooky_db_get_table_count(const spooky_db * self, char const * table_name);
static int spooky_db_get_last_row_id(const spooky_db * self, const char * table_name);
static int spooky_db_load_game(const spooky_db * self, const char * name, spooky_save_game * state);
static int spooky_db_save_game(const spooky_db * self, const char * name, const spooky_save_game * state);

static char const * spooky_db_get_user_home();
static char const * spooky_db_alloc_config_path();
static void spooky_db_ensure_config_path(const spooky_db * self);
static char * spooky_db_alloc_concat_path(const char * root_path, char const * path);

static const spooky_db spooky_db_funcs = {
  .ctor = &spooky_db_ctor,
  .dtor = &spooky_db_dtor,
  .free = &spooky_db_free,
  .release = &spooky_db_release,

  .create = &spooky_db_create_storage,
  .open = &spooky_db_open_storage,
  .close = &spooky_db_close_storage,
  .load_game = &spooky_db_load_game,
  .save_game = &spooky_db_save_game,

  .get_table_count = &spooky_db_get_table_count,
  .get_last_row_id = &spooky_db_get_last_row_id
};

const spooky_db * spooky_db_alloc() {
  spooky_db * db = calloc(1, sizeof * db);
  assert(db);
	return db;
}

const spooky_db * spooky_db_init(spooky_db * self) {
  assert(self);
  return memmove(self, &spooky_db_funcs, sizeof spooky_db_funcs);
}

const spooky_db * spooky_db_acquire() {
  return spooky_db_init((spooky_db *)(uintptr_t)spooky_db_alloc());
}

const spooky_db * spooky_db_ctor(const spooky_db * self, const char * context_name) {
  spooky_db_data * data = calloc(1, sizeof * data);

  if(!context_name) { context_name = "spooky.db"; }

  sqlite3_initialize();

  data->context_name = context_name;
  data->config_path = spooky_db_alloc_config_path();
  data->context_path = spooky_db_alloc_concat_path(data->config_path, context_name);
  data->db = NULL;
  data->is_open = false;

  ((spooky_db *)(uintptr_t)self)->data = data;

  return self;
}

const spooky_db * spooky_db_dtor(const spooky_db * self) {
  if(self) {
	  self->close(self);
    sqlite3_shutdown();
    if(self->data) {
      free((char *)(uintptr_t)self->data->config_path), self->data->config_path = NULL;
      free((char *)(uintptr_t)self->data->context_path), self->data->context_path = NULL;
      free(self->data), ((spooky_db *)(uintptr_t)self)->data = NULL;
    }
  }
  return self;
}

void spooky_db_free(const spooky_db * self) {
  if(self) {
    free((void *)(uintptr_t)self), self = NULL;
  }
}

void spooky_db_release(const spooky_db * self) {
  if(self) {
    self->free(self->dtor(self));
  }
}

int spooky_db_create_storage(const spooky_db * self) {
  if(self->data->is_open) { return -1; }

  spooky_db_ensure_config_path(self);
  char * err_msg = NULL;

	const char * path = self->data->context_path;

  char * sql = NULL;
	sqlite3 * db = NULL;
	int res = sqlite3_open(path, &db);
	if(res != SQLITE_OK) { goto err0; }

  sql = calloc(2048, sizeof * sql);

  /* See sp_types.h for spooky_save_game_v1 */
	snprintf(sql, 2048,
		"pragma encoding = utf8;" \
    "pragma foreign_keys = on;" \
    "create table if not exists spooky_saves_v1 (" \
			"id integer primary key," \
      "seed integer," \
      "turns integer," \
      "deaths integer," \
      "x real," \
      "y real,"\
      "z real" \
		"); " \
		"create table if not exists spooky_saves (" \
			"id integer primary key," \
      "name text unique not null," \
      "save_game_version integer not null," \
      "v1_id integer unique not null,"
      "foreign key (v1_id) references spooky_saves_v1(id)" \
		"); " \
		"create index if not exists spooky_saves_name_inx on spooky_saves(name);"\
  );

	res = sqlite3_exec(db, sql, NULL, 0, &err_msg);
	if(res != SQLITE_OK) { goto err0; }

  sqlite3_close(db);
	if(sql) { free(sql), sql = NULL; }

  return res;

err0:
	fprintf(stderr, "Failed to create context %s, '%s'\n", path, err_msg);
  if(db) { sqlite3_close(db); }
	free(sql), sql = NULL;

  return res;
}

static int spooky_db_open_storage(const spooky_db * self) {
  if(self->data->is_open) { return -1; }

	const char * path = self->data->context_path;
  sqlite3 * db = NULL;
	int res = sqlite3_open(path, &db);
  if(res == SQLITE_OK) {
    self->data->db = db;
    self->data->is_open = true;
  }

  return res;
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
		} else {
      /* Closing a non-open db is okay */
      ret = SQLITE_OK;
    }
	}

	return ret;
}

static int spooky_db_get_last_row_id(const spooky_db * self, const char * table_name) {
  static const char statement[] = "select last_insert_rowid() from [%s];";

  if(!spooky_db_validate_table_name(table_name)) { return -1; }

  char * sql = calloc(1024, sizeof * sql);
  int sql_len = snprintf(sql, 1024, statement, table_name);
  assert(sql_len > -1);

	sqlite3_stmt * stmt = NULL;
	int res = sqlite3_prepare_v2(self->data->db, sql, -1, &stmt, NULL);
	if(res != SQLITE_OK) goto err0;

  int id = -1;
	while(sqlite3_step(stmt) == SQLITE_ROW) {
		id = sqlite3_column_int(stmt, 0);
	}

	sqlite3_finalize(stmt);
  free(sql), sql = NULL;

  return id;

err0:
  return -1;
}

static int spooky_db_get_table_count(const spooky_db * self, const char * table_name) {
  if(!spooky_db_validate_table_name(table_name)) { return -1; }

  static const size_t max_sql_len = 1024;
	char * sql = calloc(max_sql_len, sizeof * sql);
	snprintf(sql, max_sql_len, "select count(*) from [%s];", table_name);

	sqlite3_stmt * stmt = NULL;
  int res = sqlite3_prepare_v2(self->data->db, sql, -1, &stmt, NULL);
	if(res != SQLITE_OK) { goto err0; }

  int count = 0;
	while(sqlite3_step(stmt) == SQLITE_ROW) {
		count = sqlite3_column_int(stmt, 0);
	}

	sqlite3_finalize(stmt);
  free(sql), sql = NULL;

  return count;

err0:
	free(sql), sql = NULL;

	return -1;
}

static bool spooky_db_validate_table_name(const char * table_name) {
	/* Assumes *ACCEPTABLE_TABLES is max strlen() of ACCEPTABLE_TABLES[] */
	const char * ACCEPTABLE_TABLES[] = {  "spooky_saves", "spooky_saves_v1" };
	const size_t ACCEPTABLE_TABLES_LEN = sizeof ACCEPTABLE_TABLES / sizeof * ACCEPTABLE_TABLES;
	const size_t MAX_TABLE_NAME_LEN = 1024;

	if(!table_name) { return false; }

	bool found = false;
	for(size_t i = 0; i < ACCEPTABLE_TABLES_LEN; i++) {
		if(strncmp(table_name, ACCEPTABLE_TABLES[i], MAX_TABLE_NAME_LEN) == 0) {
			found = true;
			break;
		}
	}

	return found;
}

static int spooky_db_load_game_v1(const spooky_db * self, spooky_save_game_v1 * state)  {
  static char const * sql = "select seed, turns, deaths, x, y, z from spooky_saves_v1 where id = ?;";

	sqlite3_stmt * stmt = NULL;
	int rc = sqlite3_prepare_v2(self->data->db, sql, -1, &stmt, NULL);
	if(rc != SQLITE_OK) goto err0;

	rc = sqlite3_bind_int(stmt, 1, state->base.v1_id);
	if(rc != SQLITE_OK) goto err1;

	rc = sqlite3_step(stmt);
	if(rc == SQLITE_ROW) {
    state->seed = sqlite3_column_int(stmt, 0);
    state->turns = sqlite3_column_int(stmt, 1);
    state->deaths = sqlite3_column_int(stmt, 2);
    state->x = sqlite3_column_double(stmt, 3);
    state->y = sqlite3_column_double(stmt, 4);
    state->z = sqlite3_column_double(stmt, 5);
  }

  sqlite3_finalize(stmt);

  return SQLITE_OK;

err0:

err1:
  return -1;
}

static int spooky_db_load_game(const spooky_db * self, const char * name, spooky_save_game * state) {
  static char const * sql = "select name, save_game_version, v1_id from spooky_saves where name = ?;";

	sqlite3_stmt * stmt = NULL;
	int res = sqlite3_prepare_v2(self->data->db, sql, -1, &stmt, NULL);
	if(res != SQLITE_OK) { goto err0; }

	res = sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
	if(res != SQLITE_OK) { goto err1; }

	res = sqlite3_step(stmt);
	if(res == SQLITE_ROW) {
	  char const * save_game_name = (char const *)sqlite3_column_text(stmt, 0);

    state->name = strndup(save_game_name, 1024);
    state->save_game_version = sqlite3_column_int(stmt, 1);
    state->v1_id = sqlite3_column_int(stmt, 2);

    if(state->save_game_version == 1) {
      sqlite3_finalize(stmt);
      return spooky_db_load_game_v1(self, (spooky_save_game_v1 *)state);
    } else {
      fprintf(stderr, "Unsupported game save version found.\n");
      abort();
    }
	} else { goto err1; }

  return SQLITE_OK;

err1:
  fprintf(stderr, "Unable to bind name.\n");
	sqlite3_finalize(stmt);
  return res;
err0:
  fprintf(stderr, "Unable to prepare SQL.\n");
	return res;
}

static int spooky_db_save_game_v1(const spooky_db * self, const spooky_save_game_v1 * state, int * id) {
	static char const * sql = "insert into spooky_saves_v1 (seed, turns, deaths, x, y, z) values (?, ?, ?, ?, ?, ?);";
	if(!state) return -1;

	sqlite3_stmt * stmt = NULL;
	int res = sqlite3_prepare_v2(self->data->db, sql, -1, &stmt, NULL);
	if(res != SQLITE_OK) goto err0;

  res = sqlite3_bind_int(stmt, 1, state->seed);
	if(res != SQLITE_OK) { goto err1; }

  res = sqlite3_bind_int(stmt, 2, state->turns);
	if(res != SQLITE_OK) { goto err2; }

  res = sqlite3_bind_int(stmt, 3, state->deaths);
	if(res != SQLITE_OK) { goto err3; }

  res = sqlite3_bind_double(stmt, 4, state->x);
	if(res != SQLITE_OK) { goto err4; }

  res = sqlite3_bind_double(stmt, 5, state->y);
	if(res != SQLITE_OK) { goto err5; }

  res = sqlite3_bind_double(stmt, 6, state->z);
	if(res != SQLITE_OK) { goto err6; }

	res = sqlite3_step(stmt);
	if(res != SQLITE_DONE) { goto err0; }

  res = sqlite3_finalize(stmt);
  stmt = NULL;
  if(res != SQLITE_OK) { goto err0; }

  *id = self->get_last_row_id(self, "spooky_saves_v1");
  assert(*id > -1);

  res = SQLITE_OK;
  return res;

err6:
  fprintf(stderr, "Unable to bind 'z' on save.\n");
  goto err0;
err5:
  fprintf(stderr, "Unable to bind 'y' on save.\n");
  goto err0;
err4:
  fprintf(stderr, "Unable to bind 'x' on save.\n");
  goto err0;
err3:
  fprintf(stderr, "Unable to bind 'deaths' on save.\n");
  goto err0;
err2:
  fprintf(stderr, "Unable to bind 'turns' on save.\n");
  goto err0;
err1:
  fprintf(stderr, "Unable to bind 'seed' on save.\n");
  goto err0;
err0:
  fprintf(stderr, "Unable to execute insert query.\n");
  return res;
}

static int spooky_db_save_game(const spooky_db * self, const char * name, const spooky_save_game * state) {
	if(!state) return -1;

  int id = -1, res = -1;
  if(state->save_game_version == 1) {
    res = spooky_db_save_game_v1(self, (const spooky_save_game_v1 *)state, &id);
  } else {
    fprintf(stderr, "Unknown save game version encountered.\n");
    return -1;
  }

  assert(id >= 0 && res == 0);

  static char const * sql = "insert into spooky_saves (name, save_game_version, v1_id) values (?, ?, ?) on conflict (name) do update set save_game_version = excluded.save_game_version, v1_id = excluded.v1_id;";

	sqlite3_stmt * stmt = NULL;
	res = sqlite3_prepare_v2(self->data->db, sql, -1, &stmt, NULL);
	if(res != SQLITE_OK) { goto err0; }

  res = sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
	if(res != SQLITE_OK) { goto err1; }

  res = sqlite3_bind_int(stmt, 2, state->save_game_version);
	if(res != SQLITE_OK) { goto err2; }

  res = sqlite3_bind_int(stmt, 3, id);
	if(res != SQLITE_OK) { goto err3; }

	res = sqlite3_step(stmt);
	if(res != SQLITE_DONE) { goto err0; }

  res = sqlite3_finalize(stmt);
  stmt = NULL;
  if(res != SQLITE_OK) { goto err0; }

  return SQLITE_OK;

err3:
  fprintf(stderr, "Unable to bind 'v1' on save.\n");
  goto err0;
err2:
  fprintf(stderr, "Unable to bind 'version' on save.\n");
  goto err0;
err1:
  fprintf(stderr, "Unable to bind 'name' on save.\n");
  goto err0;
err0:
  return -1;
}

static const char * spooky_db_get_user_home() {
	char const * home = getenv("HOME");
	if(!home) {
		struct passwd * pw = getpwuid(getuid());
		home = pw->pw_dir;
	}

	return home;
}

static const char * spooky_db_alloc_config_path() {
  char const * home = spooky_db_get_user_home();
  size_t home_len = strnlen(home, spooky_db_max_path_len);
  assert(home_len < spooky_db_max_path_len);

  size_t sub_path_len = strnlen("/.config/spooky", 32) + 1 /* '\0' term */;
  assert(home_len + sub_path_len < spooky_db_max_path_len);

  char * temp = calloc(home_len + sub_path_len, sizeof * temp);
  assert(temp);
  if(!temp) { abort(); }

  char * config_path = temp;
  int config_path_len = snprintf(config_path, spooky_db_max_path_len, "%s/.config/spooky", home);
  assert(config_path_len > 0 && (size_t)config_path_len < spooky_db_max_path_len);

	return config_path;
}

static void spooky_db_ensure_config_path(const spooky_db * self) {
	char const * config_path = self->data->config_path;

	struct stat st = { 0 };
	if(stat(config_path, &st) == -1) {
		mkdir(config_path, 0700);
	}
}

static char * spooky_db_alloc_concat_path(const char * root_path, const char * path) {
  size_t alloc_len = strnlen(root_path, spooky_db_max_path_len) + sizeof(char /* '/' */) + strnlen(path, spooky_db_max_path_len) + sizeof(char /* '\0' */);
	char * full_path = calloc(alloc_len, sizeof * full_path);
	if (!full_path) { abort(); }

	int full_path_len = snprintf(full_path, spooky_db_max_path_len, "%s/%s", root_path, path);
  assert(full_path_len > 0 && (size_t)full_path_len < spooky_db_max_path_len);

  return full_path;
}
