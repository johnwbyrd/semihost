/*
 * ZBC Semihosting Host Library
 *
 * Emulator/device-side library for parsing semihosting requests,
 * dispatching to syscall handlers, and building responses.
 */

#ifndef ZBC_SEMI_HOST_H
#define ZBC_SEMI_HOST_H

#include "zbc_semi_common.h"

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
  /**
   * Read a single byte from guest memory.
   *
   * @param addr     Guest memory address (64-bit to handle any arch)
   * @param context  User-provided context pointer
   * @return         Byte value at address
   */
  uint8_t (*read_u8)(uint64_t addr, void *context);

  /**
   * Write a single byte to guest memory.
   *
   * @param addr     Guest memory address
   * @param value    Byte value to write
   * @param context  User-provided context pointer
   */
  void (*write_u8)(uint64_t addr, uint8_t value, void *context);

  /**
   * Read a block of bytes from guest memory.
   *
   * @param dest     Destination buffer in host memory
   * @param addr     Guest memory address
   * @param size     Number of bytes to read
   * @param context  User-provided context pointer
   *
   * If NULL, library will use read_u8 in a loop.
   */
  void (*read_block)(void *dest, uint64_t addr, size_t size, void *context);

  /**
   * Write a block of bytes to guest memory.
   *
   * @param addr     Guest memory address
   * @param src      Source buffer in host memory
   * @param size     Number of bytes to write
   * @param context  User-provided context pointer
   *
   * If NULL, library will use write_u8 in a loop.
   */
  void (*write_block)(uint64_t addr, const void *src, size_t size,
                      void *context);

} zbc_host_mem_ops_t;

/*------------------------------------------------------------------------
 * Syscall context
 *
 * Passed to syscall handlers with parsed parameters from the CALL chunk.
 *------------------------------------------------------------------------*/

#define ZBC_HOST_MAX_PARMS 8
#define ZBC_HOST_MAX_DATA 4

struct zbc_host_state;

typedef struct zbc_syscall_ctx {
  /* Back-pointer to host state */
  struct zbc_host_state *state;

  /* User-provided handler context */
  void *user_context;

  /* Parsed PARM chunks */
  int parm_count;
  int64_t parms[ZBC_HOST_MAX_PARMS];

  /* Parsed DATA chunks */
  int data_count;
  struct {
    uint8_t type;  /* ZBC_DATA_TYPE_* */
    size_t size;   /* Payload size (excludes type/reserved) */
    uint8_t *data; /* Pointer into work buffer */
  } data[ZBC_HOST_MAX_DATA];

} zbc_syscall_ctx_t;

/*------------------------------------------------------------------------
 * Syscall result structure
 *
 * Handler fills this to specify the response.
 *------------------------------------------------------------------------*/

#define ZBC_HOST_MAX_RESULT_PARMS 4

typedef struct zbc_syscall_result {
  int64_t result; /* Return value (written in guest endianness) */
  int32_t error;  /* errno value (0 = success) */

  /* Return data (DATA chunk) - for reads, get_cmdline, etc. */
  void *data;       /* Pointer to return data, or NULL */
  size_t data_size; /* Size of return data in bytes */

  /* Return parameters (PARM chunks) - for heapinfo */
  int parm_count; /* Number of PARM chunks to return */
  uint8_t parm_types[ZBC_HOST_MAX_RESULT_PARMS]; /* ZBC_PARM_TYPE_* for each */
  uint64_t parm_values[ZBC_HOST_MAX_RESULT_PARMS]; /* Values to return */
} zbc_syscall_result_t;

/*------------------------------------------------------------------------
 * Syscall handler function type
 *------------------------------------------------------------------------*/

/**
 * Syscall handler function.
 *
 * @param ctx     Context with parsed parameters and state
 * @param result  Structure to fill with response
 * @return        0 on success, negative on handler error
 *
 * Handler should populate result->result and result->error.
 * For syscalls that return data (read, get_cmdline, etc.),
 * set result->data and result->data_size.
 */
typedef int (*zbc_syscall_handler_t)(zbc_syscall_ctx_t *ctx,
                                     zbc_syscall_result_t *result);

/*------------------------------------------------------------------------
 * Syscall handlers vtable
 *
 * One handler per syscall. NULL entries use default handler.
 *------------------------------------------------------------------------*/

typedef struct zbc_host_handlers {
  zbc_syscall_handler_t sys_open;          /* 0x01 */
  zbc_syscall_handler_t sys_close;         /* 0x02 */
  zbc_syscall_handler_t sys_writec;        /* 0x03 */
  zbc_syscall_handler_t sys_write0;        /* 0x04 */
  zbc_syscall_handler_t sys_write;         /* 0x05 */
  zbc_syscall_handler_t sys_read;          /* 0x06 */
  zbc_syscall_handler_t sys_readc;         /* 0x07 */
  zbc_syscall_handler_t sys_iserror;       /* 0x08 */
  zbc_syscall_handler_t sys_istty;         /* 0x09 */
  zbc_syscall_handler_t sys_seek;          /* 0x0A */
  zbc_syscall_handler_t sys_flen;          /* 0x0C */
  zbc_syscall_handler_t sys_tmpnam;        /* 0x0D */
  zbc_syscall_handler_t sys_remove;        /* 0x0E */
  zbc_syscall_handler_t sys_rename;        /* 0x0F */
  zbc_syscall_handler_t sys_clock;         /* 0x10 */
  zbc_syscall_handler_t sys_time;          /* 0x11 */
  zbc_syscall_handler_t sys_system;        /* 0x12 */
  zbc_syscall_handler_t sys_errno;         /* 0x13 */
  zbc_syscall_handler_t sys_get_cmdline;   /* 0x15 */
  zbc_syscall_handler_t sys_heapinfo;      /* 0x16 */
  zbc_syscall_handler_t sys_exit;          /* 0x18 */
  zbc_syscall_handler_t sys_exit_extended; /* 0x20 */
  zbc_syscall_handler_t sys_elapsed;       /* 0x30 */
  zbc_syscall_handler_t sys_tickfreq;      /* 0x31 */
} zbc_host_handlers_t;

/*------------------------------------------------------------------------
 * Host state structure
 *
 * Caller allocates this and passes to all host functions.
 * Initialize with zbc_host_init() before first use.
 *------------------------------------------------------------------------*/

typedef struct zbc_host_state {
  /* Guest configuration (from CNFG chunk) */
  uint8_t int_size;      /* Guest sizeof(int) */
  uint8_t ptr_size;      /* Guest sizeof(void*) */
  uint8_t endianness;    /* Guest byte order */
  uint8_t cnfg_received; /* Non-zero if CNFG has been parsed */

  /* Memory access callbacks */
  zbc_host_mem_ops_t mem_ops;
  void *mem_context;

  /* Syscall handlers */
  zbc_host_handlers_t handlers;
  void *handler_context;

  /* Work buffer for RIFF parsing */
  uint8_t *work_buf;
  size_t work_buf_size;

  /* Last errno value (for SYS_ERRNO) */
  int32_t last_errno;

} zbc_host_state_t;

/*========================================================================
 * Initialization
 *========================================================================*/

/**
 * Initialize host state.
 *
 * @param state          State structure to initialize
 * @param mem_ops        Memory access callbacks (required)
 * @param mem_context    Context pointer passed to mem_ops callbacks
 * @param work_buf       Work buffer for RIFF parsing (required)
 * @param work_buf_size  Size of work buffer (recommend 4096 bytes min)
 *
 * Must be called before any other host functions.
 * The work_buf must remain valid for the lifetime of the state.
 */
void zbc_host_init(zbc_host_state_t *state, const zbc_host_mem_ops_t *mem_ops,
                   void *mem_context, uint8_t *work_buf, size_t work_buf_size);

/**
 * Set all syscall handlers at once.
 *
 * @param state     Host state
 * @param handlers  Handlers vtable (copied into state)
 * @param context   Context pointer passed to handlers
 *
 * NULL handlers in the vtable will use default (failing) handlers.
 */
void zbc_host_set_handlers(zbc_host_state_t *state,
                           const zbc_host_handlers_t *handlers, void *context);

/**
 * Set a single syscall handler.
 *
 * @param state    Host state
 * @param opcode   Syscall opcode (SH_SYS_*)
 * @param handler  Handler function (NULL for default)
 */
void zbc_host_set_handler(zbc_host_state_t *state, uint8_t opcode,
                          zbc_syscall_handler_t handler);

/**
 * Get default handlers vtable.
 *
 * @param handlers  Structure to fill with default handlers
 *
 * Default handlers all return -1 with ENOSYS errno.
 */
void zbc_host_get_default_handlers(zbc_host_handlers_t *handlers);

/**
 * Reset CNFG state (requires new CNFG from guest).
 *
 * @param state  Host state
 */
void zbc_host_reset_cnfg(zbc_host_state_t *state);

/*========================================================================
 * Request Processing
 *========================================================================*/

/**
 * Process a semihosting request.
 *
 * @param state      Host state
 * @param riff_addr  Guest memory address of RIFF buffer
 * @return           ZBC_OK or error code
 *
 * This is the main entry point. It:
 * 1. Reads the RIFF structure from guest memory into work buffer
 * 2. Parses CNFG chunk (if present) and caches configuration
 * 3. Parses CALL chunk and extracts parameters
 * 4. Dispatches to appropriate handler
 * 5. Builds RETN or ERRO chunk
 * 6. Writes response back to guest memory (overwrites CALL)
 */
int zbc_host_process(zbc_host_state_t *state, uint64_t riff_addr);

/*========================================================================
 * Value Conversion Helpers
 *
 * These convert between host-native and guest-endian representations.
 *========================================================================*/

/**
 * Read an integer from guest-endian buffer.
 *
 * @param state  Host state (for endianness info)
 * @param buf    Buffer containing value
 * @return       Integer value (sign-extended to 64 bits)
 */
int64_t zbc_host_read_int(const zbc_host_state_t *state, const uint8_t *buf);

/**
 * Write an integer to guest-endian buffer.
 *
 * @param state  Host state (for endianness info)
 * @param buf    Buffer to write to
 * @param value  Value to write
 */
void zbc_host_write_int(const zbc_host_state_t *state, uint8_t *buf,
                        int64_t value);

/**
 * Read a pointer from guest-endian buffer.
 *
 * @param state  Host state (for endianness and ptr_size info)
 * @param buf    Buffer containing value
 * @return       Pointer value as 64-bit unsigned
 */
uint64_t zbc_host_read_ptr(const zbc_host_state_t *state, const uint8_t *buf);

/**
 * Write a pointer to guest-endian buffer.
 *
 * @param state  Host state (for endianness and ptr_size info)
 * @param buf    Buffer to write to
 * @param value  Value to write
 */
void zbc_host_write_ptr(const zbc_host_state_t *state, uint8_t *buf,
                        uint64_t value);

/*========================================================================
 * Guest Memory Access Helpers
 *
 * Convenience functions that use the mem_ops callbacks.
 *========================================================================*/

/**
 * Read bytes from guest memory into host buffer.
 *
 * @param state  Host state
 * @param dest   Destination buffer (host memory)
 * @param addr   Guest memory address
 * @param size   Number of bytes to read
 */
void zbc_host_read_guest(zbc_host_state_t *state, void *dest, uint64_t addr,
                         size_t size);

/**
 * Write bytes from host buffer to guest memory.
 *
 * @param state  Host state
 * @param addr   Guest memory address
 * @param src    Source buffer (host memory)
 * @param size   Number of bytes to write
 */
void zbc_host_write_guest(zbc_host_state_t *state, uint64_t addr,
                          const void *src, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* ZBC_SEMI_HOST_H */
