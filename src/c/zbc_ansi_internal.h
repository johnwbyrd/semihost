/*
 * ZBC Semihosting ANSI Backend - Internal Header
 *
 * Shared types and functions used by both secure and insecure backends.
 * This is an internal header - not part of the public API.
 */

#ifndef ZBC_ANSI_INTERNAL_H
#define ZBC_ANSI_INTERNAL_H

#include "zbc_semihost.h"
#include <stdio.h>

/*========================================================================
 * Common FD State
 *
 * Both secure and insecure backends embed this structure to manage
 * file descriptors. This eliminates code duplication.
 *========================================================================*/

typedef struct zbc_ansi_fd_state_s {
  void *files[ZBC_ANSI_MAX_FILES];
  zbc_ansi_fd_node_t fd_pool[ZBC_ANSI_MAX_FILES];
  zbc_ansi_fd_node_t *free_fd_list;
  int next_fd;
} zbc_ansi_fd_state_t;

/*========================================================================
 * FD Management Functions
 *========================================================================*/

/*
 * Initialize FD state.
 */
void zbc_ansi_fd_init(zbc_ansi_fd_state_t *fd);

/*
 * Allocate a file descriptor for a FILE pointer.
 * Returns the FD (>= ZBC_ANSI_FIRST_FD) on success, -1 on failure.
 */
int zbc_ansi_fd_alloc(zbc_ansi_fd_state_t *fd, FILE *fp);

/*
 * Free a file descriptor, returning it to the free list.
 */
void zbc_ansi_fd_free(zbc_ansi_fd_state_t *fd, int fd_num);

/*
 * Get the FILE pointer for a file descriptor.
 * Returns NULL if invalid. Handles stdin/stdout/stderr (0/1/2).
 */
FILE *zbc_ansi_fd_get(zbc_ansi_fd_state_t *fd, int fd_num);

/*
 * Close all open files and reset state.
 */
void zbc_ansi_fd_cleanup(zbc_ansi_fd_state_t *fd);

/*========================================================================
 * Mode String Conversion
 *========================================================================*/

/*
 * Convert ARM semihosting open mode to fopen mode string.
 * Returns NULL for invalid mode.
 */
const char *zbc_ansi_mode_string(int mode);

/*
 * Check if open mode is a write mode (modes 4+ involve writing).
 */
int zbc_ansi_mode_is_write(int mode);

/*========================================================================
 * Console I/O
 *========================================================================*/

void zbc_ansi_writec(char c);
void zbc_ansi_write0(const char *str);
int zbc_ansi_readc(void);
int zbc_ansi_readc_poll(void);

/*========================================================================
 * Status Functions
 *========================================================================*/

int zbc_ansi_iserror(int status);
int zbc_ansi_istty(int fd_num);

/*========================================================================
 * Time Functions
 *========================================================================*/

int zbc_ansi_time(void);
int zbc_ansi_tickfreq(void);

/*========================================================================
 * Stubs
 *========================================================================*/

int zbc_ansi_get_cmdline(char *buf, size_t buf_size);
int zbc_ansi_heapinfo(uintptr_t *heap_base, uintptr_t *heap_limit,
                      uintptr_t *stack_base, uintptr_t *stack_limit);

/*========================================================================
 * Linux extensions
 *========================================================================*/

/*
 * Stat the null-terminated path and pack its metadata into the
 * caller-provided 48-byte buffer (layout per docs/source/linux-
 * extensions-proposal.rst). Returns 0 on success, -1 on POSIX
 * stat() failure; errno is preserved for the caller's last_errno
 * accumulator.
 */
int zbc_ansi_stat_path(const char *resolved_path, void *stat_buf);

/*
 * Pack one POSIX readdir() result into the SYS_READDIR wire layout
 * (d_ino[8] d_type[1] d_namlen[1] d_name[d_namlen+1]). Returns the
 * number of bytes written on success, -1 if the entry won't fit in
 * buf_size. The caller passes the DIR* it owns; this helper does the
 * readdir() call so the layout details live in one place.
 *
 * Returns 0 at end of directory (no entry written).
 */
int zbc_ansi_readdir_one(void *dir_ptr, void *buf, size_t buf_size);

#endif /* ZBC_ANSI_INTERNAL_H */
