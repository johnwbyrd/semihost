/*
 * ZBC Semihosting Host API
 *
 * Host-side API for processing semihosting requests.
 */

#ifndef ZBC_HOST_H
#define ZBC_HOST_H

#include "zbc_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/*========================================================================
 * Memory Operations
 *========================================================================*/

/* Memory access operations for reading/writing guest memory */
typedef struct {
    uint8_t (*read_u8)(uint64_t addr, void *ctx);
    void (*write_u8)(uint64_t addr, uint8_t val, void *ctx);
    void (*read_block)(void *dest, uint64_t addr, size_t size, void *ctx);
    void (*write_block)(uint64_t addr, const void *src, size_t size, void *ctx);
} zbc_host_mem_ops_t;

/*========================================================================
 * Host Types
 *========================================================================*/

/* Forward declaration for backend */
struct zbc_backend_s;

/* Host state */
typedef struct {
    zbc_host_mem_ops_t mem_ops;
    void *mem_ctx;
    const struct zbc_backend_s *backend;
    void *backend_ctx;
    uint8_t *work_buf;
    size_t work_buf_size;
    uint8_t guest_int_size;
    uint8_t guest_ptr_size;
    uint8_t guest_endianness;
    uint8_t cnfg_received;
} zbc_host_state_t;

/*========================================================================
 * Host API
 *========================================================================*/

/* Initialize host state */
void zbc_host_init(zbc_host_state_t *state,
                   const zbc_host_mem_ops_t *mem_ops, void *mem_ctx,
                   const struct zbc_backend_s *backend, void *backend_ctx,
                   uint8_t *work_buf, size_t work_buf_size);

/* Process a semihosting request at given address */
int zbc_host_process(zbc_host_state_t *state, uint64_t riff_addr);

/* Convert guest integer to host int (handles endianness) */
int zbc_host_read_guest_int(const zbc_host_state_t *state,
                            const uint8_t *data, size_t size);

/* Write host int to guest format */
void zbc_host_write_guest_int(const zbc_host_state_t *state,
                              uint8_t *data, int value, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* ZBC_HOST_H */
