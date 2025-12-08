/*
 * ZBC Semihosting Client API
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

/* Detect client platform configuration */
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

/* Client state */
typedef struct {
    volatile uint8_t *dev_base;  /* Pointer to device registers */
    uint8_t cnfg_sent;           /* 1 if CNFG chunk has been sent */
    uint8_t int_size;            /* sizeof(int) on this platform */
    uint8_t ptr_size;            /* sizeof(void*) on this platform */
    uint8_t endianness;          /* ZBC_ENDIAN_* */
    void (*doorbell_callback)(void *);  /* For testing */
    void *doorbell_ctx;
} zbc_client_state_t;

/* Response from parsed RETN/ERRO chunk */
typedef struct {
    int result;           /* Syscall return value */
    int error_code;       /* Errno value from host */
    const uint8_t *data;  /* Pointer to DATA payload (if any) */
    size_t data_size;     /* Size of DATA payload */
    int is_error;         /* 1 if ERRO chunk received */
    int proto_error;      /* Protocol error code from ERRO */
} zbc_response_t;

/*========================================================================
 * Client API
 *========================================================================*/

/* Initialize client state */
void zbc_client_init(zbc_client_state_t *state, volatile void *dev_base);

/* Check device signature and presence */
int zbc_client_check_signature(const zbc_client_state_t *state);
int zbc_client_device_present(const zbc_client_state_t *state);

/* Reset CNFG sent flag (forces resend on next call) */
void zbc_client_reset_cnfg(zbc_client_state_t *state);

/*
 * Main syscall entry point.
 * Builds RIFF request from opcode table, submits, parses response.
 *
 * response - receives parsed response (result, errno, data pointer)
 * state    - initialized client state
 * buf      - RIFF buffer (caller-provided)
 * buf_size - size of buffer
 * opcode   - SH_SYS_* opcode
 * args     - array of arguments (interpretation depends on opcode)
 *
 * Returns ZBC_OK on success, ZBC_ERR_* on protocol/transport error.
 * On success, check response->result for the syscall return value.
 */
int zbc_call(zbc_response_t *response, zbc_client_state_t *state,
             void *buf, size_t buf_size, int opcode, uintptr_t *args);

/*
 * ARM-compatible semihost entry point.
 * Same as zbc_call but takes (op, param) where param points to args array.
 */
uintptr_t zbc_semihost(zbc_client_state_t *state, uint8_t *riff_buf,
                       size_t riff_buf_size, uintptr_t op, uintptr_t param);

/* Submit request and poll for response */
int zbc_client_submit_poll(zbc_client_state_t *state, void *buf, size_t size);

/* Parse response from RIFF buffer */
int zbc_parse_response(zbc_response_t *response, const uint8_t *buf,
                       size_t capacity, const zbc_client_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* ZBC_CLIENT_H */
