/*
 * ZBC Semihosting Common Definitions
 *
 * Shared definitions for both client (guest) and host (emulator) libraries.
 */

#ifndef ZBC_SEMI_COMMON_H
#define ZBC_SEMI_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

/*------------------------------------------------------------------------
 * Standard includes - C90 compatible
 *------------------------------------------------------------------------*/

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#include <stdint.h>
#else
/* C90 fallback - platform may need to override these */
typedef unsigned char      uint8_t;
typedef signed char        int8_t;
typedef unsigned short     uint16_t;
typedef signed short       int16_t;
typedef unsigned long      uint32_t;
typedef signed long        int32_t;
#ifdef _MSC_VER
typedef unsigned __int64   uint64_t;
typedef signed __int64     int64_t;
#else
/* Many C90 compilers support long long as extension */
typedef unsigned long long uint64_t;
typedef signed long long   int64_t;
#endif
/* uintptr_t: unsigned type capable of holding a pointer */
#if defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__) || defined(__LP64__)
typedef uint64_t uintptr_t;
#else
typedef uint32_t uintptr_t;
#endif
#endif

#include <stddef.h>  /* for size_t */

/*------------------------------------------------------------------------
 * RIFF FourCC codes
 *
 * Stored as uint32_t in little-endian format for easy comparison.
 * The byte order in memory is: first char at lowest address.
 *------------------------------------------------------------------------*/

#define ZBC_ID_RIFF  0x46464952UL  /* 'RIFF' */
#define ZBC_ID_SEMI  0x494D4553UL  /* 'SEMI' */
#define ZBC_ID_CNFG  0x47464E43UL  /* 'CNFG' */
#define ZBC_ID_CALL  0x4C4C4143UL  /* 'CALL' */
#define ZBC_ID_PARM  0x4D524150UL  /* 'PARM' */
#define ZBC_ID_DATA  0x41544144UL  /* 'DATA' */
#define ZBC_ID_RETN  0x4E544552UL  /* 'RETN' */
#define ZBC_ID_ERRO  0x4F525245UL  /* 'ERRO' */

/*------------------------------------------------------------------------
 * Endianness values for CNFG chunk
 *------------------------------------------------------------------------*/

#define ZBC_ENDIAN_LITTLE  0
#define ZBC_ENDIAN_BIG     1
#define ZBC_ENDIAN_PDP     2

/*------------------------------------------------------------------------
 * PARM chunk parameter types
 *------------------------------------------------------------------------*/

#define ZBC_PARM_TYPE_INT  0x01
#define ZBC_PARM_TYPE_PTR  0x02

/*------------------------------------------------------------------------
 * DATA chunk data types
 *------------------------------------------------------------------------*/

#define ZBC_DATA_TYPE_BINARY  0x01
#define ZBC_DATA_TYPE_STRING  0x02

/*------------------------------------------------------------------------
 * ARM Semihosting opcodes
 *------------------------------------------------------------------------*/

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
/* 0x0B is not used */
#define SH_SYS_FLEN           0x0C
#define SH_SYS_TMPNAM         0x0D
#define SH_SYS_REMOVE         0x0E
#define SH_SYS_RENAME         0x0F
#define SH_SYS_CLOCK          0x10
#define SH_SYS_TIME           0x11
#define SH_SYS_SYSTEM         0x12
#define SH_SYS_ERRNO          0x13
/* 0x14 is not used */
#define SH_SYS_GET_CMDLINE    0x15
#define SH_SYS_HEAPINFO       0x16
/* 0x17 is not used */
#define SH_SYS_EXIT           0x18
/* 0x19-0x1F not used */
#define SH_SYS_EXIT_EXTENDED  0x20
/* 0x21-0x2F not used */
#define SH_SYS_ELAPSED        0x30
#define SH_SYS_TICKFREQ       0x31

/*------------------------------------------------------------------------
 * Device register offsets
 *
 * The semihosting device presents 32 bytes of memory-mapped registers.
 *------------------------------------------------------------------------*/

#define ZBC_REG_SIGNATURE    0x00  /* 8 bytes, R - ASCII "SEMIHOST" */
#define ZBC_REG_RIFF_PTR     0x08  /* 16 bytes, RW - pointer to RIFF buffer */
#define ZBC_REG_DOORBELL     0x18  /* 1 byte, W - write to trigger request */
#define ZBC_REG_IRQ_STATUS   0x19  /* 1 byte, R - interrupt status flags */
#define ZBC_REG_IRQ_ENABLE   0x1A  /* 1 byte, RW - interrupt enable mask */
#define ZBC_REG_IRQ_ACK      0x1B  /* 1 byte, W - write 1s to clear IRQ bits */
#define ZBC_REG_STATUS       0x1C  /* 1 byte, R - device status flags */
/* 0x1D-0x1F reserved */

#define ZBC_REG_SIZE         0x20  /* Total register space: 32 bytes */

/*------------------------------------------------------------------------
 * SIGNATURE register value
 *------------------------------------------------------------------------*/

#define ZBC_SIGNATURE_SIZE   8
#define ZBC_SIGNATURE_STR    "SEMIHOST"
/* Individual bytes for verification */
#define ZBC_SIGNATURE_BYTE0  0x53  /* 'S' */
#define ZBC_SIGNATURE_BYTE1  0x45  /* 'E' */
#define ZBC_SIGNATURE_BYTE2  0x4D  /* 'M' */
#define ZBC_SIGNATURE_BYTE3  0x49  /* 'I' */
#define ZBC_SIGNATURE_BYTE4  0x48  /* 'H' */
#define ZBC_SIGNATURE_BYTE5  0x4F  /* 'O' */
#define ZBC_SIGNATURE_BYTE6  0x53  /* 'S' */
#define ZBC_SIGNATURE_BYTE7  0x54  /* 'T' */

/*------------------------------------------------------------------------
 * STATUS register bits
 *------------------------------------------------------------------------*/

#define ZBC_STATUS_RESPONSE_READY  0x01  /* Bit 0: response available */
#define ZBC_STATUS_DEVICE_PRESENT  0x80  /* Bit 7: device exists */

/*------------------------------------------------------------------------
 * IRQ_STATUS and IRQ_ENABLE bits
 *------------------------------------------------------------------------*/

#define ZBC_IRQ_RESPONSE_READY  0x01  /* Bit 0: response ready interrupt */
#define ZBC_IRQ_ERROR           0x02  /* Bit 1: error interrupt */

/*------------------------------------------------------------------------
 * Open mode flags (ARM semihosting compatible)
 *------------------------------------------------------------------------*/

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

/*------------------------------------------------------------------------
 * Library error codes (negative values for client-side errors)
 *------------------------------------------------------------------------*/

#define ZBC_OK                    0
#define ZBC_ERR_BUFFER_TOO_SMALL  (-1)
#define ZBC_ERR_INVALID_ARG       (-2)
#define ZBC_ERR_NOT_INITIALIZED   (-3)
#define ZBC_ERR_DEVICE_ERROR      (-4)
#define ZBC_ERR_TIMEOUT           (-5)
#define ZBC_ERR_PARSE_ERROR       (-6)

/*------------------------------------------------------------------------
 * ERRO chunk error codes (protocol-level errors)
 *------------------------------------------------------------------------*/

#define ZBC_PROTO_ERR_INVALID_CHUNK    0x01
#define ZBC_PROTO_ERR_MALFORMED_RIFF   0x02
#define ZBC_PROTO_ERR_MISSING_CNFG     0x03
#define ZBC_PROTO_ERR_UNSUPPORTED_OP   0x04
#define ZBC_PROTO_ERR_INVALID_PARAMS   0x05

/*------------------------------------------------------------------------
 * Structure sizes
 *------------------------------------------------------------------------*/

#define ZBC_HDR_SIZE         12  /* 'RIFF' + size(4) + 'SEMI' */
#define ZBC_CHUNK_HDR_SIZE    8  /* FourCC(4) + size(4) */
#define ZBC_CNFG_DATA_SIZE    4  /* int_size + ptr_size + endianness + reserved */
#define ZBC_CNFG_TOTAL_SIZE  12  /* header(8) + data(4) */
#define ZBC_CALL_HDR_SIZE    12  /* header(8) + opcode(1) + reserved(3) */
#define ZBC_PARM_HDR_SIZE    12  /* header(8) + type(1) + reserved(3) */
#define ZBC_DATA_HDR_SIZE    12  /* header(8) + type(1) + reserved(3) */
#define ZBC_RETN_HDR_SIZE     8  /* header(8), result and errno follow */

/*------------------------------------------------------------------------
 * Helper macros for little-endian byte manipulation
 *
 * RIFF format requires little-endian for all structure fields.
 * These macros work on any byte order host.
 *------------------------------------------------------------------------*/

/* Write a 32-bit value in little-endian format to a byte buffer */
#define ZBC_WRITE_U32_LE(buf, val) do { \
    unsigned char *_p = (unsigned char *)(buf); \
    uint32_t _v = (uint32_t)(val); \
    _p[0] = (unsigned char)(_v & 0xFFU); \
    _p[1] = (unsigned char)((_v >> 8) & 0xFFU); \
    _p[2] = (unsigned char)((_v >> 16) & 0xFFU); \
    _p[3] = (unsigned char)((_v >> 24) & 0xFFU); \
} while(0)

/* Read a 32-bit little-endian value from a byte buffer */
#define ZBC_READ_U32_LE(buf) ( \
    (uint32_t)(((const unsigned char *)(buf))[0]) | \
    ((uint32_t)(((const unsigned char *)(buf))[1]) << 8) | \
    ((uint32_t)(((const unsigned char *)(buf))[2]) << 16) | \
    ((uint32_t)(((const unsigned char *)(buf))[3]) << 24) \
)

/* Write a 16-bit value in little-endian format */
#define ZBC_WRITE_U16_LE(buf, val) do { \
    unsigned char *_p = (unsigned char *)(buf); \
    uint16_t _v = (uint16_t)(val); \
    _p[0] = (unsigned char)(_v & 0xFFU); \
    _p[1] = (unsigned char)((_v >> 8) & 0xFFU); \
} while(0)

/* Read a 16-bit little-endian value */
#define ZBC_READ_U16_LE(buf) ( \
    (uint16_t)(((const unsigned char *)(buf))[0]) | \
    ((uint16_t)(((const unsigned char *)(buf))[1]) << 8) \
)

/* Pad size to even byte boundary per RIFF specification */
#define ZBC_PAD_SIZE(size) (((size) + 1U) & ~(size_t)1U)

/*------------------------------------------------------------------------
 * FourCC writing helper
 *------------------------------------------------------------------------*/

#define ZBC_WRITE_FOURCC(buf, c0, c1, c2, c3) do { \
    unsigned char *_p = (unsigned char *)(buf); \
    _p[0] = (unsigned char)(c0); \
    _p[1] = (unsigned char)(c1); \
    _p[2] = (unsigned char)(c2); \
    _p[3] = (unsigned char)(c3); \
} while(0)

/*------------------------------------------------------------------------
 * Memory copy helper (C90 compatible, avoids memcpy dependency)
 *------------------------------------------------------------------------*/

#define ZBC_MEMCPY(dst, src, n) do { \
    unsigned char *_d = (unsigned char *)(dst); \
    const unsigned char *_s = (const unsigned char *)(src); \
    size_t _n = (n); \
    while (_n-- > 0) *_d++ = *_s++; \
} while(0)

#define ZBC_MEMSET(dst, val, n) do { \
    unsigned char *_d = (unsigned char *)(dst); \
    unsigned char _v = (unsigned char)(val); \
    size_t _n = (n); \
    while (_n-- > 0) *_d++ = _v; \
} while(0)

#ifdef __cplusplus
}
#endif

#endif /* ZBC_SEMI_COMMON_H */
