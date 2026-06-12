/*
 * ZBC Semihosting Dummy Backend
 *
 * A no-op backend that returns success for all operations.
 * Useful for testing or when semihosting output is not needed.
 */

#include "zbc_semihost.h"

static int dummy_open(void *ctx, const char *path, size_t path_len, int mode) {
  (void)ctx;
  (void)path;
  (void)path_len;
  (void)mode;
  return 3; /* Return fd 3 to avoid stdin/stdout/stderr */
}

static int dummy_close(void *ctx, int fd) {
  (void)ctx;
  (void)fd;
  return 0;
}

static int dummy_read(void *ctx, int fd, void *buf, size_t count) {
  (void)ctx;
  (void)fd;
  (void)buf;
  return (int)count; /* No bytes read (EOF) */
}

static int dummy_write(void *ctx, int fd, const void *buf, size_t count) {
  (void)ctx;
  (void)fd;
  (void)buf;
  (void)count;
  return 0; /* All bytes "written" */
}

static int dummy_seek(void *ctx, int fd, int pos) {
  (void)ctx;
  (void)fd;
  (void)pos;
  return 0;
}

static intmax_t dummy_flen(void *ctx, int fd) {
  (void)ctx;
  (void)fd;
  return 0;
}

static int dummy_remove(void *ctx, const char *path, size_t path_len) {
  (void)ctx;
  (void)path;
  (void)path_len;
  return 0;
}

static int dummy_rename(void *ctx, const char *old_path, size_t old_len,
                        const char *new_path, size_t new_len) {
  (void)ctx;
  (void)old_path;
  (void)old_len;
  (void)new_path;
  (void)new_len;
  return 0;
}

static int dummy_tmpnam(void *ctx, char *buf, size_t buf_size, int id) {
  (void)ctx;
  if (buf_size >= 8) {
    buf[0] = 't';
    buf[1] = 'm';
    buf[2] = 'p';
    buf[3] = (char)('0' + (id / 100) % 10);
    buf[4] = (char)('0' + (id / 10) % 10);
    buf[5] = (char)('0' + id % 10);
    buf[6] = '\0';
    return 0;
  }
  return -1;
}

static void dummy_writec(void *ctx, char c) {
  (void)ctx;
  (void)c;
}

static void dummy_write0(void *ctx, const char *str) {
  (void)ctx;
  (void)str;
}

static int dummy_readc(void *ctx) {
  (void)ctx;
  return -1; /* EOF */
}

static int dummy_iserror(void *ctx, int status) {
  (void)ctx;
  (void)status;
  return 0; /* Not an error */
}

static int dummy_istty(void *ctx, int fd) {
  (void)ctx;
  (void)fd;
  return 0; /* Not a TTY */
}

static int dummy_clock(void *ctx) {
  (void)ctx;
  return 0;
}

static int dummy_time(void *ctx) {
  (void)ctx;
  return 0;
}

static int dummy_elapsed(void *ctx, unsigned int *lo, unsigned int *hi) {
  (void)ctx;
  *lo = 0;
  *hi = 0;
  return 0;
}

static int dummy_tickfreq(void *ctx) {
  (void)ctx;
  return 100; /* 100 Hz */
}

static int dummy_do_system(void *ctx, const char *cmd, size_t cmd_len) {
  (void)ctx;
  (void)cmd;
  (void)cmd_len;
  return 0;
}

static int dummy_get_cmdline(void *ctx, char *buf, size_t buf_size) {
  (void)ctx;
  if (buf_size > 0) {
    buf[0] = '\0';
  }
  return 0; /* Empty command line */
}

static int dummy_heapinfo(void *ctx, uintptr_t *heap_base,
                          uintptr_t *heap_limit, uintptr_t *stack_base,
                          uintptr_t *stack_limit) {
  (void)ctx;
  *heap_base = 0;
  *heap_limit = 0;
  *stack_base = 0;
  *stack_limit = 0;
  return 0;
}

static void dummy_do_exit(void *ctx, unsigned int reason,
                          unsigned int subcode) {
  (void)ctx;
  (void)reason;
  (void)subcode;
  /* No action */
}

static int dummy_get_errno(void *ctx) {
  (void)ctx;
  return 0;
}

static int dummy_timer_config(void *ctx, unsigned int rate_hz) {
  (void)ctx;
  (void)rate_hz;
  return 0;
}

static int dummy_stat(void *ctx, const char *path, size_t path_len,
                      void *stat_buf) {
  size_t i;
  uint8_t *out = (uint8_t *)stat_buf;
  (void)ctx;
  (void)path;
  (void)path_len;
  /* Fill 48 bytes of zeroes so callers see a syntactically valid
   * (if semantically empty) response. */
  for (i = 0; i < SH_STAT_BUF_SIZE; i++) {
    out[i] = 0;
  }
  return 0;
}

static int dummy_opendir(void *ctx, const char *path, size_t path_len) {
  (void)ctx;
  (void)path;
  (void)path_len;
  return 1; /* any non-negative handle */
}

static int dummy_readdir(void *ctx, int handle, void *buf, size_t buf_size) {
  (void)ctx;
  (void)handle;
  (void)buf;
  (void)buf_size;
  return 0; /* end of directory immediately */
}

static int dummy_closedir(void *ctx, int handle) {
  (void)ctx;
  (void)handle;
  return 0;
}

static int dummy_readc_poll(void *ctx) {
  (void)ctx;
  return -1; /* no character available */
}

static int dummy_fstat(void *ctx, int fd, void *stat_buf) {
  size_t i;
  uint8_t *out = (uint8_t *)stat_buf;
  (void)ctx;
  (void)fd;
  for (i = 0; i < SH_STAT_BUF_SIZE; i++) {
    out[i] = 0;
  }
  return 0;
}

static int dummy_mkdir(void *ctx, const char *path, size_t path_len,
                       int mode) {
  (void)ctx;
  (void)path;
  (void)path_len;
  (void)mode;
  return 0;
}

static int dummy_rmdir(void *ctx, const char *path, size_t path_len) {
  (void)ctx;
  (void)path;
  (void)path_len;
  return 0;
}

static int dummy_ftruncate(void *ctx, int fd, uint64_t length) {
  (void)ctx;
  (void)fd;
  (void)length;
  return 0;
}

static int dummy_fsync(void *ctx, int fd) {
  (void)ctx;
  (void)fd;
  return 0;
}

static int dummy_link(void *ctx, const char *old_path, size_t old_len,
                      const char *new_path, size_t new_len) {
  (void)ctx;
  (void)old_path;
  (void)old_len;
  (void)new_path;
  (void)new_len;
  return 0;
}

static int dummy_symlink(void *ctx, const char *target, size_t target_len,
                         const char *linkpath, size_t linkpath_len) {
  (void)ctx;
  (void)target;
  (void)target_len;
  (void)linkpath;
  (void)linkpath_len;
  return 0;
}

static int dummy_readlink(void *ctx, const char *path, size_t path_len,
                          void *buf, size_t buf_size) {
  (void)ctx;
  (void)path;
  (void)path_len;
  (void)buf;
  (void)buf_size;
  return 0; /* no bytes written -- empty target */
}

static int dummy_lstat(void *ctx, const char *path, size_t path_len,
                       void *stat_buf) {
  size_t i;
  uint8_t *out = (uint8_t *)stat_buf;
  (void)ctx;
  (void)path;
  (void)path_len;
  for (i = 0; i < SH_STAT_BUF_SIZE; i++) {
    out[i] = 0;
  }
  return 0;
}

static const zbc_backend_t dummy_backend = {
    dummy_open,        dummy_close,    dummy_read,        dummy_write,
    dummy_seek,        dummy_flen,     dummy_remove,      dummy_rename,
    dummy_tmpnam,      dummy_writec,   dummy_write0,      dummy_readc,
    dummy_iserror,     dummy_istty,    dummy_clock,       dummy_time,
    dummy_elapsed,     dummy_tickfreq, dummy_do_system,   dummy_get_cmdline,
    dummy_heapinfo,    dummy_do_exit,  dummy_get_errno,   dummy_timer_config,
    dummy_stat,        dummy_opendir,  dummy_readdir,     dummy_closedir,
    dummy_readc_poll,  dummy_fstat,    dummy_mkdir,       dummy_rmdir,
    dummy_ftruncate,   dummy_fsync,    dummy_link,        dummy_symlink,
    dummy_readlink,    dummy_lstat};

const zbc_backend_t *zbc_backend_dummy(void) { return &dummy_backend; }
