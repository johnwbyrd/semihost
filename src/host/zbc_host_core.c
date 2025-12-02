/*
 * ZBC Semihosting Host Library - Core Functions
 *
 * Initialization, RIFF parsing, backend dispatch, and response building.
 */

#include "zbc_semi_host.h"
#include <string.h>

#ifndef ENOSYS
#define ENOSYS 38 /* Function not implemented */
#endif

/*========================================================================
 * Initialization
 *========================================================================*/

int zbc_host_init(zbc_host_state_t *state,
                  const zbc_host_mem_ops_t *mem_ops,
                  void *mem_context,
                  const zbc_backend_t *backend,
                  void *backend_context,
                  unsigned char *work_buf,
                  size_t work_buf_size)
{
    if (!state || !mem_ops || !backend || !work_buf) {
        return ZBC_ERR_INVALID_ARG;
    }

    if (!mem_ops->read_u8 || !mem_ops->write_u8) {
        return ZBC_ERR_INVALID_ARG;
    }

    /* Initialize configuration as unknown */
    state->int_size = 0;
    state->ptr_size = 0;
    state->endianness = ZBC_ENDIAN_LITTLE;
    state->cnfg_received = 0;

    /* Set memory ops */
    state->mem_ops = *mem_ops;
    state->mem_context = mem_context;

    /* Set backend */
    state->backend = backend;
    state->backend_context = backend_context;

    /* Set work buffer */
    state->work_buf = work_buf;
    state->work_buf_size = work_buf_size;

    state->last_errno = 0;

    return ZBC_OK;
}

void zbc_host_reset_cnfg(zbc_host_state_t *state)
{
    if (state) {
        state->cnfg_received = 0;
        state->int_size = 0;
        state->ptr_size = 0;
    }
}

void zbc_host_set_backend(zbc_host_state_t *state,
                          const zbc_backend_t *backend,
                          void *backend_context)
{
    if (state && backend) {
        state->backend = backend;
        state->backend_context = backend_context;
    }
}

/*========================================================================
 * Guest Memory Access
 *========================================================================*/

void zbc_host_read_guest(zbc_host_state_t *state, void *dest, uint64_t addr,
                         size_t size)
{
    unsigned char *d;
    size_t i;

    if (!state || !dest || !state->mem_ops.read_u8) {
        return;
    }

    if (state->mem_ops.read_block) {
        state->mem_ops.read_block(dest, addr, size, state->mem_context);
    } else {
        d = (unsigned char *)dest;
        for (i = 0; i < size; i++) {
            d[i] = state->mem_ops.read_u8(addr + i, state->mem_context);
        }
    }
}

void zbc_host_write_guest(zbc_host_state_t *state, uint64_t addr,
                          const void *src, size_t size)
{
    const unsigned char *s;
    size_t i;

    if (!state || !src || !state->mem_ops.write_u8) {
        return;
    }

    if (state->mem_ops.write_block) {
        state->mem_ops.write_block(addr, src, size, state->mem_context);
    } else {
        s = (const unsigned char *)src;
        for (i = 0; i < size; i++) {
            state->mem_ops.write_u8(addr + i, s[i], state->mem_context);
        }
    }
}

/*========================================================================
 * Value Conversion
 *========================================================================*/

long zbc_host_read_int(const zbc_host_state_t *state, const unsigned char *buf)
{
    uint64_t value = 0;
    size_t size;
    size_t i;
    uint64_t sign_bit;

    if (!state || !buf) {
        return 0;
    }

    size = state->int_size;
    if (size == 0 || size > 8) {
        return 0;
    }

    if (state->endianness == ZBC_ENDIAN_LITTLE) {
        for (i = size; i > 0; i--) {
            value = (value << 8) | buf[i - 1];
        }
    } else if (state->endianness == ZBC_ENDIAN_BIG) {
        for (i = 0; i < size; i++) {
            value = (value << 8) | buf[i];
        }
    } else {
        /* PDP endian */
        for (i = 0; i < size; i += 2) {
            if (i + 1 < size) {
                value = (value << 16) | ((uint64_t)buf[i] << 8) | buf[i + 1];
            } else {
                value = (value << 8) | buf[i];
            }
        }
    }

    /* Sign extend */
    if (size < 8) {
        sign_bit = (uint64_t)1 << (size * 8 - 1);
        if (value & sign_bit) {
            value |= ~(((uint64_t)1 << (size * 8)) - 1);
        }
    }

    return (long)value;
}

void zbc_host_write_int(const zbc_host_state_t *state, unsigned char *buf,
                        long value)
{
    uint64_t uval;
    size_t size;
    size_t i;

    if (!state || !buf) {
        return;
    }

    size = state->int_size;
    if (size == 0 || size > 8) {
        return;
    }

    uval = (uint64_t)value;

    if (state->endianness == ZBC_ENDIAN_LITTLE) {
        for (i = 0; i < size; i++) {
            buf[i] = (unsigned char)(uval & 0xFF);
            uval >>= 8;
        }
    } else if (state->endianness == ZBC_ENDIAN_BIG) {
        for (i = size; i > 0; i--) {
            buf[i - 1] = (unsigned char)(uval & 0xFF);
            uval >>= 8;
        }
    } else {
        /* PDP endian */
        for (i = 0; i < size; i += 2) {
            if (i + 1 < size) {
                buf[i + 1] = (unsigned char)(uval & 0xFF);
                uval >>= 8;
                buf[i] = (unsigned char)(uval & 0xFF);
                uval >>= 8;
            } else {
                buf[i] = (unsigned char)(uval & 0xFF);
            }
        }
    }
}

uint64_t zbc_host_read_ptr(const zbc_host_state_t *state,
                           const unsigned char *buf)
{
    uint64_t value = 0;
    size_t size;
    size_t i;

    if (!state || !buf) {
        return 0;
    }

    size = state->ptr_size;
    if (size == 0 || size > 8) {
        return 0;
    }

    if (state->endianness == ZBC_ENDIAN_LITTLE) {
        for (i = size; i > 0; i--) {
            value = (value << 8) | buf[i - 1];
        }
    } else if (state->endianness == ZBC_ENDIAN_BIG) {
        for (i = 0; i < size; i++) {
            value = (value << 8) | buf[i];
        }
    } else {
        /* PDP endian */
        for (i = 0; i < size; i += 2) {
            if (i + 1 < size) {
                value = (value << 16) | ((uint64_t)buf[i] << 8) | buf[i + 1];
            } else {
                value = (value << 8) | buf[i];
            }
        }
    }

    return value;
}

void zbc_host_write_ptr(const zbc_host_state_t *state, unsigned char *buf,
                        uint64_t value)
{
    size_t size;
    size_t i;

    if (!state || !buf) {
        return;
    }

    size = state->ptr_size;
    if (size == 0 || size > 8) {
        return;
    }

    if (state->endianness == ZBC_ENDIAN_LITTLE) {
        for (i = 0; i < size; i++) {
            buf[i] = (unsigned char)(value & 0xFF);
            value >>= 8;
        }
    } else if (state->endianness == ZBC_ENDIAN_BIG) {
        for (i = size; i > 0; i--) {
            buf[i - 1] = (unsigned char)(value & 0xFF);
            value >>= 8;
        }
    } else {
        /* PDP endian */
        for (i = 0; i < size; i += 2) {
            if (i + 1 < size) {
                buf[i + 1] = (unsigned char)(value & 0xFF);
                value >>= 8;
                buf[i] = (unsigned char)(value & 0xFF);
                value >>= 8;
            } else {
                buf[i] = (unsigned char)(value & 0xFF);
            }
        }
    }
}

/*========================================================================
 * Internal: Write ERRO chunk to guest memory
 *========================================================================*/

static int write_erro(zbc_host_state_t *state, uint64_t addr,
                      uint16_t error_code)
{
    unsigned char erro[16];
    size_t offset;

    /* Build ERRO chunk */
    ZBC_WRITE_FOURCC(erro, 'E', 'R', 'R', 'O');
    ZBC_WRITE_U32_LE(erro + 4, 4); /* chunk size: error_code(2) + reserved(2) */
    ZBC_WRITE_U16_LE(erro + 8, error_code);
    erro[10] = 0; /* reserved */
    erro[11] = 0;

    offset = ZBC_HDR_SIZE;

    /* Skip CNFG if present */
    if (state->cnfg_received) {
        offset += ZBC_CNFG_TOTAL_SIZE;
    }

    /* Write ERRO chunk (replacing CALL) */
    zbc_host_write_guest(state, addr + offset, erro, 12);

    return ZBC_OK;
}

/*========================================================================
 * Internal: Write RETN chunk to guest memory
 *========================================================================*/

static int write_retn(zbc_host_state_t *state, uint64_t addr,
                      int result_val, int errno_val,
                      const void *data, size_t data_size,
                      const unsigned int *parm_values, int parm_count,
                      int parm_is_ptr)
{
    unsigned char retn_buf[64];
    size_t offset;
    size_t retn_size;
    size_t int_size;
    size_t ptr_size;
    size_t write_offset;
    size_t data_chunk_size;
    size_t padded_data_size;
    size_t parm_chunk_size;
    size_t padded_parm_size;
    size_t value_size;
    int p;

    int_size = state->int_size;
    ptr_size = state->ptr_size;

    /* Calculate RETN chunk data size */
    retn_size = int_size + 4; /* result + errno */

    /* Add size for PARM sub-chunks */
    value_size = parm_is_ptr ? ptr_size : int_size;
    for (p = 0; p < parm_count; p++) {
        parm_chunk_size = 4 + value_size; /* type(1) + reserved(3) + value */
        padded_parm_size = ZBC_PAD_SIZE(parm_chunk_size);
        retn_size += ZBC_CHUNK_HDR_SIZE + padded_parm_size;
    }

    /* Add size for DATA sub-chunk */
    if (data && data_size > 0) {
        data_chunk_size = 4 + data_size; /* type(1) + reserved(3) + payload */
        padded_data_size = ZBC_PAD_SIZE(data_chunk_size);
        retn_size += ZBC_CHUNK_HDR_SIZE + padded_data_size;
    }

    /* Build RETN chunk header */
    ZBC_WRITE_FOURCC(retn_buf, 'R', 'E', 'T', 'N');
    ZBC_WRITE_U32_LE(retn_buf + 4, (uint32_t)retn_size);

    /* Write result in guest endianness */
    zbc_host_write_int(state, retn_buf + 8, result_val);

    /* Write errno (always little-endian per spec) */
    ZBC_WRITE_U32_LE(retn_buf + 8 + int_size, (uint32_t)errno_val);

    write_offset = 8 + int_size + 4;

    /* Calculate position in guest memory */
    offset = ZBC_HDR_SIZE;
    if (state->cnfg_received) {
        offset += ZBC_CNFG_TOTAL_SIZE;
    }

    /* Write RETN header + result + errno */
    zbc_host_write_guest(state, addr + offset, retn_buf, write_offset);

    /* Write PARM sub-chunks if present */
    for (p = 0; p < parm_count; p++) {
        unsigned char parm_buf[32];
        size_t parm_data_size = 4 + value_size;

        ZBC_WRITE_FOURCC(parm_buf, 'P', 'A', 'R', 'M');
        ZBC_WRITE_U32_LE(parm_buf + 4, (uint32_t)parm_data_size);
        parm_buf[8] = parm_is_ptr ? ZBC_PARM_TYPE_PTR : ZBC_PARM_TYPE_INT;
        parm_buf[9] = 0;
        parm_buf[10] = 0;
        parm_buf[11] = 0;

        /* Write value in guest endianness */
        if (parm_is_ptr) {
            zbc_host_write_ptr(state, parm_buf + 12, parm_values[p]);
        } else {
            zbc_host_write_int(state, parm_buf + 12, (long)parm_values[p]);
        }

        zbc_host_write_guest(state, addr + offset + write_offset, parm_buf,
                             12 + value_size);
        write_offset += ZBC_CHUNK_HDR_SIZE + ZBC_PAD_SIZE(parm_data_size);
    }

    /* Write DATA sub-chunk if present */
    if (data && data_size > 0) {
        unsigned char data_hdr[12];
        const unsigned char *src;
        size_t i;

        data_chunk_size = 4 + data_size;

        ZBC_WRITE_FOURCC(data_hdr, 'D', 'A', 'T', 'A');
        ZBC_WRITE_U32_LE(data_hdr + 4, (uint32_t)data_chunk_size);
        data_hdr[8] = ZBC_DATA_TYPE_BINARY;
        data_hdr[9] = 0;
        data_hdr[10] = 0;
        data_hdr[11] = 0;

        zbc_host_write_guest(state, addr + offset + write_offset, data_hdr, 12);
        write_offset += 12;

        /* Write payload */
        src = (const unsigned char *)data;
        for (i = 0; i < data_size; i++) {
            state->mem_ops.write_u8(addr + offset + write_offset + i, src[i],
                                    state->mem_context);
        }

        /* Write padding byte if odd size */
        if (data_chunk_size & 1) {
            state->mem_ops.write_u8(addr + offset + write_offset + data_size,
                                    0, state->mem_context);
        }
    }

    return ZBC_OK;
}

/*========================================================================
 * Internal: Parsed call context
 *========================================================================*/

#define MAX_PARMS 8
#define MAX_DATA 4

typedef struct parsed_call {
    unsigned char opcode;
    int parm_count;
    long parms[MAX_PARMS];
    int data_count;
    struct {
        unsigned char type;
        size_t size;
        unsigned char *data;
    } data[MAX_DATA];
} parsed_call_t;

/*========================================================================
 * Main Processing Function
 *========================================================================*/

int zbc_host_process(zbc_host_state_t *state, uint64_t riff_addr)
{
    uint32_t chunk_id;
    uint32_t riff_size;
    size_t offset;
    size_t chunk_size;
    parsed_call_t call;
    const zbc_backend_t *be;
    void *ctx;
    long result;
    int err;

    if (!state || !state->work_buf || !state->mem_ops.read_u8 || !state->backend) {
        return ZBC_ERR_INVALID_ARG;
    }

    be = state->backend;
    ctx = state->backend_context;

    /* Read RIFF header (12 bytes) */
    zbc_host_read_guest(state, state->work_buf, riff_addr, ZBC_HDR_SIZE);

    /* Verify RIFF signature */
    chunk_id = ZBC_READ_U32_LE(state->work_buf);
    if (chunk_id != ZBC_ID_RIFF) {
        return write_erro(state, riff_addr, ZBC_PROTO_ERR_MALFORMED_RIFF);
    }

    /* Get RIFF size */
    riff_size = ZBC_READ_U32_LE(state->work_buf + 4);

    /* Verify SEMI form type */
    chunk_id = ZBC_READ_U32_LE(state->work_buf + 8);
    if (chunk_id != ZBC_ID_SEMI) {
        return write_erro(state, riff_addr, ZBC_PROTO_ERR_MALFORMED_RIFF);
    }

    /* Read entire RIFF structure */
    if (riff_size + 8 > state->work_buf_size) {
        return ZBC_ERR_BUFFER_TOO_SMALL;
    }

    zbc_host_read_guest(state, state->work_buf, riff_addr, riff_size + 8);

    /* Parse chunks */
    offset = ZBC_HDR_SIZE;

    /* Check for CNFG chunk */
    if (offset + ZBC_CHUNK_HDR_SIZE <= riff_size + 8) {
        chunk_id = ZBC_READ_U32_LE(state->work_buf + offset);

        if (chunk_id == ZBC_ID_CNFG) {
            chunk_size = ZBC_READ_U32_LE(state->work_buf + offset + 4);

            if (chunk_size >= ZBC_CNFG_DATA_SIZE) {
                state->int_size = state->work_buf[offset + 8];
                state->ptr_size = state->work_buf[offset + 9];
                state->endianness = state->work_buf[offset + 10];
                state->cnfg_received = 1;
            }

            offset += ZBC_CHUNK_HDR_SIZE + ZBC_PAD_SIZE(chunk_size);
        }
    }

    /* Require CNFG before processing CALL */
    if (!state->cnfg_received) {
        return write_erro(state, riff_addr, ZBC_PROTO_ERR_MISSING_CNFG);
    }

    /* Parse CALL chunk */
    if (offset + ZBC_CHUNK_HDR_SIZE > riff_size + 8) {
        return write_erro(state, riff_addr, ZBC_PROTO_ERR_INVALID_CHUNK);
    }

    chunk_id = ZBC_READ_U32_LE(state->work_buf + offset);
    if (chunk_id != ZBC_ID_CALL) {
        return write_erro(state, riff_addr, ZBC_PROTO_ERR_INVALID_CHUNK);
    }

    chunk_size = ZBC_READ_U32_LE(state->work_buf + offset + 4);

    /* Extract opcode */
    call.opcode = state->work_buf[offset + 8];
    call.parm_count = 0;
    call.data_count = 0;

    /* Parse sub-chunks (PARM and DATA) */
    {
        size_t sub_offset = offset + ZBC_CALL_HDR_SIZE;
        size_t call_end = offset + ZBC_CHUNK_HDR_SIZE + chunk_size;
        uint32_t sub_id;
        uint32_t sub_size;

        while (sub_offset + ZBC_CHUNK_HDR_SIZE <= call_end) {
            sub_id = ZBC_READ_U32_LE(state->work_buf + sub_offset);
            sub_size = ZBC_READ_U32_LE(state->work_buf + sub_offset + 4);

            if (sub_id == ZBC_ID_PARM) {
                if (call.parm_count < MAX_PARMS && sub_size >= 4) {
                    unsigned char parm_type = state->work_buf[sub_offset + 8];
                    size_t value_size;

                    if (parm_type == ZBC_PARM_TYPE_INT) {
                        value_size = state->int_size;
                    } else if (parm_type == ZBC_PARM_TYPE_PTR) {
                        value_size = state->ptr_size;
                    } else {
                        value_size = 0;
                    }

                    if (value_size > 0 && sub_size >= 4 + value_size) {
                        call.parms[call.parm_count] =
                            zbc_host_read_int(state, state->work_buf + sub_offset + 12);
                        call.parm_count++;
                    }
                }
            } else if (sub_id == ZBC_ID_DATA) {
                if (call.data_count < MAX_DATA && sub_size >= 4) {
                    call.data[call.data_count].type = state->work_buf[sub_offset + 8];
                    call.data[call.data_count].size = sub_size - 4;
                    call.data[call.data_count].data = state->work_buf + sub_offset + 12;
                    call.data_count++;
                }
            }

            sub_offset += ZBC_CHUNK_HDR_SIZE + ZBC_PAD_SIZE(sub_size);
        }
    }

    /* Dispatch to backend based on opcode */
    result = 0;
    err = 0;

    switch (call.opcode) {
    case SH_SYS_OPEN:
        if (be->open && call.data_count > 0 && call.parm_count >= 2) {
            result = be->open(ctx,
                              (const char *)call.data[0].data,
                              call.data[0].size,
                              (int)call.parms[0]);
            if (result < 0) {
                err = be->get_errno ? be->get_errno(ctx) : ENOSYS;
            }
        } else {
            result = -1;
            err = ENOSYS;
        }
        state->last_errno = err;
        return write_retn(state, riff_addr, result, err, NULL, 0, NULL, 0, 0);

    case SH_SYS_CLOSE:
        if (be->close && call.parm_count >= 1) {
            result = be->close(ctx, (int)call.parms[0]);
            if (result < 0) {
                err = be->get_errno ? be->get_errno(ctx) : ENOSYS;
            }
        } else {
            result = -1;
            err = ENOSYS;
        }
        state->last_errno = err;
        return write_retn(state, riff_addr, result, err, NULL, 0, NULL, 0, 0);

    case SH_SYS_WRITEC:
        if (be->writec && call.parm_count >= 1) {
            be->writec(ctx, (char)call.parms[0]);
            result = 0;
        }
        return write_retn(state, riff_addr, result, 0, NULL, 0, NULL, 0, 0);

    case SH_SYS_WRITE0:
        if (be->write0 && call.data_count > 0) {
            /* Ensure null termination */
            if (call.data[0].size > 0) {
                call.data[0].data[call.data[0].size - 1] = '\0';
            }
            be->write0(ctx, (const char *)call.data[0].data);
        }
        return write_retn(state, riff_addr, 0, 0, NULL, 0, NULL, 0, 0);

    case SH_SYS_WRITE:
        if (be->write && call.parm_count >= 2 && call.data_count > 0) {
            result = be->write(ctx,
                               (int)call.parms[0],
                               call.data[0].data,
                               call.data[0].size);
            if (result < 0) {
                err = be->get_errno ? be->get_errno(ctx) : ENOSYS;
            }
        } else {
            result = -1;
            err = ENOSYS;
        }
        state->last_errno = err;
        return write_retn(state, riff_addr, result, err, NULL, 0, NULL, 0, 0);

    case SH_SYS_READ:
        if (be->read && call.parm_count >= 2) {
            size_t count = (size_t)call.parms[1];
            unsigned char *read_buf;

            /* Use work buffer for read data (after parsed RIFF) */
            read_buf = state->work_buf + riff_size + 8;
            if (count > state->work_buf_size - (riff_size + 8)) {
                count = state->work_buf_size - (riff_size + 8);
            }

            result = be->read(ctx, (int)call.parms[0], read_buf, count);
            if (result < 0) {
                err = be->get_errno ? be->get_errno(ctx) : ENOSYS;
                state->last_errno = err;
                return write_retn(state, riff_addr, result, err, NULL, 0, NULL, 0, 0);
            }

            /* result = bytes NOT read, so bytes_read = count - result */
            state->last_errno = 0;
            return write_retn(state, riff_addr, result, 0,
                              read_buf, count - (size_t)result, NULL, 0, 0);
        }
        return write_retn(state, riff_addr, -1, ENOSYS, NULL, 0, NULL, 0, 0);

    case SH_SYS_READC:
        if (be->readc) {
            result = be->readc(ctx);
        } else {
            result = -1;
        }
        return write_retn(state, riff_addr, result, 0, NULL, 0, NULL, 0, 0);

    case SH_SYS_ISERROR:
        if (be->iserror && call.parm_count >= 1) {
            result = be->iserror(ctx, call.parms[0]);
        } else {
            result = (call.parm_count >= 1 && call.parms[0] < 0) ? 1 : 0;
        }
        return write_retn(state, riff_addr, result, 0, NULL, 0, NULL, 0, 0);

    case SH_SYS_ISTTY:
        if (be->istty && call.parm_count >= 1) {
            result = be->istty(ctx, (int)call.parms[0]);
        } else {
            result = 0;
        }
        return write_retn(state, riff_addr, result, 0, NULL, 0, NULL, 0, 0);

    case SH_SYS_SEEK:
        if (be->seek && call.parm_count >= 2) {
            result = be->seek(ctx, (int)call.parms[0], call.parms[1]);
            if (result < 0) {
                err = be->get_errno ? be->get_errno(ctx) : ENOSYS;
            }
        } else {
            result = -1;
            err = ENOSYS;
        }
        state->last_errno = err;
        return write_retn(state, riff_addr, result, err, NULL, 0, NULL, 0, 0);

    case SH_SYS_FLEN:
        if (be->flen && call.parm_count >= 1) {
            result = be->flen(ctx, (int)call.parms[0]);
            if (result < 0) {
                err = be->get_errno ? be->get_errno(ctx) : ENOSYS;
            }
        } else {
            result = -1;
            err = ENOSYS;
        }
        state->last_errno = err;
        return write_retn(state, riff_addr, result, err, NULL, 0, NULL, 0, 0);

    case SH_SYS_TMPNAM:
        if (be->tmpnam && call.parm_count >= 2) {
            size_t maxlen = (size_t)call.parms[1];
            unsigned char *tmp_buf = state->work_buf + riff_size + 8;

            if (maxlen > state->work_buf_size - (riff_size + 8)) {
                maxlen = state->work_buf_size - (riff_size + 8);
            }

            result = be->tmpnam(ctx, (char *)tmp_buf, maxlen, (int)call.parms[0]);
            if (result == 0) {
                size_t len = strlen((char *)tmp_buf) + 1;
                return write_retn(state, riff_addr, 0, 0, tmp_buf, len, NULL, 0, 0);
            }
            err = be->get_errno ? be->get_errno(ctx) : ENOSYS;
        } else {
            result = -1;
            err = ENOSYS;
        }
        state->last_errno = err;
        return write_retn(state, riff_addr, result, err, NULL, 0, NULL, 0, 0);

    case SH_SYS_REMOVE:
        if (be->remove && call.data_count > 0) {
            result = be->remove(ctx,
                                (const char *)call.data[0].data,
                                call.data[0].size);
            if (result < 0) {
                err = be->get_errno ? be->get_errno(ctx) : ENOSYS;
            }
        } else {
            result = -1;
            err = ENOSYS;
        }
        state->last_errno = err;
        return write_retn(state, riff_addr, result, err, NULL, 0, NULL, 0, 0);

    case SH_SYS_RENAME:
        if (be->rename && call.data_count >= 2) {
            result = be->rename(ctx,
                                (const char *)call.data[0].data,
                                call.data[0].size,
                                (const char *)call.data[1].data,
                                call.data[1].size);
            if (result < 0) {
                err = be->get_errno ? be->get_errno(ctx) : ENOSYS;
            }
        } else {
            result = -1;
            err = ENOSYS;
        }
        state->last_errno = err;
        return write_retn(state, riff_addr, result, err, NULL, 0, NULL, 0, 0);

    case SH_SYS_CLOCK:
        if (be->clock) {
            result = be->clock(ctx);
        } else {
            result = -1;
        }
        return write_retn(state, riff_addr, result, 0, NULL, 0, NULL, 0, 0);

    case SH_SYS_TIME:
        if (be->time) {
            result = be->time(ctx);
        } else {
            result = -1;
        }
        return write_retn(state, riff_addr, result, 0, NULL, 0, NULL, 0, 0);

    case SH_SYS_SYSTEM:
        if (be->do_system && call.data_count > 0) {
            result = be->do_system(ctx,
                                   (const char *)call.data[0].data,
                                   call.data[0].size);
        } else {
            result = -1;
        }
        return write_retn(state, riff_addr, result, 0, NULL, 0, NULL, 0, 0);

    case SH_SYS_ERRNO:
        result = state->last_errno;
        return write_retn(state, riff_addr, result, 0, NULL, 0, NULL, 0, 0);

    case SH_SYS_GET_CMDLINE:
        if (be->get_cmdline && call.parm_count >= 1) {
            size_t maxlen = (size_t)call.parms[0];
            unsigned char *cmd_buf = state->work_buf + riff_size + 8;

            if (maxlen > state->work_buf_size - (riff_size + 8)) {
                maxlen = state->work_buf_size - (riff_size + 8);
            }

            result = be->get_cmdline(ctx, (char *)cmd_buf, maxlen);
            if (result >= 0) {
                size_t len = strlen((char *)cmd_buf) + 1;
                return write_retn(state, riff_addr, (long)len, 0, cmd_buf, len, NULL, 0, 0);
            }
            err = be->get_errno ? be->get_errno(ctx) : ENOSYS;
        } else {
            result = -1;
            err = ENOSYS;
        }
        state->last_errno = err;
        return write_retn(state, riff_addr, result, err, NULL, 0, NULL, 0, 0);

    case SH_SYS_HEAPINFO:
        if (be->heapinfo) {
            unsigned int heap_base, heap_limit, stack_base, stack_limit;
            unsigned int parms[4];

            result = be->heapinfo(ctx, &heap_base, &heap_limit,
                                  &stack_base, &stack_limit);
            if (result == 0) {
                parms[0] = heap_base;
                parms[1] = heap_limit;
                parms[2] = stack_base;
                parms[3] = stack_limit;
                return write_retn(state, riff_addr, 0, 0, NULL, 0, parms, 4, 1);
            }
        }
        return write_retn(state, riff_addr, -1, ENOSYS, NULL, 0, NULL, 0, 0);

    case SH_SYS_EXIT:
    case SH_SYS_EXIT_EXTENDED:
        if (be->do_exit) {
            unsigned int reason = 0;
            unsigned int subcode = 0;

            if (call.parm_count >= 1) {
                reason = (unsigned int)call.parms[0];
            }
            if (call.parm_count >= 2) {
                subcode = (unsigned int)call.parms[1];
            }

            be->do_exit(ctx, reason, subcode);
        }
        /* May not return, but write response anyway */
        return write_retn(state, riff_addr, 0, 0, NULL, 0, NULL, 0, 0);

    case SH_SYS_ELAPSED:
        if (be->elapsed) {
            unsigned int lo, hi;
            unsigned char elapsed_data[8];

            result = be->elapsed(ctx, &lo, &hi);
            if (result == 0) {
                /* Return as 8-byte little-endian DATA chunk */
                ZBC_WRITE_U32_LE(elapsed_data, (uint32_t)lo);
                ZBC_WRITE_U32_LE(elapsed_data + 4, (uint32_t)hi);
                return write_retn(state, riff_addr, 0, 0, elapsed_data, 8, NULL, 0, 0);
            }
        }
        return write_retn(state, riff_addr, -1, ENOSYS, NULL, 0, NULL, 0, 0);

    case SH_SYS_TICKFREQ:
        if (be->tickfreq) {
            result = be->tickfreq(ctx);
        } else {
            result = -1;
        }
        return write_retn(state, riff_addr, result, 0, NULL, 0, NULL, 0, 0);

    default:
        return write_erro(state, riff_addr, ZBC_PROTO_ERR_UNSUPPORTED_OP);
    }
}
