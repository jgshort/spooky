#ifdef __STDC_ALLOC_LIB__
#define __STDC_WANT_LIB_EXT2__ 1
#else
#define _POSIX_C_SOURCE 200809L
#endif

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

#include "../include/sp_io.h"
#include "../include/sp_z.h"
#include "../include/sp_error.h"

static const size_t SP_IO_MAX_PATH_LEN = 65536;

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

    home_len = strnlen(home, SP_IO_MAX_PATH_LEN);
  }

  *path_len = home_len;
  return home;
}

char * spooky_io_alloc_config_path(void) {
  static const char * default_path = "/.config/pkin";

  size_t home_len = 0;
  const char * home = spooky_io_get_user_home(&home_len);
  size_t default_path_len = strnlen(default_path, SP_IO_MAX_PATH_LEN);
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
  char * full_path = calloc(strnlen(root_path, SP_IO_MAX_PATH_LEN) + strnlen(path, SP_IO_MAX_PATH_LEN) + sizeof('\0'), sizeof * full_path);
  if (!full_path) { abort(); }

  int allocated = snprintf(full_path, SP_IO_MAX_PATH_LEN, "%s%s", root_path, path);
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
  SP_SET_BINARY_MODE(fp);
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
  SP_SET_BINARY_MODE(fp);
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

static const char * spooky_io_gen_io_ex_msg(const char * start, const char * path, const char * end) {
  static const size_t max_block_len = 256;
  static char buf[1024] = { 0 };

  assert(start && path);

  size_t start_len = strnlen(start, max_block_len);
  size_t path_len = strnlen(path, max_block_len);
  size_t end_len = strnlen(end, max_block_len);

  assert((start_len + path_len + end_len + 1) < (sizeof buf / sizeof buf[0]));
  memcpy(buf, start, strnlen(start, max_block_len));

  size_t buf_len = strnlen(start, max_block_len);
  memcpy(buf + buf_len, path, path_len);

  if(end) {
    memcpy(buf + buf_len + path_len, end, strlen(end));
  }

  buf[start_len + path_len + end_len] = '\0';

  return buf;
}

errno_t spooky_io_read_buffer_from_file(const char * path, char ** buffer, const spooky_ex ** ex) {
  FILE *fp = fopen(path, "r");
  if(errno || !fp) { goto err1; }

  fseek(fp, 0L, SEEK_END);
  if(errno) { goto err2; }

  const long temp_file_size = ftell(fp);
  if(errno || temp_file_size < 0) { goto err3; }

  const unsigned long file_size = (unsigned long)temp_file_size;
  rewind(fp);
  if(errno) { goto err4; }

  char * temp = (char*)malloc((sizeof * temp) * (file_size + 1));
  if (!temp) { goto err5; }

  size_t bytesRead = fread(temp, sizeof * temp, file_size, fp);
  if (bytesRead != file_size) { goto err6; }
  temp[bytesRead] = '\0';

  int ret = fclose(fp);
  if(ret || ret == EOF || errno) { goto err7; }

  *buffer = temp;
  return SP_SUCCESS;

err7: /* Failure to close the file. */
  goto err_free;

err6: /* Failure to read to end (bytesRead != file_size). */
  goto err_free;

err_free: /* Ensure freed buffer on post-allocation failures. */
  free(temp), temp = NULL;
  goto err;

err5: /* malloc failure. */
  spooky_ex_new(__LINE__, __FILE__, -1, "Out-of-memory exception reading file contents to buffer.", NULL, ex);
  goto err;

err4: /* Rewind failure (I/O failure)*/
err3: /* ftell failure (I/O failure) */
  goto err2;

/* fseek failure */
err2:
  {
    const char * buf = spooky_io_gen_io_ex_msg("I/O exception reading file path '", path, "'.");
    spooky_ex_new(__LINE__, __FILE__, -1, buf, NULL, ex);
    goto err;
  }

/* Unable to open file. */
err1:
  {
    const char * buf = spooky_io_gen_io_ex_msg("Error reading file path '", path, "'.");
    spooky_ex_new(__LINE__, __FILE__, -1, buf, NULL, ex);
    goto err;
  }

err:
  return SP_FAILURE;
}

