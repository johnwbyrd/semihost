/*
 * ZBC Semihosting Client Library
 *
 * Guest-side library for building semihosting requests,
 * communicating with the memory-mapped device, and parsing responses.
 */

#ifndef ZBC_SEMI_CLIENT_H
#define ZBC_SEMI_CLIENT_H

#include "zbc_semi_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*------------------------------------------------------------------------
 * Compile-time platform configuration
 *
 * These can be overridden by the build system if needed.
 *------------------------------------------------------------------------*/

/* Size of native int type in bytes */
#ifndef ZBC_CLIENT_INT_SIZE
#define ZBC_CLIENT_INT_SIZE sizeof(int)
#endif

/* Size of pointer type in bytes */
#ifndef ZBC_CLIENT_PTR_SIZE
#define ZBC_CLIENT_PTR_SIZE sizeof(void *)
#endif

/* Endianness detection */
#ifndef ZBC_CLIENT_ENDIANNESS
#if defined(__BYTE_ORDER__)
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define ZBC_CLIENT_ENDIANNESS ZBC_ENDIAN_BIG
#elif __BYTE_ORDER__ == __ORDER_PDP_ENDIAN__
#define ZBC_CLIENT_ENDIANNESS ZBC_ENDIAN_PDP
#else
#define ZBC_CLIENT_ENDIANNESS ZBC_ENDIAN_LITTLE
#endif
#elif defined(__BIG_ENDIAN__) || defined(_BIG_ENDIAN)
#define ZBC_CLIENT_ENDIANNESS ZBC_ENDIAN_BIG
#else
#define ZBC_CLIENT_ENDIANNESS ZBC_ENDIAN_LITTLE
#endif
#endif

/*------------------------------------------------------------------------
 * Client state structure
 *
 * Caller allocates this and passes to all client functions.
 * Initialize with zbc_client_init() before first use.
 *------------------------------------------------------------------------*/

typedef struct zbc_client_state {
  volatile uint8_t *dev_base; /* Device register base address */
  uint8_t cnfg_sent;          /* Non-zero if CNFG chunk already sent */
  uint8_t int_size;           /* Cached: sizeof(int) */
  uint8_t ptr_size;           /* Cached: sizeof(void*) */
  uint8_t endianness;         /* Cached: native byte order */
  /* Test hook: called when doorbell is triggered. NULL in production. */
  void (*doorbell_callback)(void *ctx);
  void *doorbell_ctx;
} zbc_client_state_t;

/*------------------------------------------------------------------------
 * Builder context
 *
 * Used internally by builder functions to construct RIFF structures.
 * Caller allocates on stack and passes to builder functions.
 *------------------------------------------------------------------------*/

typedef struct zbc_builder {
  uint8_t *buf;       /* Buffer start */
  size_t capacity;    /* Total buffer capacity */
  size_t offset;      /* Current write position */
  size_t call_offset; /* Offset where CALL chunk started (for patching) */
  int error;          /* Sticky error code (0 = OK) */
} zbc_builder_t;

/*------------------------------------------------------------------------
 * Response structure
 *
 * Populated by zbc_parse_response() after device processes request.
 *------------------------------------------------------------------------*/

typedef struct zbc_response {
  int result;           /* Syscall return value */
  int error_code;       /* errno value (0 = success) */
  const uint8_t *data;  /* Pointer to DATA chunk payload, or NULL */
  size_t data_size;     /* Size of data payload in bytes */
  int is_error;         /* Non-zero if ERRO chunk received */
  uint16_t proto_error; /* ERRO chunk error code if is_error */
} zbc_response_t;

/*------------------------------------------------------------------------
 * Heap info structure (for SYS_HEAPINFO)
 *------------------------------------------------------------------------*/

typedef struct zbc_heapinfo {
  void *heap_base;
  void *heap_limit;
  void *stack_base;
  void *stack_limit;
} zbc_heapinfo_t;

/*========================================================================
 * Initialization and Device Detection
 *========================================================================*/

/**
 * Initialize client state.
 *
 * @param state     Pointer to state structure to initialize
 * @param dev_base  Memory-mapped device register base address
 *
 * Must be called before any other client functions.
 */
void zbc_client_init(zbc_client_state_t *state, volatile void *dev_base);

/**
 * Check if device is present by reading SIGNATURE register.
 *
 * @param state  Initialized client state
 * @return       Non-zero if device present (signature matches), zero otherwise
 *
 * Reads 8-byte SIGNATURE register and verifies it contains "SEMIHOST".
 * This is the recommended way to detect the device.
 */
int zbc_client_check_signature(const zbc_client_state_t *state);

/**
 * Check if device is present (legacy method).
 *
 * @param state  Initialized client state
 * @return       Non-zero if device present, zero otherwise
 *
 * Reads STATUS register and checks DEVICE_PRESENT bit.
 * Consider using zbc_client_check_signature() for more robust detection.
 */
int zbc_client_device_present(const zbc_client_state_t *state);

/**
 * Reset CNFG sent flag (forces CNFG to be sent on next request).
 *
 * @param state  Client state
 *
 * Useful if device was reset or for testing.
 */
void zbc_client_reset_cnfg(zbc_client_state_t *state);

/*========================================================================
 * Buffer Size Calculation
 *========================================================================*/

/**
 * Calculate minimum buffer size for a syscall.
 *
 * @param state       Client state (to check if CNFG needed)
 * @param opcode      Syscall opcode (SH_SYS_*)
 * @param write_size  Size of data to write (for SYS_WRITE, etc.), or 0
 * @param read_size   Expected response data size (for SYS_READ), or 0
 * @return            Required buffer size, or negative error code
 *
 * Call this before allocating buffer to ensure sufficient space.
 */
int zbc_calc_buffer_size(const zbc_client_state_t *state, uint8_t opcode,
                         size_t write_size, size_t read_size);

/*========================================================================
 * Low-Level Builder Functions
 *
 * These allow manual construction of RIFF requests for special cases.
 * Most users should use the high-level syscall functions instead.
 *========================================================================*/

/**
 * Start building a RIFF request.
 *
 * @param builder   Builder context to initialize
 * @param buf       Buffer to write RIFF structure into
 * @param capacity  Size of buffer in bytes
 * @param state     Client state (for CNFG info)
 * @return          ZBC_OK or error code
 *
 * Writes RIFF header and CNFG chunk (if not already sent).
 */
int zbc_builder_start(zbc_builder_t *builder, uint8_t *buf, size_t capacity,
                      zbc_client_state_t *state);

/**
 * Begin a CALL chunk.
 *
 * @param builder  Builder context
 * @param opcode   Syscall opcode (SH_SYS_*)
 * @return         ZBC_OK or error code
 */
int zbc_builder_begin_call(zbc_builder_t *builder, uint8_t opcode);

/**
 * Add a PARM chunk with integer value.
 *
 * @param builder  Builder context
 * @param value    Integer value (will be written in int_size bytes)
 * @return         ZBC_OK or error code
 */
int zbc_builder_add_parm_int(zbc_builder_t *builder, int value);

/**
 * Add a PARM chunk with unsigned integer value.
 *
 * @param builder  Builder context
 * @param value    Unsigned integer value
 * @return         ZBC_OK or error code
 */
int zbc_builder_add_parm_uint(zbc_builder_t *builder, unsigned int value);

/**
 * Add a DATA chunk with binary data.
 *
 * @param builder  Builder context
 * @param data     Pointer to data (may be NULL if size is 0)
 * @param size     Size of data in bytes
 * @return         ZBC_OK or error code
 */
int zbc_builder_add_data_binary(zbc_builder_t *builder, const void *data,
                                size_t size);

/**
 * Add a DATA chunk with null-terminated string.
 *
 * @param builder  Builder context
 * @param str      Null-terminated string
 * @return         ZBC_OK or error code
 *
 * The null terminator is included in the chunk.
 */
int zbc_builder_add_data_string(zbc_builder_t *builder, const char *str);

/**
 * Finish building the RIFF request.
 *
 * @param builder   Builder context
 * @param out_size  Receives total size of RIFF structure (may be NULL)
 * @return          ZBC_OK or error code
 *
 * Patches all chunk sizes. Must be called before submitting.
 */
int zbc_builder_finish(zbc_builder_t *builder, size_t *out_size);

/*========================================================================
 * Device Communication
 *========================================================================*/

/**
 * Submit request and poll for response.
 *
 * @param state  Client state
 * @param buf    Buffer containing RIFF request
 * @param size   Size of request in bytes
 * @return       ZBC_OK or error code
 *
 * Writes buffer address to RIFF_PTR, triggers DOORBELL, polls STATUS
 * until RESPONSE_READY is set. Device overwrites CALL with RETN in buffer.
 */
int zbc_client_submit_poll(zbc_client_state_t *state, void *buf, size_t size);

/*========================================================================
 * Response Parsing
 *========================================================================*/

/**
 * Parse response from buffer after device completes.
 *
 * @param response  Response structure to populate
 * @param buf       Buffer containing device response
 * @param capacity  Size of buffer
 * @return          ZBC_OK or error code
 *
 * Extracts result value, errno, and optional DATA chunk from RETN.
 * If device returned ERRO, sets is_error flag and proto_error code.
 */
int zbc_parse_response(zbc_response_t *response, const uint8_t *buf,
                       size_t capacity, const zbc_client_state_t *state);

/*========================================================================
 * Low-Level Semihosting Entry Point (for picolibc integration)
 *========================================================================*/

/**
 * Low-level semihosting entry point for picolibc integration.
 *
 * This function matches the signature needed by picolibc's sys_semihost()
 * and handles marshalling ARM-style parameter blocks into RIFF format.
 *
 * @param state         Initialized client state
 * @param riff_buf      Buffer for RIFF request/response
 * @param riff_buf_size Size of buffer
 * @param op            ARM semihosting opcode (SH_SYS_*)
 * @param param         Pointer to ARM-style parameter block (cast to uintptr_t)
 * @return              Syscall result (interpretation depends on opcode)
 *
 * Parameter block format varies by opcode (see ARM semihosting spec):
 * - SYS_OPEN:  {path_ptr, mode, path_len}  (3 x uintptr_t)
 * - SYS_WRITE: {fd, buf_ptr, count}        (3 x uintptr_t)
 * - SYS_READ:  {fd, buf_ptr, count}        (3 x uintptr_t)
 * - etc.
 *
 * The param block uses uintptr_t-sized fields (like picolibc's sh_param_t).
 */
uintptr_t zbc_semihost(zbc_client_state_t *state, uint8_t *riff_buf,
                       size_t riff_buf_size, uintptr_t op, uintptr_t param);

/*========================================================================
 * High-Level Syscall Functions
 *
 * Each function builds the request, submits it, and parses the response.
 * Caller provides a working buffer; buffer is reused for response.
 *========================================================================*/

/**
 * Open a file.
 *
 * @param state     Client state
 * @param buf       Working buffer
 * @param buf_size  Size of buffer
 * @param pathname  File path (null-terminated)
 * @param mode      Open mode (SH_OPEN_*)
 * @return          File descriptor (>= 0) or negative error code
 */
int zbc_sys_open(zbc_client_state_t *state, void *buf, size_t buf_size,
                 const char *pathname, int mode);

/**
 * Close a file.
 *
 * @param state     Client state
 * @param buf       Working buffer
 * @param buf_size  Size of buffer
 * @param fd        File descriptor to close
 * @return          0 on success, negative error code on failure
 */
int zbc_sys_close(zbc_client_state_t *state, void *buf, size_t buf_size,
                  int fd);

/**
 * Read from a file.
 *
 * @param state     Client state
 * @param buf       Working buffer (must be large enough for response data)
 * @param buf_size  Size of buffer
 * @param fd        File descriptor
 * @param dest      Destination buffer for read data
 * @param count     Number of bytes to read
 * @return          Number of bytes read (>= 0) or negative error code
 */
int zbc_sys_read(zbc_client_state_t *state, void *buf, size_t buf_size, int fd,
                 void *dest, size_t count);

/**
 * Write to a file.
 *
 * @param state     Client state
 * @param buf       Working buffer (must be large enough for write data)
 * @param buf_size  Size of buffer
 * @param fd        File descriptor
 * @param src       Data to write
 * @param count     Number of bytes to write
 * @return          Number of bytes written (>= 0) or negative error code
 */
int zbc_sys_write(zbc_client_state_t *state, void *buf, size_t buf_size,
                  int fd, const void *src, size_t count);

/**
 * Write a single character to debug output.
 *
 * @param state     Client state
 * @param buf       Working buffer
 * @param buf_size  Size of buffer
 * @param c         Character to write
 * @return          0 on success, negative error code on failure
 */
int zbc_sys_writec(zbc_client_state_t *state, void *buf, size_t buf_size,
                   char c);

/**
 * Write null-terminated string to debug output.
 *
 * @param state     Client state
 * @param buf       Working buffer
 * @param buf_size  Size of buffer
 * @param str       String to write (null-terminated)
 * @return          0 on success, negative error code on failure
 */
int zbc_sys_write0(zbc_client_state_t *state, void *buf, size_t buf_size,
                   const char *str);

/**
 * Read a single character from input.
 *
 * @param state     Client state
 * @param buf       Working buffer
 * @param buf_size  Size of buffer
 * @return          Character read (0-255) or negative error code
 */
int zbc_sys_readc(zbc_client_state_t *state, void *buf, size_t buf_size);

/**
 * Check if a value represents an error.
 *
 * @param state     Client state
 * @param buf       Working buffer
 * @param buf_size  Size of buffer
 * @param status    Status value to check
 * @return          Non-zero if error, zero if not error, negative on lib error
 */
int zbc_sys_iserror(zbc_client_state_t *state, void *buf, size_t buf_size,
                    int status);

/**
 * Check if file descriptor is a TTY.
 *
 * @param state     Client state
 * @param buf       Working buffer
 * @param buf_size  Size of buffer
 * @param fd        File descriptor
 * @return          1 if TTY, 0 if not TTY, negative on error
 */
int zbc_sys_istty(zbc_client_state_t *state, void *buf, size_t buf_size,
                  int fd);

/**
 * Seek to position in file.
 *
 * @param state     Client state
 * @param buf       Working buffer
 * @param buf_size  Size of buffer
 * @param fd        File descriptor
 * @param pos       Absolute byte position
 * @return          0 on success, negative error code on failure
 */
int zbc_sys_seek(zbc_client_state_t *state, void *buf, size_t buf_size, int fd,
                 unsigned int pos);

/**
 * Get file length.
 *
 * @param state     Client state
 * @param buf       Working buffer
 * @param buf_size  Size of buffer
 * @param fd        File descriptor
 * @return          File length (>= 0) or negative error code
 */
int zbc_sys_flen(zbc_client_state_t *state, void *buf, size_t buf_size,
                 int fd);

/**
 * Get temporary filename.
 *
 * @param state     Client state
 * @param buf       Working buffer
 * @param buf_size  Size of buffer
 * @param pathname  Buffer to receive filename
 * @param id        Identifier for uniqueness
 * @param maxpath   Size of pathname buffer
 * @return          0 on success, negative error code on failure
 */
int zbc_sys_tmpnam(zbc_client_state_t *state, void *buf, size_t buf_size,
                   char *pathname, int id, int maxpath);

/**
 * Remove (delete) a file.
 *
 * @param state     Client state
 * @param buf       Working buffer
 * @param buf_size  Size of buffer
 * @param pathname  File path to remove
 * @return          0 on success, negative error code on failure
 */
int zbc_sys_remove(zbc_client_state_t *state, void *buf, size_t buf_size,
                   const char *pathname);

/**
 * Rename a file.
 *
 * @param state     Client state
 * @param buf       Working buffer
 * @param buf_size  Size of buffer
 * @param old_path  Current filename
 * @param new_path  New filename
 * @return          0 on success, negative error code on failure
 */
int zbc_sys_rename(zbc_client_state_t *state, void *buf, size_t buf_size,
                   const char *old_path, const char *new_path);

/**
 * Get elapsed time in centiseconds.
 *
 * @param state     Client state
 * @param buf       Working buffer
 * @param buf_size  Size of buffer
 * @return          Centiseconds since program start, or negative error
 */
unsigned int zbc_sys_clock(zbc_client_state_t *state, void *buf,
                           size_t buf_size);

/**
 * Get current time in seconds since epoch.
 *
 * @param state     Client state
 * @param buf       Working buffer
 * @param buf_size  Size of buffer
 * @return          Seconds since 1970-01-01 00:00:00 UTC, or negative error
 */
unsigned int zbc_sys_time(zbc_client_state_t *state, void *buf,
                          size_t buf_size);

/**
 * Execute a system command.
 *
 * @param state     Client state
 * @param buf       Working buffer
 * @param buf_size  Size of buffer
 * @param command   Command string to execute
 * @return          Command exit status, or negative error code
 */
int zbc_sys_system(zbc_client_state_t *state, void *buf, size_t buf_size,
                   const char *command);

/**
 * Get last errno value.
 *
 * @param state     Client state
 * @param buf       Working buffer
 * @param buf_size  Size of buffer
 * @return          Last errno value, or negative on library error
 */
int zbc_sys_errno(zbc_client_state_t *state, void *buf, size_t buf_size);

/**
 * Get command line arguments.
 *
 * @param state     Client state
 * @param buf       Working buffer
 * @param buf_size  Size of buffer
 * @param dest      Buffer to receive command line
 * @param max_size  Size of dest buffer
 * @return          Length of command line, or negative error code
 */
int zbc_sys_get_cmdline(zbc_client_state_t *state, void *buf, size_t buf_size,
                        char *dest, int max_size);

/**
 * Get heap and stack information.
 *
 * @param state     Client state
 * @param buf       Working buffer
 * @param buf_size  Size of buffer
 * @param info      Structure to receive heap/stack info
 * @return          0 on success, negative error code on failure
 */
int zbc_sys_heapinfo(zbc_client_state_t *state, void *buf, size_t buf_size,
                     zbc_heapinfo_t *info);

/**
 * Exit the program.
 *
 * @param state     Client state
 * @param buf       Working buffer
 * @param buf_size  Size of buffer
 * @param exception Exception code
 * @param subcode   Subcode
 *
 * Does not return.
 */
void zbc_sys_exit(zbc_client_state_t *state, void *buf, size_t buf_size,
                  unsigned int exception, unsigned int subcode);

/**
 * Exit with extended status.
 *
 * @param state     Client state
 * @param buf       Working buffer
 * @param buf_size  Size of buffer
 * @param code      Exit code
 *
 * Does not return.
 */
void zbc_sys_exit_extended(zbc_client_state_t *state, void *buf,
                           size_t buf_size, unsigned int code);

/**
 * Get 64-bit elapsed tick count.
 *
 * @param state     Client state
 * @param buf       Working buffer
 * @param buf_size  Size of buffer
 * @param low       Receives low 32 bits of tick count
 * @param high      Receives high 32 bits of tick count
 * @return          0 on success, negative error code on failure
 *
 * Returns 64-bit value split into two 32-bit parts for portability.
 */
int zbc_sys_elapsed(zbc_client_state_t *state, void *buf, size_t buf_size,
                    uint32_t *low, uint32_t *high);

/**
 * Get tick frequency.
 *
 * @param state     Client state
 * @param buf       Working buffer
 * @param buf_size  Size of buffer
 * @return          Ticks per second, or negative error code
 */
unsigned int zbc_sys_tickfreq(zbc_client_state_t *state, void *buf,
                              size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* ZBC_SEMI_CLIENT_H */
