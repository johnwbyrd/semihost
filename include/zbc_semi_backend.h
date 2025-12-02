/*
 * ZBC Semihosting Backend Interface
 *
 * This header defines the backend vtable that implements the actual
 * semihosting operations. The host library dispatches parsed RIFF
 * requests to backend functions.
 *
 * Two backends are provided:
 *   - Dummy backend: All operations succeed with no side effects
 *   - ANSI backend: Uses standard C library (fopen, fread, etc.)
 *
 * Users may implement custom backends by creating a zbc_backend_t
 * structure with their own function pointers.
 */

#ifndef ZBC_SEMI_BACKEND_H
#define ZBC_SEMI_BACKEND_H

#include <stddef.h>
#include "zbc_semi_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*------------------------------------------------------------------------
 * Backend vtable
 *
 * Each function corresponds to an ARM semihosting syscall.
 * Return values follow ARM semihosting conventions:
 *   - File operations return fd on success, -1 on error
 *   - Read/write return bytes NOT transferred (0 = complete success)
 *   - Status functions return 0 for false/success, non-zero for true/error
 *
 * The ctx parameter is the backend_context passed to zbc_host_init().
 * Backend implementations may use it to store state (open files, etc.).
 *
 * NULL function pointers are allowed; the host library will return
 * appropriate error codes for unimplemented operations.
 *------------------------------------------------------------------------*/

typedef struct zbc_backend {
    /*
     * SYS_OPEN (0x01): Open a file
     *
     * path: File path (not necessarily null-terminated)
     * path_len: Length of path in bytes
     * mode: ARM semihosting open mode (0-11, see specification)
     *
     * Returns: File descriptor (>= 0) on success, -1 on error
     */
    int (*open)(void *ctx, const char *path, size_t path_len, int mode);

    /*
     * SYS_CLOSE (0x02): Close a file
     *
     * fd: File descriptor to close
     *
     * Returns: 0 on success, -1 on error
     */
    int (*close)(void *ctx, int fd);

    /*
     * SYS_READ (0x06): Read from a file
     *
     * fd: File descriptor
     * buf: Buffer to read into
     * count: Number of bytes to read
     *
     * Returns: Number of bytes NOT read (0 = all bytes read successfully)
     *          Returns count on EOF, -1 on error
     */
    int (*read)(void *ctx, int fd, void *buf, size_t count);

    /*
     * SYS_WRITE (0x05): Write to a file
     *
     * fd: File descriptor
     * buf: Buffer to write from
     * count: Number of bytes to write
     *
     * Returns: Number of bytes NOT written (0 = all bytes written)
     *          -1 on error
     */
    int (*write)(void *ctx, int fd, const void *buf, size_t count);

    /*
     * SYS_SEEK (0x0A): Seek to position in file
     *
     * fd: File descriptor
     * pos: Absolute byte position from start of file
     *
     * Returns: 0 on success, -1 on error
     */
    int (*seek)(void *ctx, int fd, int pos);

    /*
     * SYS_FLEN (0x0C): Get file length
     *
     * fd: File descriptor
     *
     * Returns: File length in bytes (>= 0), -1 on error
     */
    int (*flen)(void *ctx, int fd);

    /*
     * SYS_REMOVE (0x0E): Delete a file
     *
     * path: File path (not necessarily null-terminated)
     * path_len: Length of path in bytes
     *
     * Returns: 0 on success, -1 on error
     */
    int (*remove)(void *ctx, const char *path, size_t path_len);

    /*
     * SYS_RENAME (0x0F): Rename a file
     *
     * old_path: Current file path
     * old_len: Length of old_path
     * new_path: New file path
     * new_len: Length of new_path
     *
     * Returns: 0 on success, -1 on error
     */
    int (*rename)(void *ctx,
                  const char *old_path, size_t old_len,
                  const char *new_path, size_t new_len);

    /*
     * SYS_TMPNAM (0x0D): Generate temporary filename
     *
     * buf: Buffer to receive filename
     * buf_size: Size of buffer
     * id: Identifier for uniqueness (0-255)
     *
     * Returns: 0 on success, -1 on error
     */
    int (*tmpnam)(void *ctx, char *buf, size_t buf_size, int id);

    /*
     * SYS_WRITEC (0x03): Write single character to console
     *
     * c: Character to write
     */
    void (*writec)(void *ctx, char c);

    /*
     * SYS_WRITE0 (0x04): Write null-terminated string to console
     *
     * str: Null-terminated string
     */
    void (*write0)(void *ctx, const char *str);

    /*
     * SYS_READC (0x07): Read single character from console
     *
     * Returns: Character read (0-255), -1 on error/EOF
     */
    int (*readc)(void *ctx);

    /*
     * SYS_ISERROR (0x08): Check if value indicates error
     *
     * status: Value to check
     *
     * Returns: Non-zero if status indicates error, 0 otherwise
     */
    int (*iserror)(void *ctx, int status);

    /*
     * SYS_ISTTY (0x09): Check if fd is a terminal
     *
     * fd: File descriptor
     *
     * Returns: 1 if fd is a TTY, 0 if not, -1 on error
     */
    int (*istty)(void *ctx, int fd);

    /*
     * SYS_CLOCK (0x10): Get elapsed time
     *
     * Returns: Centiseconds since execution started, -1 if not available
     */
    int (*clock)(void *ctx);

    /*
     * SYS_TIME (0x11): Get current time
     *
     * Returns: Seconds since 1970-01-01 00:00:00 UTC, -1 on error
     */
    int (*time)(void *ctx);

    /*
     * SYS_ELAPSED (0x30): Get elapsed tick count (64-bit)
     *
     * lo: Receives low 32 bits of tick count
     * hi: Receives high 32 bits of tick count
     *
     * Returns: 0 on success, -1 if not available
     */
    int (*elapsed)(void *ctx, unsigned int *lo, unsigned int *hi);

    /*
     * SYS_TICKFREQ (0x31): Get tick frequency
     *
     * Returns: Ticks per second, -1 if not available
     */
    int (*tickfreq)(void *ctx);

    /*
     * SYS_SYSTEM (0x12): Execute system command
     *
     * cmd: Command string (not necessarily null-terminated)
     * cmd_len: Length of command
     *
     * Returns: Command exit status, -1 on error
     */
    int (*do_system)(void *ctx, const char *cmd, size_t cmd_len);

    /*
     * SYS_GET_CMDLINE (0x15): Get command line arguments
     *
     * buf: Buffer to receive command line
     * buf_size: Size of buffer
     *
     * Returns: Length of command line on success, -1 on error
     */
    int (*get_cmdline)(void *ctx, char *buf, size_t buf_size);

    /*
     * SYS_HEAPINFO (0x16): Get heap and stack information
     *
     * heap_base: Receives heap base address
     * heap_limit: Receives heap limit address
     * stack_base: Receives stack base address
     * stack_limit: Receives stack limit address
     *
     * Returns: 0 on success, -1 on error
     */
    int (*heapinfo)(void *ctx,
                    unsigned int *heap_base, unsigned int *heap_limit,
                    unsigned int *stack_base, unsigned int *stack_limit);

    /*
     * SYS_EXIT (0x18) / SYS_EXIT_EXTENDED (0x20): Exit program
     *
     * reason: Exit reason code
     * subcode: Additional exit information
     *
     * This function may not return (e.g., if running in emulator).
     */
    void (*do_exit)(void *ctx, unsigned int reason, unsigned int subcode);

    /*
     * SYS_ERRNO (0x13): Get last error code
     *
     * Returns: Last errno value from failed operation
     */
    int (*get_errno)(void *ctx);

} zbc_backend_t;

/*------------------------------------------------------------------------
 * Pre-built backends
 *------------------------------------------------------------------------*/

/*
 * Get the dummy backend.
 *
 * The dummy backend returns success for all operations with no side
 * effects. Useful for testing or when semihosting output is not needed.
 *
 * Behavior:
 *   - open: Returns fd 3 (avoids stdin/stdout/stderr)
 *   - close: Returns 0 (success)
 *   - read: Returns count (no bytes read, simulates EOF)
 *   - write: Returns 0 (all bytes "written")
 *   - seek: Returns 0 (success)
 *   - flen: Returns 0
 *   - remove/rename: Returns 0 (success)
 *   - tmpnam: Writes "tmp<id>" to buffer
 *   - writec/write0: No output
 *   - readc: Returns -1 (EOF)
 *   - iserror: Returns 0 (not an error)
 *   - istty: Returns 0 (not a TTY)
 *   - clock: Returns 0
 *   - time: Returns 0
 *   - elapsed: Sets lo=hi=0, returns 0
 *   - tickfreq: Returns 100
 *   - do_system: Returns 0
 *   - get_cmdline: Returns empty string
 *   - heapinfo: Sets all to 0
 *   - do_exit: No action
 *   - get_errno: Returns 0
 *
 * Returns: Pointer to static backend structure (never NULL)
 */
const zbc_backend_t *zbc_backend_dummy(void);

/*
 * Get the ANSI C backend.
 *
 * The ANSI backend implements semihosting using only standard C library
 * functions, making it portable across any hosted environment.
 *
 * File descriptor mapping:
 *   - fd 0: stdin
 *   - fd 1: stdout
 *   - fd 2: stderr
 *   - fd 3+: Dynamically allocated FILE* handles
 *
 * The backend may allocate memory to track open files. Call
 * zbc_backend_ansi_cleanup() before program exit to close all
 * open files and free resources.
 *
 * Console I/O uses stdin/stdout via getchar()/putchar().
 *
 * Returns: Pointer to static backend structure (never NULL)
 */
const zbc_backend_t *zbc_backend_ansi(void);

/*
 * Clean up ANSI backend resources.
 *
 * Closes all open files and frees any allocated memory.
 * Call this before program exit if using the ANSI backend.
 */
void zbc_backend_ansi_cleanup(void);

/*------------------------------------------------------------------------
 * Implementing custom backends
 *
 * To create a custom backend:
 *
 * 1. Declare a zbc_backend_t structure
 * 2. Assign function pointers for operations you want to implement
 * 3. Set unused operations to NULL (host will return error)
 * 4. Pass your backend to zbc_host_init()
 *
 * Example:
 *
 *   static int my_open(void *ctx, const char *path, size_t len, int mode)
 *   {
 *       struct my_state *state = (struct my_state *)ctx;
 *       ... implementation ...
 *   }
 *
 *   static const zbc_backend_t my_backend = {
 *       my_open,
 *       my_close,
 *       my_read,
 *       my_write,
 *       NULL,  // seek not implemented
 *       NULL,  // flen not implemented
 *       ...
 *   };
 *
 *   // In initialization:
 *   struct my_state state;
 *   zbc_host_init(&host, &mem_ops, mem_ctx, &my_backend, &state,
 *                 work_buf, sizeof(work_buf));
 *
 *------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* ZBC_SEMI_BACKEND_H */
