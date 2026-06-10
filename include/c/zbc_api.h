/**
 * @file zbc_api.h
 * @brief ZBC Semihosting High-Level API
 *
 * Type-safe wrapper functions for all semihosting operations.
 * This provides a more comfortable API than the raw zbc_call() interface.
 */

#ifndef ZBC_API_H
#define ZBC_API_H

#include "zbc_client.h"

#ifdef __cplusplus
extern "C" {
#endif

/*========================================================================
 * API Types
 *========================================================================*/

/**
 * High-level API state.
 *
 * Bundles the client state and RIFF buffer for convenient function calls.
 * Initialize with zbc_api_init() before use.
 */
typedef struct {
    zbc_client_state_t *client;  /**< Pointer to initialized client state */
    void *buf;                   /**< RIFF buffer for requests/responses */
    size_t buf_size;             /**< Size of RIFF buffer */
    int last_errno;              /**< Errno from last operation */
} zbc_api_t;

/*========================================================================
 * Initialization
 *========================================================================*/

/**
 * Initialize API state.
 *
 * @param api       API state to initialize
 * @param client    Pointer to initialized client state
 * @param buf       RIFF buffer (caller-provided)
 * @param buf_size  Size of RIFF buffer in bytes
 */
void zbc_api_init(zbc_api_t *api, zbc_client_state_t *client,
                  void *buf, size_t buf_size);

/**
 * Get errno from last operation.
 *
 * @param api  API state
 * @return Host errno value from last operation, or 0 if successful
 */
int zbc_api_errno(zbc_api_t *api);

/*========================================================================
 * File Operations
 *========================================================================*/

/**
 * Open a file.
 *
 * @param api   API state
 * @param path  File path (null-terminated)
 * @param mode  Open mode (SH_OPEN_R, SH_OPEN_W, etc.)
 * @return File descriptor on success, -1 on error
 */
int zbc_api_open(zbc_api_t *api, const char *path, int mode);

/**
 * Close a file.
 *
 * @param api  API state
 * @param fd   File descriptor
 * @return 0 on success, -1 on error
 */
int zbc_api_close(zbc_api_t *api, int fd);

/**
 * Read from a file.
 *
 * @param api    API state
 * @param fd     File descriptor
 * @param dest   Destination buffer
 * @param count  Maximum bytes to read
 * @return Bytes NOT read (0 = full read), -1 on error
 */
int zbc_api_read(zbc_api_t *api, int fd, void *dest, size_t count);

/**
 * Write to a file.
 *
 * @param api    API state
 * @param fd     File descriptor
 * @param data   Data to write
 * @param count  Number of bytes to write
 * @return Bytes NOT written (0 = full write), -1 on error
 */
int zbc_api_write(zbc_api_t *api, int fd, const void *data, size_t count);

/**
 * Seek to position in file.
 *
 * @param api  API state
 * @param fd   File descriptor
 * @param pos  Absolute position in bytes
 * @return 0 on success, -1 on error
 */
int zbc_api_seek(zbc_api_t *api, int fd, int pos);

/**
 * Get file length.
 *
 * @param api  API state
 * @param fd   File descriptor
 * @return File length in bytes, -1 on error
 */
intmax_t zbc_api_flen(zbc_api_t *api, int fd);

/**
 * Check if file descriptor is a TTY.
 *
 * @param api  API state
 * @param fd   File descriptor
 * @return 1 if TTY, 0 if not, -1 on error
 */
int zbc_api_istty(zbc_api_t *api, int fd);

/**
 * Remove (delete) a file.
 *
 * @param api   API state
 * @param path  File path (null-terminated)
 * @return 0 on success, -1 on error
 */
int zbc_api_remove(zbc_api_t *api, const char *path);

/**
 * Rename a file.
 *
 * @param api       API state
 * @param old_path  Current file path (null-terminated)
 * @param new_path  New file path (null-terminated)
 * @return 0 on success, -1 on error
 */
int zbc_api_rename(zbc_api_t *api, const char *old_path, const char *new_path);

/**
 * Generate a temporary filename.
 *
 * @param api     API state
 * @param dest    Destination buffer for filename
 * @param maxlen  Maximum length of filename
 * @param id      Identifier (0-255)
 * @return 0 on success, -1 on error
 */
int zbc_api_tmpnam(zbc_api_t *api, char *dest, size_t maxlen, int id);

/*========================================================================
 * Console Operations
 *========================================================================*/

/**
 * Write a single character to console.
 *
 * @param api  API state
 * @param c    Character to write
 */
void zbc_api_writec(zbc_api_t *api, char c);

/**
 * Write a null-terminated string to console.
 *
 * @param api  API state
 * @param str  String to write (null-terminated)
 */
void zbc_api_write0(zbc_api_t *api, const char *str);

/**
 * Read a character from console (blocking).
 *
 * @param api  API state
 * @return Character read, or -1 on error
 */
int zbc_api_readc(zbc_api_t *api);

/*========================================================================
 * Time Operations
 *========================================================================*/

/**
 * Get centiseconds since execution started.
 *
 * @param api  API state
 * @return Centiseconds, or -1 on error
 */
int zbc_api_clock(zbc_api_t *api);

/**
 * Get seconds since Unix epoch.
 *
 * @param api  API state
 * @return Seconds since epoch, or -1 on error
 */
int zbc_api_time(zbc_api_t *api);

/**
 * Get tick frequency (ticks per second).
 *
 * @param api  API state
 * @return Ticks per second, or -1 on error
 */
int zbc_api_tickfreq(zbc_api_t *api);

/**
 * Get elapsed tick count (64-bit).
 *
 * @param api        API state
 * @param ticks_out  Receives 64-bit tick count
 * @return 0 on success, -1 on error
 */
int zbc_api_elapsed(zbc_api_t *api, uint64_t *ticks_out);

/**
 * Configure periodic timer.
 *
 * @param api      API state
 * @param rate_hz  Timer frequency in Hz (0 to disable)
 * @return 0 on success, -1 on error
 */
int zbc_api_timer_config(zbc_api_t *api, unsigned int rate_hz);

/*========================================================================
 * System Operations
 *========================================================================*/

/**
 * Check if a value represents an error.
 *
 * This is pure logic (no semihosting call).
 *
 * @param status  Value to check
 * @return 1 if error (negative), 0 otherwise
 */
int zbc_api_iserror(int status);

/**
 * Get errno value from host.
 *
 * This fetches the current errno from the host, which may differ
 * from zbc_api_errno() if other operations occurred.
 *
 * @param api  API state
 * @return Host errno value
 */
int zbc_api_get_errno(zbc_api_t *api);

/**
 * Execute a shell command on the host.
 *
 * @param api  API state
 * @param cmd  Command string (null-terminated)
 * @return Command exit code, or -1 on error
 */
int zbc_api_system(zbc_api_t *api, const char *cmd);

/**
 * Get command line arguments.
 *
 * @param api     API state
 * @param dest    Destination buffer
 * @param maxlen  Maximum length
 * @return 0 on success, -1 on error
 */
int zbc_api_get_cmdline(zbc_api_t *api, char *dest, size_t maxlen);

/**
 * Get heap and stack information.
 *
 * @param api          API state
 * @param heap_base    Receives heap base address
 * @param heap_limit   Receives heap limit address
 * @param stack_base   Receives stack base address
 * @param stack_limit  Receives stack limit address
 * @return 0 on success, -1 on error
 */
int zbc_api_heapinfo(zbc_api_t *api, uintptr_t *heap_base,
                     uintptr_t *heap_limit, uintptr_t *stack_base,
                     uintptr_t *stack_limit);

/**
 * Exit the application.
 *
 * @param api     API state
 * @param status  Exit status code
 */
void zbc_api_exit(zbc_api_t *api, int status);

/**
 * Exit the application with extended information.
 *
 * @param api      API state
 * @param reason   Exit reason (ADP_Stopped_* constant)
 * @param subcode  Additional exit code
 */
void zbc_api_exit_extended(zbc_api_t *api, unsigned int reason,
                           unsigned int subcode);

#ifdef __cplusplus
}
#endif

#endif /* ZBC_API_H */
