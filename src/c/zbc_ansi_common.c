/*
 * ZBC Semihosting ANSI Backend - Common Functions
 *
 * Shared code used by both secure and insecure backends:
 * - FD management (alloc/free/get)
 * - Mode string conversion
 * - Console I/O (writec, write0, readc)
 * - Time functions (clock, time, tickfreq)
 * - Error/heap/cmdline stubs
 */

#include "zbc_ansi_internal.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#ifndef _WIN32
#include <dirent.h>
#include <sys/select.h>
#include <unistd.h>
#endif

/*========================================================================
 * FD Management
 *========================================================================*/

void zbc_ansi_fd_init(zbc_ansi_fd_state_t *fd) {
  int i;

  fd->next_fd = ZBC_ANSI_FIRST_FD;
  fd->free_fd_list = NULL;

  for (i = 0; i < ZBC_ANSI_MAX_FILES; i++) {
    fd->files[i] = NULL;
    fd->fd_pool[i].fd = 0;
    fd->fd_pool[i].next = NULL;
  }
}

int zbc_ansi_fd_alloc(zbc_ansi_fd_state_t *fd, FILE *fp) {
  int fd_num;
  int idx;
  zbc_ansi_fd_node_t *node;

  if (fd->free_fd_list != NULL) {
    /* Reuse from free list (LIFO) */
    node = fd->free_fd_list;
    fd_num = node->fd;
    fd->free_fd_list = node->next;
    node->fd = 0;
    node->next = NULL;
  } else {
    /* Allocate new */
    fd_num = fd->next_fd++;
  }

  idx = fd_num - ZBC_ANSI_FIRST_FD;
  if (idx < 0 || idx >= ZBC_ANSI_MAX_FILES) {
    return -1; /* Out of FDs */
  }

  fd->files[idx] = fp;
  return fd_num;
}

void zbc_ansi_fd_free(zbc_ansi_fd_state_t *fd, int fd_num) {
  int idx;
  int pool_idx;

  idx = fd_num - ZBC_ANSI_FIRST_FD;
  if (idx < 0 || idx >= ZBC_ANSI_MAX_FILES) {
    return;
  }

  fd->files[idx] = NULL;

  /* Find a free pool slot and add to free list */
  for (pool_idx = 0; pool_idx < ZBC_ANSI_MAX_FILES; pool_idx++) {
    if (fd->fd_pool[pool_idx].fd == 0 || fd->fd_pool[pool_idx].fd == fd_num) {
      fd->fd_pool[pool_idx].fd = fd_num;
      fd->fd_pool[pool_idx].next = fd->free_fd_list;
      fd->free_fd_list = &fd->fd_pool[pool_idx];
      break;
    }
  }
}

FILE *zbc_ansi_fd_get(zbc_ansi_fd_state_t *fd, int fd_num) {
  int idx;

  /* Handle stdin/stdout/stderr */
  if (fd_num == 0)
    return stdin;
  if (fd_num == 1)
    return stdout;
  if (fd_num == 2)
    return stderr;

  idx = fd_num - ZBC_ANSI_FIRST_FD;
  if (idx < 0 || idx >= ZBC_ANSI_MAX_FILES) {
    return NULL;
  }

  return (FILE *)fd->files[idx];
}

void zbc_ansi_fd_cleanup(zbc_ansi_fd_state_t *fd) {
  int i;

  for (i = 0; i < ZBC_ANSI_MAX_FILES; i++) {
    if (fd->files[i] != NULL) {
      fclose((FILE *)fd->files[i]);
      fd->files[i] = NULL;
    }
  }
}

/*========================================================================
 * Mode String Conversion
 *========================================================================*/

const char *zbc_ansi_mode_string(int mode) {
  switch (mode) {
  case 0:
    return "r"; /* SH_OPEN_R */
  case 1:
    return "rb"; /* SH_OPEN_RB */
  case 2:
    return "r+"; /* SH_OPEN_R_PLUS */
  case 3:
    return "r+b"; /* SH_OPEN_R_PLUS_B */
  case 4:
    return "w"; /* SH_OPEN_W */
  case 5:
    return "wb"; /* SH_OPEN_WB */
  case 6:
    return "w+"; /* SH_OPEN_W_PLUS */
  case 7:
    return "w+b"; /* SH_OPEN_W_PLUS_B */
  case 8:
    return "a"; /* SH_OPEN_A */
  case 9:
    return "ab"; /* SH_OPEN_AB */
  case 10:
    return "a+"; /* SH_OPEN_A_PLUS */
  case 11:
    return "a+b"; /* SH_OPEN_A_PLUS_B */
  default:
    return NULL;
  }
}

int zbc_ansi_mode_is_write(int mode) { return (mode >= 4); }

/*========================================================================
 * Console I/O
 *========================================================================*/

void zbc_ansi_writec(char c) {
  putchar(c);
  fflush(stdout);
}

void zbc_ansi_write0(const char *str) {
  fputs(str, stdout);
  fflush(stdout);
}

int zbc_ansi_readc(void) {
  int c;
  c = getchar();
  if (c == EOF) {
    return -1;
  }
  return c;
}

int zbc_ansi_readc_poll(void) {
#ifdef _WIN32
  /* Windows console I/O has no select() equivalent. Always report
   * "no character"; the guest can fall back to SYS_READC if it needs
   * blocking input. */
  return -1;
#else
  fd_set rfds;
  struct timeval tv;
  unsigned char c;

  FD_ZERO(&rfds);
  FD_SET(0, &rfds);
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  if (select(1, &rfds, (fd_set *)0, (fd_set *)0, &tv) <= 0) {
    return -1;
  }
  if (read(0, &c, 1) != 1) {
    return -1;
  }
  return (int)c;
#endif
}

/*========================================================================
 * Status Functions
 *========================================================================*/

int zbc_ansi_iserror(int status) { return (status < 0) ? 1 : 0; }

int zbc_ansi_istty(int fd_num) {
  if (fd_num >= 0 && fd_num <= 2) {
    return 1;
  }
  return 0;
}

/*========================================================================
 * Time Functions
 *========================================================================*/

int zbc_ansi_time(void) {
  time_t t;

  t = time(NULL);
  if (t == (time_t)-1) {
    return -1;
  }

  return (int)t;
}

int zbc_ansi_tickfreq(void) { return (int)CLOCKS_PER_SEC; }

/*========================================================================
 * Stubs
 *========================================================================*/

int zbc_ansi_get_cmdline(char *buf, size_t buf_size) {
  if (buf_size > 0) {
    buf[0] = '\0';
  }
  return 0;
}

int zbc_ansi_heapinfo(uintptr_t *heap_base, uintptr_t *heap_limit,
                      uintptr_t *stack_base, uintptr_t *stack_limit) {
  *heap_base = 0;
  *heap_limit = 0;
  *stack_base = 0;
  *stack_limit = 0;
  return 0;
}

/*========================================================================
 * Linux extensions
 *========================================================================*/

/* Little-endian writers for the fixed 48-byte stat layout. We do this
 * by hand instead of casting through the host's `struct stat` because
 * stat's layout is host-defined while the wire layout is platform-
 * agnostic (and the C library on the *guest* may not even have
 * sys/stat.h). */
static void le_pack_u32(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
  p[2] = (uint8_t)(v >> 16);
  p[3] = (uint8_t)(v >> 24);
}

static void le_pack_u64(uint8_t *p, uint64_t v) {
  le_pack_u32(p, (uint32_t)v);
  le_pack_u32(p + 4, (uint32_t)((v >> 16) >> 16));
}

int zbc_ansi_stat_path(const char *resolved_path, void *stat_buf) {
  struct stat st;
  uint8_t *out = (uint8_t *)stat_buf;

  if (stat(resolved_path, &st) != 0) {
    return -1;
  }

  /* Per docs/source/linux-extensions-proposal.rst:
   *   ino[8] mode[4] nlink[4] size[8] mtime[8] atime[8] ctime[8]
   * all little-endian. The host's struct stat field widths may differ
   * by platform; cast to fixed widths before packing. */
  le_pack_u64(out + 0, (uint64_t)st.st_ino);
  le_pack_u32(out + 8, (uint32_t)st.st_mode);
  le_pack_u32(out + 12, (uint32_t)st.st_nlink);
  le_pack_u64(out + 16, (uint64_t)st.st_size);
  le_pack_u64(out + 24, (uint64_t)st.st_mtime);
  le_pack_u64(out + 32, (uint64_t)st.st_atime);
  le_pack_u64(out + 40, (uint64_t)st.st_ctime);
  return 0;
}

int zbc_ansi_readdir_one(void *dir_ptr, void *buf, size_t buf_size) {
#ifdef _WIN32
  (void)dir_ptr;
  (void)buf;
  (void)buf_size;
  errno = ENOSYS;
  return -1;
#else
  DIR *dir = (DIR *)dir_ptr;
  struct dirent *de;
  uint8_t *out = (uint8_t *)buf;
  size_t name_len;
  size_t need;

  if (!dir) {
    return -1;
  }

  errno = 0;
  de = readdir(dir);
  if (!de) {
    return errno == 0 ? 0 : -1; /* 0 = clean EOD; -1 = real error */
  }

  name_len = strlen(de->d_name);
  if (name_len > 255) {
    return -1; /* name field is uint8 in the wire layout */
  }
  need = SH_DIRENT_HDR_SIZE + name_len + 1;
  if (buf_size < need) {
    return -1;
  }

  le_pack_u64(out + 0, (uint64_t)de->d_ino);
  out[8] = (uint8_t)de->d_type;
  out[9] = (uint8_t)name_len;
  memcpy(out + 10, de->d_name, name_len + 1);

  return (int)need;
#endif
}
