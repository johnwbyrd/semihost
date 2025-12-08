/*
 * ZBC Semihosting Protocol Definitions
 *
 * Wire protocol constants: opcodes, RIFF FourCC codes, register definitions,
 * error codes, and byte manipulation helpers.
 */

#ifndef ZBC_PROTOCOL_H
#define ZBC_PROTOCOL_H

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

#define ZBC_OK                    0
#define ZBC_ERR_NULL_ARG          1   /* NULL pointer passed */
#define ZBC_ERR_HEADER_OVERFLOW   2   /* Chunk header extends past container */
#define ZBC_ERR_DATA_OVERFLOW     3   /* Chunk data extends past container */
#define ZBC_ERR_BAD_RIFF_MAGIC    4   /* Not a RIFF container */
#define ZBC_ERR_BAD_FORM_TYPE     5   /* Wrong form type (expected SEMI) */
#define ZBC_ERR_RIFF_OVERFLOW     6   /* RIFF size exceeds buffer */
#define ZBC_ERR_NOT_FOUND         7   /* Chunk with requested ID not found */
#define ZBC_ERR_BUFFER_FULL       8   /* Not enough space to write chunk */
#define ZBC_ERR_UNKNOWN_OPCODE    9   /* Opcode not in table */
#define ZBC_ERR_NOT_INITIALIZED   10  /* State not initialized */
#define ZBC_ERR_DEVICE_ERROR      11  /* Device communication error */
#define ZBC_ERR_TIMEOUT           12  /* Operation timed out */
#define ZBC_ERR_INVALID_ARG       13  /* Invalid argument */
#define ZBC_ERR_PARSE_ERROR       14  /* Malformed RIFF data */

/*========================================================================
 * Protocol error codes (in ERRO chunk)
 *========================================================================*/

#define ZBC_PROTO_ERR_INVALID_CHUNK   0x01
#define ZBC_PROTO_ERR_MALFORMED_RIFF  0x02
#define ZBC_PROTO_ERR_MISSING_CNFG    0x03
#define ZBC_PROTO_ERR_UNSUPPORTED_OP  0x04
#define ZBC_PROTO_ERR_INVALID_PARAMS  0x05

/*========================================================================
 * RIFF chunk structures
 *
 * These structs represent the wire format. Use them for both reading
 * (overlay onto buffer) and writing (fill in fields directly).
 *
 * Note: We use [1] instead of [] for C90 compatibility. The [1] is a
 * placeholder - actual data may be larger. Access via pointer arithmetic
 * on the data field.
 *========================================================================*/

/*
 * Generic RIFF chunk: id(4) + size(4) + data[size]
 * All chunk access goes through this struct - no magic offsets.
 */
typedef struct {
    uint32_t id;      /* FourCC, little-endian */
    uint32_t size;    /* Payload size in bytes (not including this header) */
    uint8_t  data[1]; /* Chunk payload (variable length, [1] for C90) */
} zbc_chunk_t;

/*
 * RIFF container: "RIFF"(4) + size(4) + form_type(4) + chunks...
 */
typedef struct {
    uint32_t riff_id;    /* Must be ZBC_ID_RIFF */
    uint32_t size;       /* Size of everything after this field */
    uint32_t form_type;  /* e.g., ZBC_ID_SEMI */
    uint8_t  data[1];    /* Container chunks (variable length, [1] for C90) */
} zbc_riff_t;

/*========================================================================
 * Chunk payload structures
 *
 * Each chunk type has a payload struct. Access fields by name, not offset.
 *========================================================================*/

/* CNFG chunk payload */
typedef struct {
    uint8_t int_size;     /* Guest integer size (1-4) */
    uint8_t ptr_size;     /* Guest pointer size (1-8) */
    uint8_t endianness;   /* 0=little, 1=big */
    uint8_t reserved;
} zbc_cnfg_payload_t;

/* CALL chunk header (before sub-chunks) */
typedef struct {
    uint8_t opcode;       /* SH_SYS_* */
    uint8_t reserved[3];
} zbc_call_header_t;

/* PARM chunk payload */
typedef struct {
    uint8_t type;         /* ZBC_PARM_TYPE_INT or ZBC_PARM_TYPE_PTR */
    uint8_t reserved[3];
    uint8_t value[1];     /* int_size or ptr_size bytes, native endian ([1] for C90) */
} zbc_parm_payload_t;

/* DATA chunk payload */
typedef struct {
    uint8_t type;         /* ZBC_DATA_TYPE_BINARY or ZBC_DATA_TYPE_STRING */
    uint8_t reserved[3];
    uint8_t payload[1];   /* Variable-length data ([1] for C90) */
} zbc_data_payload_t;

/* ERRO chunk payload */
typedef struct {
    uint16_t error_code;  /* Protocol error code, little-endian */
    uint8_t reserved[2];
    /* Optional error message follows */
} zbc_erro_payload_t;

/*
 * RETN chunk payload:
 *   result[int_size] - native endian return value
 *   errno[4]         - little-endian errno
 *   optional DATA sub-chunk
 *
 * Note: result size varies by guest int_size, so we access via byte array.
 */
typedef struct {
    uint8_t data[1];  /* result[int_size] + errno[4] + optional sub-chunks ([1] for C90) */
} zbc_retn_payload_t;

/*========================================================================
 * Wire format size constants
 *
 * C90 requires [1] instead of [] for flexible arrays, which adds padding.
 * These constants give the actual wire format sizes for offset calculations.
 *========================================================================*/

/* RIFF header: "RIFF"(4) + size(4) + form_type(4) = 12 bytes */
#define ZBC_RIFF_HDR_SIZE    12

/* Chunk header: id(4) + size(4) = 8 bytes */
#define ZBC_CHUNK_HDR_SIZE   8

/* Round size up to word boundary (RIFF requires even-byte alignment) */
#define ZBC_PAD_SIZE(size) (((size) + 1U) & ~(size_t)1U)

/* Total bytes for a chunk on wire: header + padded payload */
#define ZBC_CHUNK_WIRE_SIZE(chunk) \
    (ZBC_CHUNK_HDR_SIZE + ZBC_PAD_SIZE((chunk)->size))

/*========================================================================
 * Payload wire sizes (without struct padding)
 *========================================================================*/

/* CNFG payload: int_size(1) + ptr_size(1) + endianness(1) + reserved(1) = 4 bytes */
#define ZBC_CNFG_PAYLOAD_SIZE    4

/* CALL header: opcode(1) + reserved(3) = 4 bytes */
#define ZBC_CALL_HDR_PAYLOAD_SIZE  4

/* PARM header: type(1) + reserved(3) = 4 bytes (value follows) */
#define ZBC_PARM_HDR_SIZE    4

/* DATA header: type(1) + reserved(3) = 4 bytes (payload follows) */
#define ZBC_DATA_HDR_SIZE    4

/* ERRO payload: error_code(2) + reserved(2) = 4 bytes */
#define ZBC_ERRO_PAYLOAD_SIZE    4

/*========================================================================
 * Legacy defines (kept for compatibility)
 *========================================================================*/

#define ZBC_HDR_SIZE         ZBC_RIFF_HDR_SIZE
#define ZBC_CNFG_DATA_SIZE   ZBC_CNFG_PAYLOAD_SIZE
#define ZBC_CNFG_TOTAL_SIZE  (ZBC_CHUNK_HDR_SIZE + ZBC_CNFG_PAYLOAD_SIZE)
#define ZBC_CALL_HDR_SIZE    (ZBC_CHUNK_HDR_SIZE + ZBC_CALL_HDR_PAYLOAD_SIZE)
#define ZBC_RETN_HDR_SIZE    ZBC_CHUNK_HDR_SIZE

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
 * New chunk-based API (struct-based, no magic offsets)
 *========================================================================*/

/*
 * Validate chunk fits within container bounds.
 *
 * chunk         - pointer to chunk to validate
 * container_end - first byte PAST the valid container region
 *
 * Returns: ZBC_OK, ZBC_ERR_NULL_ARG, ZBC_ERR_HEADER_OVERFLOW, ZBC_ERR_DATA_OVERFLOW
 */
int zbc_chunk_validate(const zbc_chunk_t *chunk, const uint8_t *container_end);

/*
 * Get pointer to next sibling chunk.
 * Caller MUST validate the returned chunk before accessing it.
 *
 * chunk - current chunk
 * out   - receives pointer to next chunk
 *
 * Returns: ZBC_OK, ZBC_ERR_NULL_ARG
 */
int zbc_chunk_next(const zbc_chunk_t *chunk, zbc_chunk_t **out);

/*
 * Get pointer to first sub-chunk within a container chunk.
 *
 * container   - parent chunk (e.g., CALL) that contains sub-chunks
 * header_size - bytes to skip before sub-chunks (e.g., sizeof(zbc_call_header_t))
 * out         - receives pointer to first sub-chunk
 *
 * Returns: ZBC_OK, ZBC_ERR_NULL_ARG
 */
int zbc_chunk_first_sub(const zbc_chunk_t *container, size_t header_size,
                        zbc_chunk_t **out);

/*
 * Get container end pointer (for validating sub-chunks).
 *
 * chunk - the container chunk
 * out   - receives pointer to first byte past chunk data
 *
 * Returns: ZBC_OK, ZBC_ERR_NULL_ARG
 */
int zbc_chunk_end(const zbc_chunk_t *chunk, const uint8_t **out);

/*
 * Find chunk by ID within container bounds.
 * Validates each chunk while searching.
 *
 * start - start of search region (first chunk)
 * end   - first byte PAST the search region
 * id    - FourCC to find
 * out   - receives pointer to found chunk
 *
 * Returns: ZBC_OK, ZBC_ERR_NULL_ARG, ZBC_ERR_NOT_FOUND,
 *          ZBC_ERR_HEADER_OVERFLOW, ZBC_ERR_DATA_OVERFLOW
 */
int zbc_chunk_find(const uint8_t *start, const uint8_t *end,
                   uint32_t id, zbc_chunk_t **out);

/*
 * Validate RIFF container.
 *
 * riff          - pointer to RIFF container
 * buf_size      - total buffer size
 * expected_form - expected form type (e.g., ZBC_ID_SEMI)
 *
 * Returns: ZBC_OK, ZBC_ERR_NULL_ARG, ZBC_ERR_BAD_RIFF_MAGIC,
 *          ZBC_ERR_BAD_FORM_TYPE, ZBC_ERR_RIFF_OVERFLOW
 */
int zbc_riff_validate(const zbc_riff_t *riff, size_t buf_size,
                      uint32_t expected_form);

/*
 * Get end pointer for RIFF container.
 *
 * riff - the RIFF container
 * out  - receives pointer to first byte past RIFF data
 *
 * Returns: ZBC_OK, ZBC_ERR_NULL_ARG
 */
int zbc_riff_end(const zbc_riff_t *riff, const uint8_t **out);

/*========================================================================
 * Parsed RIFF structure
 *
 * Parse once, then access fields. No state machine, no interleaved
 * validation. Just pointers into the original buffer.
 *========================================================================*/

#define ZBC_MAX_PARMS 8
#define ZBC_MAX_DATA  4

typedef struct {
    /* Guest configuration (from CNFG chunk) */
    uint8_t int_size;
    uint8_t ptr_size;
    uint8_t endianness;
    uint8_t has_cnfg;

    /* Request: CALL chunk info */
    uint8_t opcode;
    uint8_t has_call;

    /* Request: parameters from PARM sub-chunks */
    int parm_count;
    int parms[ZBC_MAX_PARMS];

    /* Request/Response: data from DATA sub-chunks */
    int data_count;
    struct {
        const uint8_t *ptr;
        size_t size;
    } data[ZBC_MAX_DATA];

    /* Response: RETN chunk info */
    int result;
    int host_errno;
    uint8_t has_retn;

    /* Response: ERRO chunk info */
    uint16_t proto_error;
    uint8_t has_erro;
} zbc_parsed_t;

/*
 * Parse a RIFF SEMI buffer into a zbc_parsed_t structure.
 *
 * This is the single entry point for parsing. It walks all chunks,
 * extracts relevant fields, and populates the parsed structure.
 * After this call, the caller can simply check fields like:
 *   if (parsed.has_retn) { use parsed.result; }
 *
 * buf      - RIFF buffer to parse
 * buf_size - size of buffer
 * int_size - guest int size (for decoding PARM/RETN values)
 * endian   - guest endianness (ZBC_ENDIAN_*)
 * out      - receives parsed structure
 *
 * Returns: ZBC_OK on success, error code on parse failure
 */
int zbc_riff_parse(const uint8_t *buf, size_t buf_size,
                   int int_size, int endian, zbc_parsed_t *out);

#ifdef __cplusplus
}
#endif

#endif /* ZBC_PROTOCOL_H */
