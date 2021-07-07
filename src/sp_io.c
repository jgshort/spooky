#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include "sp_io.h"
#include "sp_z.h"

static const size_t SPOOKY_IO_MAX_PATH_LEN = 65536;

static const char * spooky_io_get_user_home(size_t * path_len) {
	static const char * home = NULL;
  static size_t home_len = 0;

	if(!home) {
    home = getenv("HOME");
    if(!home) {
      struct passwd * pw = getpwuid(getuid());
      if(pw) {
        home = pw->pw_dir;
      }
      if(!home) { home = "./"; }
    }

    home_len = strnlen(home, SPOOKY_IO_MAX_PATH_LEN);
	}

  *path_len = home_len;
	return home;
}

char * spooky_io_alloc_config_path() {
  static const char * default_path = "/.config/pkin";

  size_t home_len = 0;
  const char * home = spooky_io_get_user_home(&home_len);
  size_t default_path_len = strnlen(default_path, SPOOKY_IO_MAX_PATH_LEN);
  char * config_path = calloc(home_len + default_path_len + 1, sizeof * config_path);
  if(!config_path) { abort(); }

  snprintf(config_path, home_len + default_path_len + 1, "%s%s", home, default_path);
  config_path[home_len + default_path_len] = '\0';

  return config_path;
}

void spooky_io_ensure_path(const char * path, mode_t mode) {
	struct stat st = { 0 };
	if (stat(path, &st) == -1) {
		mkdir(path, mode);
	}
}

char * spooky_io_alloc_concat_path(char const * root_path, char const * path) {
	char * full_path = calloc(strnlen(root_path, SPOOKY_IO_MAX_PATH_LEN) + strnlen(path, SPOOKY_IO_MAX_PATH_LEN) + sizeof('\0'), sizeof * full_path);
	if (!full_path) { abort(); }

  int allocated = snprintf(full_path, SPOOKY_IO_MAX_PATH_LEN, "%s%s", root_path, path);
  assert(allocated >= 0);
  full_path[allocated] = '\0';

	return full_path;
}

FILE * spooky_io_open_or_create_binary_file_for_writing(const char * path, int * fd_out) {
  int fd = open(path, O_CREAT | O_RDWR | O_EXCL, S_IRUSR | S_IWUSR);
  if(fd < 0) {
    /* already exists; open it */
    if(errno == EEXIST) {
      fd = open(path, O_RDWR | O_EXCL, S_IRUSR | S_IWUSR);
    } else {
      *fd_out = fd;
      return NULL;
    }
  } else {
    /* doesn't already exist; empty file */
  }

  FILE * fp = fdopen(fd, "wb+x");
  SPOOKY_SET_BINARY_MODE(fp);
  fseek(fp, 0, SEEK_SET);

  *fd_out = fd;

  return fp;
}

FILE * spooky_io_open_binary_file_for_reading(const char * path, int * fd_out) {
  FILE * fp = NULL;

  int fd = open(path, O_RDWR | O_EXCL, S_IRUSR | S_IWUSR);

  if(fd < 0) {
    *fd_out = fd;
    return NULL;
  }

  fp = fdopen(fd, "rb");
  SPOOKY_SET_BINARY_MODE(fp);
  fseek(fp, 0, SEEK_SET);

  *fd_out = fd;

  return fp;
}
/*
int spooky_io_context_exists(char const * context_name) {
	char const * config_path = get_config_path();
	char * full_path = alloc_concat_path(config_path, context_name);

	struct stat st = {0};
	int exists = stat(full_path, &st) == 0;

	free(full_path), full_path = NULL;

	return exists;
}
*/
