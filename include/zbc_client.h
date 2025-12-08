/**
 * @file zbc_client.h
 * @brief ZBC Semihosting Client API
 *
 * Client-side API for embedded/guest code to make semihosting calls.
 */

#ifndef ZBC_CLIENT_H
#define ZBC_CLIENT_H

#include "zbc_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/*========================================================================
 * Client Configuration
 *========================================================================*/

/** Detect client platform configuration */
#ifndef ZBC_CLIENT_INT_SIZE
#define ZBC_CLIENT_INT_SIZE ((int)sizeof(int))
#endif

#ifndef ZBC_CLIENT_PTR_SIZE
#define ZBC_CLIENT_PTR_SIZE ((int)sizeof(void *))
#endif

#ifndef ZBC_CLIENT_ENDIANNESS
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define ZBC_CLIENT_ENDIANNESS ZBC_ENDIAN_BIG
#else
#define ZBC_CLIENT_ENDIANNESS ZBC_ENDIAN_LITTLE
#endif
#endif

/*========================================================================
 * Client Types
 *========================================================================*/

/**
 * Client state structure.
 *
 * Initialize with zbc_client_init() before use.
 */
typedef struct {
    volatile uint8_t *dev_base;  /**< Pointer to device registers */
    uint8_t cnfg_sent;           /**< 1 if CNFG chunk has been sent */
    uint8_t int_size;            /**< sizeof(int) on this platform */
    uint8_t ptr_size;            /**< sizeof(void*) on this platform */
    uint8_t endianness;          /**< ZBC_ENDIAN_LITTLE or ZBC_ENDIAN_BIG */
    void (*doorbell_callback)(void *);  /**< For testing */
    void *doorbell_ctx;          /**< Context for doorbell callback */
} zbc_client_state_t;

/**
 * Response from a semihosting call.
 *
 * Populated by zbc_call() and zbc_parse_response().
 */
typedef struct {
    int result;           /**< Syscall return value */
    int error_code;       /**< Errno value from host */
    const uint8_t *data;  /**< Pointer to DATA payload (if any) */
    size_t data_size;     /**< Size of DATA payload */
    int is_error;         /**< 1 if ERRO chunk received */
    int proto_error;      /**< Protocol error code from ERRO */
} zbc_response_t;

/*========================================================================
 * Client API
 *========================================================================*/

/**
 * Initialize client state with device base address.
 *
 * The function detects the platform's integer size, pointer size, and
 * endianness automatically using compile-time configuration macros.
 *
 * @param state    Client state structure to initialize
 * @param dev_base Memory-mapped device base address
 */
void zbc_client_init(zbc_client_state_t *state, volatile void *dev_base);

/**
 * Check if a semihosting device is present by reading the signature.
 *
 * Call this before making semihosting calls to verify the device exists.
 *
 * @param state Initialized client state
 * @return 1 if device signature matches "SEMIHOST", 0 otherwise
 */
int zbc_client_check_signature(const zbc_client_state_t *state);

/**
 * Check device present bit in status register.
 *
 * @param state Initialized client state
 * @return 1 if device present bit is set, 0 otherwise
 */
int zbc_client_device_present(const zbc_client_state_t *state);

/**
 * Reset the CNFG sent flag, forcing resend on next call.
 *
 * Normally the CNFG chunk is sent only once. Use this if you need
 * to resend configuration (e.g., after device reset).
 *
 * @param state Initialized client state
 */
void zbc_client_reset_cnfg(zbc_client_state_t *state);

/**
 * Execute a semihosting syscall.
 *
 * This is the main entry point for making semihosting calls. The function:
 * 1. Builds a RIFF request from the opcode table
 * 2. Submits the request to the device
 * 3. Waits for the response (polling)
 * 4. Parses the response into the response structure
 *
 * On success, check response->result for the syscall return value
 * and response->error_code for the host errno.
 *
 * @param[out] response  Receives parsed response (result, errno, data pointer)
 * @param state          Initialized client state
 * @param buf            RIFF buffer (caller-provided)
 * @param buf_size       Size of buffer in bytes
 * @param opcode         SH_SYS_* opcode from zbc_protocol.h
 * @param args           Array of arguments (layout depends on opcode), may be NULL
 * @return ZBC_OK on success, ZBC_ERR_* on protocol/transport error
 */
int zbc_call(zbc_response_t *response, zbc_client_state_t *state,
             void *buf, size_t buf_size, int opcode, uintptr_t *args);

/**
 * ARM-compatible semihost entry point.
 *
 * This is a thin wrapper around zbc_call() that accepts the ARM-style
 * parameter block format (op, pointer-to-args) used by picolibc and newlib.
 *
 * Use this to implement sys_semihost() for libc integration.
 *
 * @param state         Initialized client state
 * @param riff_buf      RIFF buffer
 * @param riff_buf_size Size of buffer
 * @param op            SH_SYS_* opcode
 * @param param         Pointer to args array (cast from uintptr_t*)
 * @return Syscall result, or (uintptr_t)-1 on error
 */
uintptr_t zbc_semihost(zbc_client_state_t *state, uint8_t *riff_buf,
                       size_t riff_buf_size, uintptr_t op, uintptr_t param);

/**
 * Submit a RIFF request and poll for response.
 *
 * This writes the buffer address to RIFF_PTR, triggers DOORBELL, and
 * polls STATUS until RESPONSE_READY is set. Most users should use
 * zbc_call() instead.
 *
 * @param state Initialized client state
 * @param buf   RIFF buffer containing the request
 * @param size  Size of request data
 * @return ZBC_OK on success, error code on failure
 */
int zbc_client_submit_poll(zbc_client_state_t *state, void *buf, size_t size);

/**
 * Parse response from RIFF buffer.
 *
 * Extracts the result, errno, and data from a RETN or ERRO chunk.
 * Most users should use zbc_call() instead.
 *
 * @param[out] response Receives parsed response
 * @param buf           RIFF buffer containing the response
 * @param capacity      Buffer capacity
 * @param state         Client state (for int_size/endianness)
 * @return ZBC_OK on success, ZBC_ERR_PARSE_ERROR on failure
 */
int zbc_parse_response(zbc_response_t *response, const uint8_t *buf,
                       size_t capacity, const zbc_client_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* ZBC_CLIENT_H */
