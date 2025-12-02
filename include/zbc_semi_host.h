/*
 * ZBC Semihosting Host Library
 *
 * Emulator/device-side library for parsing semihosting requests,
 * dispatching to backend handlers, and building responses.
 */

#ifndef ZBC_SEMI_HOST_H
#define ZBC_SEMI_HOST_H

#include "zbc_semi_common.h"
#include "zbc_semi_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

/*------------------------------------------------------------------------
 * Memory access vtable
 *
 * The host library uses these callbacks to read/write guest memory.
 * Caller provides implementations appropriate for their emulator.
 *------------------------------------------------------------------------*/

typedef struct zbc_host_mem_ops {
    /*
     * Read a single byte from guest memory.
     *
     * addr: Guest memory address (64-bit to handle any architecture)
     * context: User-provided context pointer
     *
     * Returns: Byte value at address
     */
    unsigned char (*read_u8)(uint64_t addr, void *context);

    /*
     * Write a single byte to guest memory.
     *
     * addr: Guest memory address
     * value: Byte value to write
     * context: User-provided context pointer
     */
    void (*write_u8)(uint64_t addr, unsigned char value, void *context);

    /*
     * Read a block of bytes from guest memory.
     *
     * dest: Destination buffer in host memory
     * addr: Guest memory address
     * size: Number of bytes to read
     * context: User-provided context pointer
     *
     * If NULL, library will use read_u8 in a loop.
     */
    void (*read_block)(void *dest, uint64_t addr, size_t size, void *context);

    /*
     * Write a block of bytes to guest memory.
     *
     * addr: Guest memory address
     * src: Source buffer in host memory
     * size: Number of bytes to write
     * context: User-provided context pointer
     *
     * If NULL, library will use write_u8 in a loop.
     */
    void (*write_block)(uint64_t addr, const void *src, size_t size,
                        void *context);

} zbc_host_mem_ops_t;

/*------------------------------------------------------------------------
 * Host state structure
 *
 * Caller allocates this and passes to all host functions.
 * Initialize with zbc_host_init() before first use.
 *------------------------------------------------------------------------*/

typedef struct zbc_host_state {
    /* Guest configuration (from CNFG chunk) */
    unsigned char int_size;      /* Guest sizeof(int) */
    unsigned char ptr_size;      /* Guest sizeof(void*) */
    unsigned char endianness;    /* Guest byte order */
    unsigned char cnfg_received; /* Non-zero if CNFG has been parsed */

    /* Memory access callbacks */
    zbc_host_mem_ops_t mem_ops;
    void *mem_context;

    /* Backend for syscall handling */
    const zbc_backend_t *backend;
    void *backend_context;

    /* Work buffer for RIFF parsing */
    unsigned char *work_buf;
    size_t work_buf_size;

    /* Last errno value (for SYS_ERRNO) */
    int last_errno;

} zbc_host_state_t;

/*========================================================================
 * Initialization
 *========================================================================*/

/*
 * Initialize host state.
 *
 * state: State structure to initialize
 * mem_ops: Memory access callbacks (required, read_u8 and write_u8 must be non-NULL)
 * mem_context: Context pointer passed to mem_ops callbacks
 * backend: Backend for syscall handling (required)
 * backend_context: Context pointer passed to backend functions
 * work_buf: Work buffer for RIFF parsing (required)
 * work_buf_size: Size of work buffer (recommend 4096 bytes minimum)
 *
 * Returns: ZBC_OK on success, negative error code on failure
 *
 * Must be called before any other host functions.
 * The work_buf must remain valid for the lifetime of the state.
 */
int zbc_host_init(zbc_host_state_t *state,
                  const zbc_host_mem_ops_t *mem_ops,
                  void *mem_context,
                  const zbc_backend_t *backend,
                  void *backend_context,
                  unsigned char *work_buf,
                  size_t work_buf_size);

/*
 * Reset CNFG state (requires new CNFG from guest).
 *
 * state: Host state
 *
 * Call this if the guest is reset and may send a new CNFG.
 */
void zbc_host_reset_cnfg(zbc_host_state_t *state);

/*
 * Change the backend.
 *
 * state: Host state
 * backend: New backend (required)
 * backend_context: New context pointer
 *
 * Can be called at any time to switch backends.
 */
void zbc_host_set_backend(zbc_host_state_t *state,
                          const zbc_backend_t *backend,
                          void *backend_context);

/*========================================================================
 * Request Processing
 *========================================================================*/

/*
 * Process a semihosting request.
 *
 * state: Host state
 * riff_addr: Guest memory address of RIFF buffer
 *
 * Returns: ZBC_OK or error code
 *
 * This is the main entry point. Call this when the guest writes to
 * the DOORBELL register. It:
 *
 * 1. Reads the RIFF structure from guest memory into work buffer
 * 2. Parses CNFG chunk (if present) and caches configuration
 * 3. Parses CALL chunk and extracts parameters
 * 4. Dispatches to backend function based on opcode
 * 5. Builds RETN or ERRO chunk
 * 6. Writes response back to guest memory (overwrites CALL)
 */
int zbc_host_process(zbc_host_state_t *state, uint64_t riff_addr);

/*========================================================================
 * Value Conversion Helpers
 *
 * These convert between host-native and guest-endian representations.
 * Useful for backends that need to interpret guest data.
 *========================================================================*/

/*
 * Read an integer from guest-endian buffer.
 *
 * state: Host state (for endianness and int_size info)
 * buf: Buffer containing value (int_size bytes)
 *
 * Returns: Integer value (sign-extended to int)
 */
int zbc_host_read_int(const zbc_host_state_t *state, const unsigned char *buf);

/*
 * Write an integer to guest-endian buffer.
 *
 * state: Host state (for endianness and int_size info)
 * buf: Buffer to write to (must have int_size bytes)
 * value: Value to write
 */
void zbc_host_write_int(const zbc_host_state_t *state, unsigned char *buf,
                        int value);

/*
 * Read a pointer from guest-endian buffer.
 *
 * state: Host state (for endianness and ptr_size info)
 * buf: Buffer containing value (ptr_size bytes)
 *
 * Returns: Pointer value as 64-bit unsigned
 */
uint64_t zbc_host_read_ptr(const zbc_host_state_t *state,
                           const unsigned char *buf);

/*
 * Write a pointer to guest-endian buffer.
 *
 * state: Host state (for endianness and ptr_size info)
 * buf: Buffer to write to (must have ptr_size bytes)
 * value: Value to write
 */
void zbc_host_write_ptr(const zbc_host_state_t *state, unsigned char *buf,
                        uint64_t value);

/*========================================================================
 * Guest Memory Access Helpers
 *
 * Convenience functions that use the mem_ops callbacks.
 *========================================================================*/

/*
 * Read bytes from guest memory into host buffer.
 *
 * state: Host state
 * dest: Destination buffer (host memory)
 * addr: Guest memory address
 * size: Number of bytes to read
 */
void zbc_host_read_guest(zbc_host_state_t *state, void *dest, uint64_t addr,
                         size_t size);

/*
 * Write bytes from host buffer to guest memory.
 *
 * state: Host state
 * addr: Guest memory address
 * src: Source buffer (host memory)
 * size: Number of bytes to write
 */
void zbc_host_write_guest(zbc_host_state_t *state, uint64_t addr,
                          const void *src, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* ZBC_SEMI_HOST_H */
