/*
 * ZBC Semihosting Library
 *
 * Unified header for client and host libraries.
 * Define ZBC_CLIENT before including for client-side API.
 * Define ZBC_HOST before including for host-side API.
 */

#ifndef ZBC_SEMIHOST_H
#define ZBC_SEMIHOST_H

#ifdef __cplusplus
extern "C" {
#endif

/*========================================================================
 * Standard includes - C90 compatible
 *========================================================================*/

#include <stddef.h> /* for size_t */

#ifdef ZBC_NO_STDINT
/* Manual fallback for systems without <stdint.h> */
typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned short uint16_t;
typedef signed short int16_t;
typedef unsigned int uint32_t;
typedef signed int int32_t;
#ifdef _MSC_VER
typedef unsigned __int64 uint64_t;
typedef signed __int64 int64_t;
#else
typedef unsigned long long uint64_t;
typedef signed long long int64_t;
#endif
#if defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__) || \
    defined(__LP64__)
typedef uint64_t uintptr_t;
#else
typedef uint32_t uintptr_t;
#endif
#else
#include <stdint.h>
#endif

/*========================================================================
 * ARM Semihosting opcodes
 *========================================================================*/

#define SH_SYS_OPEN           0x01
#define SH_SYS_CLOSE          0x02
#define SH_SYS_WRITEC         0x03
#define SH_SYS_WRITE0         0x04
#define SH_SYS_WRITE          0x05
#define SH_SYS_READ           0x06
#define SH_SYS_READC          0x07
#define SH_SYS_ISERROR        0x08
#define SH_SYS_ISTTY          0x09
#define SH_SYS_SEEK           0x0A
#define SH_SYS_FLEN           0x0C
#define SH_SYS_TMPNAM         0x0D
#define SH_SYS_REMOVE         0x0E
#define SH_SYS_RENAME         0x0F
#define SH_SYS_CLOCK          0x10
#define SH_SYS_TIME           0x11
#define SH_SYS_SYSTEM         0x12
#define SH_SYS_ERRNO          0x13
#define SH_SYS_GET_CMDLINE    0x15
#define SH_SYS_HEAPINFO       0x16
#define SH_SYS_EXIT           0x18
#define SH_SYS_EXIT_EXTENDED  0x20
#define SH_SYS_ELAPSED        0x30
#define SH_SYS_TICKFREQ       0x31

/*========================================================================
 * Open mode flags (ARM semihosting compatible)
 *========================================================================*/

#define SH_OPEN_R         0   /* "r" */
#define SH_OPEN_RB        1   /* "rb" */
#define SH_OPEN_R_PLUS    2   /* "r+" */
#define SH_OPEN_R_PLUS_B  3   /* "r+b" */
#define SH_OPEN_W         4   /* "w" */
#define SH_OPEN_WB        5   /* "wb" */
#define SH_OPEN_W_PLUS    6   /* "w+" */
#define SH_OPEN_W_PLUS_B  7   /* "w+b" */
#define SH_OPEN_A         8   /* "a" */
#define SH_OPEN_AB        9   /* "ab" */
#define SH_OPEN_A_PLUS    10  /* "a+" */
#define SH_OPEN_A_PLUS_B  11  /* "a+b" */

/*========================================================================
 * RIFF FourCC codes
 *========================================================================*/

/*
 * MAKEFOURCC - construct a FourCC code from 4 characters.
 * Result is little-endian: first char at lowest address.
 * e.g., MAKEFOURCC('R','I','F','F') == 0x46464952
 */
#define ZBC_MAKEFOURCC(a, b, c, d) \
    ((uint32_t)(a) | ((uint32_t)(b) << 8) | \
     ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))

#define ZBC_ID_RIFF  ZBC_MAKEFOURCC('R','I','F','F')
#define ZBC_ID_SEMI  ZBC_MAKEFOURCC('S','E','M','I')
#define ZBC_ID_CNFG  ZBC_MAKEFOURCC('C','N','F','G')
#define ZBC_ID_CALL  ZBC_MAKEFOURCC('C','A','L','L')
#define ZBC_ID_PARM  ZBC_MAKEFOURCC('P','A','R','M')
#define ZBC_ID_DATA  ZBC_MAKEFOURCC('D','A','T','A')
#define ZBC_ID_RETN  ZBC_MAKEFOURCC('R','E','T','N')
#define ZBC_ID_ERRO  ZBC_MAKEFOURCC('E','R','R','O')

/*========================================================================
 * Endianness values for CNFG chunk
 *========================================================================*/

#define ZBC_ENDIAN_LITTLE  0
#define ZBC_ENDIAN_BIG     1

/*========================================================================
 * PARM/DATA chunk type codes
 *========================================================================*/

#define ZBC_PARM_TYPE_INT     0x01
#define ZBC_PARM_TYPE_PTR     0x02
#define ZBC_DATA_TYPE_BINARY  0x01
#define ZBC_DATA_TYPE_STRING  0x02

/*========================================================================
 * Device register offsets
 *========================================================================*/

#define ZBC_REG_SIGNATURE   0x00  /* 8 bytes, R - ASCII "SEMIHOST" */
#define ZBC_REG_RIFF_PTR    0x08  /* 16 bytes, RW - pointer to RIFF buffer */
#define ZBC_REG_DOORBELL    0x18  /* 1 byte, W - write to trigger request */
#define ZBC_REG_IRQ_STATUS  0x19  /* 1 byte, R - interrupt status flags */
#define ZBC_REG_IRQ_ENABLE  0x1A  /* 1 byte, RW - interrupt enable mask */
#define ZBC_REG_IRQ_ACK     0x1B  /* 1 byte, W - write 1s to clear IRQ bits */
#define ZBC_REG_STATUS      0x1C  /* 1 byte, R - device status flags */
#define ZBC_REG_SIZE        0x20  /* Total register space: 32 bytes */

/*========================================================================
 * Signature bytes
 *========================================================================*/

#define ZBC_SIGNATURE_SIZE   8
#define ZBC_SIGNATURE_STR    "SEMIHOST"
#define ZBC_SIGNATURE_BYTE0  0x53  /* 'S' */
#define ZBC_SIGNATURE_BYTE1  0x45  /* 'E' */
#define ZBC_SIGNATURE_BYTE2  0x4D  /* 'M' */
#define ZBC_SIGNATURE_BYTE3  0x49  /* 'I' */
#define ZBC_SIGNATURE_BYTE4  0x48  /* 'H' */
#define ZBC_SIGNATURE_BYTE5  0x4F  /* 'O' */
#define ZBC_SIGNATURE_BYTE6  0x53  /* 'S' */
#define ZBC_SIGNATURE_BYTE7  0x54  /* 'T' */

/*========================================================================
 * STATUS register bits
 *========================================================================*/

#define ZBC_STATUS_RESPONSE_READY  0x01  /* Bit 0: response available */
#define ZBC_STATUS_DEVICE_PRESENT  0x80  /* Bit 7: device exists */

/*========================================================================
 * IRQ bits
 *========================================================================*/

#define ZBC_IRQ_RESPONSE_READY  0x01
#define ZBC_IRQ_ERROR           0x02

/*========================================================================
 * Library error codes
 *========================================================================*/

#define ZBC_OK                   0
#define ZBC_ERR_BUFFER_TOO_SMALL (-1)
#define ZBC_ERR_INVALID_ARG      (-2)
#define ZBC_ERR_NOT_INITIALIZED  (-3)
#define ZBC_ERR_DEVICE_ERROR     (-4)
#define ZBC_ERR_TIMEOUT          (-5)
#define ZBC_ERR_PARSE_ERROR      (-6)
#define ZBC_ERR_UNKNOWN_OPCODE   (-7)

/*========================================================================
 * Protocol error codes (in ERRO chunk)
 *========================================================================*/

#define ZBC_PROTO_ERR_INVALID_CHUNK   0x01
#define ZBC_PROTO_ERR_MALFORMED_RIFF  0x02
#define ZBC_PROTO_ERR_MISSING_CNFG    0x03
#define ZBC_PROTO_ERR_UNSUPPORTED_OP  0x04
#define ZBC_PROTO_ERR_INVALID_PARAMS  0x05

/*========================================================================
 * Structure sizes
 *========================================================================*/

#define ZBC_HDR_SIZE         12  /* 'RIFF' + size(4) + 'SEMI' */
#define ZBC_CHUNK_HDR_SIZE   8   /* FourCC(4) + size(4) */
#define ZBC_CNFG_DATA_SIZE   4   /* int_size + ptr_size + endianness + reserved */
#define ZBC_CNFG_TOTAL_SIZE  12  /* header(8) + data(4) */
#define ZBC_CALL_HDR_SIZE    12  /* header(8) + opcode(1) + reserved(3) */
#define ZBC_PARM_HDR_SIZE    12  /* header(8) + type(1) + reserved(3) */
#define ZBC_DATA_HDR_SIZE    12  /* header(8) + type(1) + reserved(3) */
#define ZBC_RETN_HDR_SIZE    8   /* header(8), result and errno follow */

/*========================================================================
 * Helper macros for little-endian byte manipulation
 *========================================================================*/

#define ZBC_WRITE_U32_LE(buf, val) \
    do { \
        unsigned char *_p = (unsigned char *)(buf); \
        uint32_t _v = (uint32_t)(val); \
        _p[0] = (unsigned char)(_v & 0xFFU); \
        _p[1] = (unsigned char)((_v >> 8) & 0xFFU); \
        _p[2] = (unsigned char)((_v >> 16) & 0xFFU); \
        _p[3] = (unsigned char)((_v >> 24) & 0xFFU); \
    } while (0)

#define ZBC_READ_U32_LE(buf) \
    ((uint32_t)(((const unsigned char *)(buf))[0]) | \
     ((uint32_t)(((const unsigned char *)(buf))[1]) << 8) | \
     ((uint32_t)(((const unsigned char *)(buf))[2]) << 16) | \
     ((uint32_t)(((const unsigned char *)(buf))[3]) << 24))

#define ZBC_WRITE_U16_LE(buf, val) \
    do { \
        unsigned char *_p = (unsigned char *)(buf); \
        uint16_t _v = (uint16_t)(val); \
        _p[0] = (unsigned char)(_v & 0xFFU); \
        _p[1] = (unsigned char)((_v >> 8) & 0xFFU); \
    } while (0)

#define ZBC_READ_U16_LE(buf) \
    ((uint16_t)(((const unsigned char *)(buf))[0]) | \
     ((uint16_t)(((const unsigned char *)(buf))[1]) << 8))

#define ZBC_PAD_SIZE(size) (((size) + 1U) & ~(size_t)1U)

#define ZBC_WRITE_FOURCC(buf, c0, c1, c2, c3) \
    do { \
        unsigned char *_p = (unsigned char *)(buf); \
        _p[0] = (unsigned char)(c0); \
        _p[1] = (unsigned char)(c1); \
        _p[2] = (unsigned char)(c2); \
        _p[3] = (unsigned char)(c3); \
    } while (0)

/*========================================================================
 * Memory helpers (no libc dependency for bare-metal targets)
 *========================================================================*/

#define ZBC_MEMCPY(dst, src, n) \
    do { \
        unsigned char *_d = (unsigned char *)(dst); \
        const unsigned char *_s = (const unsigned char *)(src); \
        size_t _n = (n); \
        while (_n-- > 0) *_d++ = *_s++; \
    } while (0)

#define ZBC_MEMSET(dst, val, n) \
    do { \
        unsigned char *_d = (unsigned char *)(dst); \
        unsigned char _v = (unsigned char)(val); \
        size_t _n = (n); \
        while (_n-- > 0) *_d++ = _v; \
    } while (0)

/*========================================================================
 * Opcode table types
 *========================================================================*/

/* Chunk types for request building */
#define ZBC_CHUNK_NONE       0  /* Unused slot */
#define ZBC_CHUNK_PARM_INT   1  /* PARM chunk with signed int */
#define ZBC_CHUNK_PARM_UINT  2  /* PARM chunk with unsigned int */
#define ZBC_CHUNK_DATA_PTR   3  /* DATA chunk: ptr from slot, len from len_slot */
#define ZBC_CHUNK_DATA_STR   4  /* DATA chunk: null-terminated string */
#define ZBC_CHUNK_DATA_BYTE  5  /* DATA chunk: single byte from *(uint8_t*)slot */

/* Response types */
#define ZBC_RESP_INT       0  /* Returns integer in result */
#define ZBC_RESP_DATA      1  /* Returns DATA chunk, copy to dest_slot */
#define ZBC_RESP_HEAPINFO  2  /* Returns 4 pointer values */
#define ZBC_RESP_ELAPSED   3  /* Returns 8-byte tick count */

/* Single parameter/data chunk descriptor */
typedef struct {
    uint8_t type;      /* ZBC_CHUNK_* */
    uint8_t slot;      /* args[] index for value or pointer */
    uint8_t len_slot;  /* args[] index for length (DATA_PTR only) */
} zbc_chunk_desc_t;

/* Opcode table entry */
typedef struct {
    uint8_t opcode;              /* SH_SYS_* */
    uint8_t arg_count;           /* Number of args[] slots used */
    zbc_chunk_desc_t params[4];  /* Request chunks to emit */
    uint8_t resp_type;           /* ZBC_RESP_* */
    uint8_t resp_dest;           /* args[] index for response data */
    uint8_t resp_len_slot;       /* args[] index for max length */
} zbc_opcode_entry_t;

/* Opcode table lookup */
const zbc_opcode_entry_t *zbc_opcode_lookup(int opcode);
int zbc_opcode_count(void);

/*========================================================================
 * RIFF helper functions (shared by client and host)
 *========================================================================*/

/* String length (no libc) */
size_t zbc_strlen(const char *s);

/* Native endianness read/write */
void zbc_write_native_uint(uint8_t *buf, unsigned int value, int size,
                           int endianness);
int zbc_read_native_int(const uint8_t *buf, int size, int endianness);
unsigned int zbc_read_native_uint(const uint8_t *buf, int size, int endianness);

/* RIFF chunk writing */
uint8_t *zbc_riff_begin_chunk(uint8_t *buf, size_t capacity, size_t *offset,
                              uint32_t fourcc);
void zbc_riff_patch_size(uint8_t *size_ptr, size_t data_size);
int zbc_riff_write_bytes(uint8_t *buf, size_t capacity, size_t *offset,
                         const void *data, size_t size);
void zbc_riff_pad(uint8_t *buf, size_t capacity, size_t *offset);

/* RIFF chunk reading */
int zbc_riff_read_header(const uint8_t *buf, size_t capacity, size_t offset,
                         uint32_t *fourcc, uint32_t *size);
size_t zbc_riff_skip_chunk(const uint8_t *buf, size_t capacity, size_t offset);

/* RIFF container */
uint8_t *zbc_riff_begin_container(uint8_t *buf, size_t capacity, size_t *offset,
                                  uint32_t form_type);
int zbc_riff_validate_container(const uint8_t *buf, size_t capacity,
                                uint32_t expected_form_type);

/*========================================================================
 * Client-side API (define ZBC_CLIENT to enable)
 *========================================================================*/

#ifdef ZBC_CLIENT

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
 * state    - initialized client state
 * buf      - RIFF buffer (caller-provided)
 * buf_size - size of buffer
 * opcode   - SH_SYS_* opcode
 * args     - array of arguments (interpretation depends on opcode)
 *
 * Returns syscall result, or (uintptr_t)-1 on error.
 */
uintptr_t zbc_call(zbc_client_state_t *state, void *buf, size_t buf_size,
                   int opcode, uintptr_t *args);

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

#endif /* ZBC_CLIENT */

/*========================================================================
 * Host-side API (define ZBC_HOST to enable)
 *========================================================================*/

#ifdef ZBC_HOST

/* Memory access operations for reading/writing guest memory */
typedef struct {
    uint8_t (*read_u8)(uint64_t addr, void *ctx);
    void (*write_u8)(uint64_t addr, uint8_t val, void *ctx);
    void (*read_block)(void *dest, uint64_t addr, size_t size, void *ctx);
    void (*write_block)(uint64_t addr, const void *src, size_t size, void *ctx);
} zbc_host_mem_ops_t;

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

#endif /* ZBC_HOST */

/*========================================================================
 * Backend API (included with ZBC_HOST)
 *========================================================================*/

#ifdef ZBC_HOST

typedef struct zbc_backend_s {
    /* File operations */
    int (*open)(void *ctx, const char *path, size_t path_len, int mode);
    int (*close)(void *ctx, int fd);
    int (*read)(void *ctx, int fd, void *buf, size_t count);
    int (*write)(void *ctx, int fd, const void *buf, size_t count);
    int (*seek)(void *ctx, int fd, int pos);
    int (*flen)(void *ctx, int fd);
    int (*remove)(void *ctx, const char *path, size_t path_len);
    int (*rename)(void *ctx, const char *old_path, size_t old_len,
                  const char *new_path, size_t new_len);
    int (*tmpnam)(void *ctx, char *buf, size_t buf_size, int id);

    /* Console operations */
    void (*writec)(void *ctx, char c);
    void (*write0)(void *ctx, const char *str);
    int (*readc)(void *ctx);

    /* Status operations */
    int (*iserror)(void *ctx, int status);
    int (*istty)(void *ctx, int fd);
    int (*clock)(void *ctx);
    int (*time)(void *ctx);
    int (*elapsed)(void *ctx, unsigned int *lo, unsigned int *hi);
    int (*tickfreq)(void *ctx);

    /* System operations */
    int (*do_system)(void *ctx, const char *cmd, size_t cmd_len);
    int (*get_cmdline)(void *ctx, char *buf, size_t buf_size);
    int (*heapinfo)(void *ctx, unsigned int *heap_base, unsigned int *heap_limit,
                    unsigned int *stack_base, unsigned int *stack_limit);
    void (*do_exit)(void *ctx, unsigned int reason, unsigned int subcode);
    int (*get_errno)(void *ctx);
} zbc_backend_t;

/* Built-in backends */
const zbc_backend_t *zbc_backend_ansi(void);
const zbc_backend_t *zbc_backend_dummy(void);

/* ANSI backend cleanup (close any open files) */
void zbc_backend_ansi_cleanup(void);

#endif /* ZBC_HOST */

#ifdef __cplusplus
}
#endif

#endif /* ZBC_SEMIHOST_H */
