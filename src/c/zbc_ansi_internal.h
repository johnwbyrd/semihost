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
 * caller-provided 48-byte buffer (layout per the SH_SYS_STAT entry in
 * include/shared/zbc_protocol.h). Returns 0 on success, -1 on POSIX
 * stat() failure; errno is preserved for the caller's last_errno
 * accumulator.
 */
int zbc_ansi_stat_path(const char *resolved_path, void *stat_buf);

/*
 * Same as zbc_ansi_stat_path but for an already-open file descriptor.
 * The caller is responsible for mapping its handle (e.g. FILE*) to a
 * real POSIX fd via fileno() before calling.
 */
int zbc_ansi_fstat_fd(int fd, void *stat_buf);

/*
 * Create a directory at resolved_path with the given mode. On Windows
 * the mode is ignored (the host has no POSIX permission bits).
 */
int zbc_ansi_mkdir_path(const char *resolved_path, int mode);

/*
 * Remove an empty directory at resolved_path.
 */
int zbc_ansi_rmdir_path(const char *resolved_path);

/*
 * Truncate an open file to length bytes. Length is 64-bit so a small
 * guest can still request sizes beyond its native pointer width.
 */
int zbc_ansi_ftruncate_fd(int fd, uint64_t length);

/*
 * Flush a file descriptor's dirty buffers to storage.
 */
int zbc_ansi_fsync_fd(int fd);

/*
 * Create a hard link at new_path pointing to old_path. Returns 0 on
 * success, -1 on error. On Windows the helper stubs to -1/ENOSYS.
 */
int zbc_ansi_link_paths(const char *old_path, const char *new_path);

/*
 * Create a symbolic link at linkpath pointing to target. Returns 0 on
 * success, -1 on error. Stubbed on Windows; non-elevated processes
 * cannot create symlinks there without Developer Mode.
 */
int zbc_ansi_symlink_paths(const char *target, const char *linkpath);

/*
 * Read up to buf_size bytes of a symlink's target into buf (NOT
 * NUL-terminated; the caller uses the return value as the length).
 * Returns bytes written on success, -1 on error. Stubbed on Windows.
 */
int zbc_ansi_readlink_path(const char *path, void *buf, size_t buf_size);

/*
 * Like zbc_ansi_stat_path but reports the symlink's own metadata
 * rather than the target's. On Windows this falls back to stat().
 */
int zbc_ansi_lstat_path(const char *resolved_path, void *stat_buf);

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
