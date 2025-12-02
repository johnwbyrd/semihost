/*
 * ZBC Semihosting Host Library
 *
 * Parses RIFF requests, dispatches to backend, builds responses.
 */

#include "zbc_semihost.h"

#ifndef ENOSYS
#define ENOSYS 38
#endif

/*========================================================================
 * Initialization
 *========================================================================*/

void zbc_host_init(zbc_host_state_t *state,
                   const zbc_host_mem_ops_t *mem_ops, void *mem_ctx,
                   const zbc_backend_t *backend, void *backend_ctx,
                   uint8_t *work_buf, size_t work_buf_size)
{
    if (!state) {
        return;
    }

    state->mem_ops = *mem_ops;
    state->mem_ctx = mem_ctx;
    state->backend = backend;
    state->backend_ctx = backend_ctx;
    state->work_buf = work_buf;
    state->work_buf_size = work_buf_size;
    state->guest_int_size = 0;
    state->guest_ptr_size = 0;
    state->guest_endianness = ZBC_ENDIAN_LITTLE;
    state->cnfg_received = 0;
}

/*========================================================================
 * Guest memory access
 *========================================================================*/

static void read_guest(zbc_host_state_t *state, void *dest, uint64_t addr,
                       size_t size)
{
    if (state->mem_ops.read_block) {
        state->mem_ops.read_block(dest, addr, size, state->mem_ctx);
    } else {
        uint8_t *d = (uint8_t *)dest;
        size_t i;
        for (i = 0; i < size; i++) {
            d[i] = state->mem_ops.read_u8(addr + i, state->mem_ctx);
        }
    }
}

static void write_guest(zbc_host_state_t *state, uint64_t addr,
                        const void *src, size_t size)
{
    if (state->mem_ops.write_block) {
        state->mem_ops.write_block(addr, src, size, state->mem_ctx);
    } else {
        const uint8_t *s = (const uint8_t *)src;
        size_t i;
        for (i = 0; i < size; i++) {
            state->mem_ops.write_u8(addr + i, s[i], state->mem_ctx);
        }
    }
}

/*========================================================================
 * Value conversion (guest endianness)
 *========================================================================*/

int zbc_host_read_guest_int(const zbc_host_state_t *state,
                            const uint8_t *data, size_t size)
{
    return zbc_read_native_int(data, (int)size, state->guest_endianness);
}

void zbc_host_write_guest_int(const zbc_host_state_t *state,
                              uint8_t *data, int value, size_t size)
{
    zbc_write_native_uint(data, (unsigned int)value, (int)size,
                          state->guest_endianness);
}

/*========================================================================
 * Response building
 *========================================================================*/

static void write_erro(zbc_host_state_t *state, uint64_t addr, int error_code)
{
    uint8_t erro[12];
    size_t offset;

    ZBC_WRITE_U32_LE(erro, ZBC_ID_ERRO);
    ZBC_WRITE_U32_LE(erro + 4, 4);
    ZBC_WRITE_U16_LE(erro + 8, (uint16_t)error_code);
    erro[10] = 0;
    erro[11] = 0;

    offset = ZBC_HDR_SIZE;
    if (state->cnfg_received) {
        offset += ZBC_CNFG_TOTAL_SIZE;
    }

    write_guest(state, addr + offset, erro, 12);
}

static void write_retn(zbc_host_state_t *state, uint64_t addr,
                       int result, int err,
                       const void *data, size_t data_size)
{
    uint8_t buf[64];
    size_t offset;
    size_t write_pos;
    size_t retn_size;
    int int_size;

    int_size = state->guest_int_size;
    retn_size = int_size + 4;  /* result + errno */

    if (data && data_size > 0) {
        retn_size += ZBC_CHUNK_HDR_SIZE + 4 + ZBC_PAD_SIZE(data_size);
    }

    /* RETN header */
    ZBC_WRITE_U32_LE(buf, ZBC_ID_RETN);
    ZBC_WRITE_U32_LE(buf + 4, (uint32_t)retn_size);

    /* Result in guest endianness */
    zbc_host_write_guest_int(state, buf + 8, result, int_size);

    /* Errno in little-endian */
    ZBC_WRITE_U32_LE(buf + 8 + int_size, (uint32_t)err);

    write_pos = 8 + int_size + 4;

    /* Write RETN header */
    offset = ZBC_HDR_SIZE;
    if (state->cnfg_received) {
        offset += ZBC_CNFG_TOTAL_SIZE;
    }

    write_guest(state, addr + offset, buf, write_pos);

    /* Write DATA sub-chunk if present */
    if (data && data_size > 0) {
        uint8_t data_hdr[12];
        size_t chunk_size = 4 + data_size;

        ZBC_WRITE_U32_LE(data_hdr, ZBC_ID_DATA);
        ZBC_WRITE_U32_LE(data_hdr + 4, (uint32_t)chunk_size);
        data_hdr[8] = ZBC_DATA_TYPE_BINARY;
        data_hdr[9] = 0;
        data_hdr[10] = 0;
        data_hdr[11] = 0;

        write_guest(state, addr + offset + write_pos, data_hdr, 12);
        write_guest(state, addr + offset + write_pos + 12, data, data_size);

        if (chunk_size & 1) {
            uint8_t pad = 0;
            write_guest(state, addr + offset + write_pos + 12 + data_size, &pad, 1);
        }
    }
}

/*========================================================================
 * Parsed request context
 *========================================================================*/

#define MAX_PARMS 8
#define MAX_DATA  4

typedef struct {
    int opcode;
    int parm_count;
    int parms[MAX_PARMS];
    int data_count;
    struct {
        uint8_t *ptr;
        size_t size;
    } data[MAX_DATA];
} parsed_call_t;

/*========================================================================
 * Request parsing
 *========================================================================*/

static int parse_request(zbc_host_state_t *state, uint64_t riff_addr,
                         parsed_call_t *call)
{
    uint8_t *buf = state->work_buf;
    size_t capacity = state->work_buf_size;
    uint32_t chunk_id, chunk_size, riff_size;
    size_t offset, sub_offset, call_end;

    /* Read RIFF header */
    read_guest(state, buf, riff_addr, ZBC_HDR_SIZE);

    if (zbc_riff_validate_container(buf, capacity, ZBC_ID_SEMI) < 0) {
        write_erro(state, riff_addr, ZBC_PROTO_ERR_MALFORMED_RIFF);
        return -1;
    }

    riff_size = ZBC_READ_U32_LE(buf + 4);
    if (riff_size + 8 > capacity) {
        return ZBC_ERR_BUFFER_TOO_SMALL;
    }

    /* Read entire RIFF structure */
    read_guest(state, buf, riff_addr, riff_size + 8);

    offset = ZBC_HDR_SIZE;

    /* Check for CNFG */
    if (zbc_riff_read_header(buf, capacity, offset, &chunk_id, &chunk_size) == 0) {
        if (chunk_id == ZBC_ID_CNFG && chunk_size >= ZBC_CNFG_DATA_SIZE) {
            state->guest_int_size = buf[offset + 8];
            state->guest_ptr_size = buf[offset + 9];
            state->guest_endianness = buf[offset + 10];
            state->cnfg_received = 1;
            offset += ZBC_CHUNK_HDR_SIZE + ZBC_PAD_SIZE(chunk_size);
        }
    }

    if (!state->cnfg_received) {
        write_erro(state, riff_addr, ZBC_PROTO_ERR_MISSING_CNFG);
        return -1;
    }

    /* Parse CALL chunk */
    if (zbc_riff_read_header(buf, capacity, offset, &chunk_id, &chunk_size) < 0 ||
        chunk_id != ZBC_ID_CALL) {
        write_erro(state, riff_addr, ZBC_PROTO_ERR_INVALID_CHUNK);
        return -1;
    }

    call->opcode = buf[offset + 8];
    call->parm_count = 0;
    call->data_count = 0;

    /* Parse PARM and DATA sub-chunks */
    sub_offset = offset + ZBC_CALL_HDR_SIZE;
    call_end = offset + ZBC_CHUNK_HDR_SIZE + chunk_size;

    while (sub_offset + ZBC_CHUNK_HDR_SIZE <= call_end) {
        uint32_t sub_id = ZBC_READ_U32_LE(buf + sub_offset);
        uint32_t sub_size = ZBC_READ_U32_LE(buf + sub_offset + 4);

        if (sub_id == ZBC_ID_PARM && call->parm_count < MAX_PARMS && sub_size >= 4) {
            int value_size = (buf[sub_offset + 8] == ZBC_PARM_TYPE_PTR) ?
                             state->guest_ptr_size : state->guest_int_size;
            if (sub_size >= 4 + (uint32_t)value_size) {
                call->parms[call->parm_count] = zbc_host_read_guest_int(
                    state, buf + sub_offset + 12, value_size);
                call->parm_count++;
            }
        } else if (sub_id == ZBC_ID_DATA && call->data_count < MAX_DATA && sub_size >= 4) {
            call->data[call->data_count].ptr = buf + sub_offset + 12;
            call->data[call->data_count].size = sub_size - 4;
            call->data_count++;
        }

        sub_offset += ZBC_CHUNK_HDR_SIZE + ZBC_PAD_SIZE(sub_size);
    }

    return 0;
}

/*========================================================================
 * Backend dispatch
 *========================================================================*/

int zbc_host_process(zbc_host_state_t *state, uint64_t riff_addr)
{
    parsed_call_t call;
    const zbc_backend_t *be;
    void *ctx;
    int result = 0;
    int err = 0;

    if (!state || !state->work_buf || !state->backend) {
        return ZBC_ERR_INVALID_ARG;
    }

    if (parse_request(state, riff_addr, &call) < 0) {
        return ZBC_ERR_PARSE_ERROR;
    }

    be = state->backend;
    ctx = state->backend_ctx;

    /*
     * Dispatch to backend.
     *
     * Most cases follow a pattern:
     *   1. Check backend function and parameter count
     *   2. Call backend
     *   3. Get errno on error
     *   4. Write response
     *
     * We keep the switch for clarity since each opcode has
     * slightly different parameter extraction and validation.
     */

    switch (call.opcode) {

    /* File operations */

    case SH_SYS_OPEN:
        if (be->open && call.data_count > 0 && call.parm_count >= 2) {
            result = be->open(ctx, (const char *)call.data[0].ptr,
                              call.data[0].size, call.parms[0]);
            if (result < 0 && be->get_errno) {
                err = be->get_errno(ctx);
            }
        } else {
            result = -1;
            err = ENOSYS;
        }
        write_retn(state, riff_addr, result, err, NULL, 0);
        break;

    case SH_SYS_CLOSE:
        if (be->close && call.parm_count >= 1) {
            result = be->close(ctx, call.parms[0]);
            if (result < 0 && be->get_errno) {
                err = be->get_errno(ctx);
            }
        } else {
            result = -1;
            err = ENOSYS;
        }
        write_retn(state, riff_addr, result, err, NULL, 0);
        break;

    case SH_SYS_WRITE:
        if (be->write && call.parm_count >= 2 && call.data_count > 0) {
            result = be->write(ctx, call.parms[0],
                               call.data[0].ptr, call.data[0].size);
            if (result < 0 && be->get_errno) {
                err = be->get_errno(ctx);
            }
        } else {
            result = -1;
            err = ENOSYS;
        }
        write_retn(state, riff_addr, result, err, NULL, 0);
        break;

    case SH_SYS_READ:
        if (be->read && call.parm_count >= 2) {
            size_t count = (size_t)call.parms[1];
            uint8_t *read_buf = state->work_buf + state->work_buf_size / 2;
            size_t max_read = state->work_buf_size / 2;

            if (count > max_read) {
                count = max_read;
            }

            result = be->read(ctx, call.parms[0], read_buf, count);
            if (result < 0 && be->get_errno) {
                err = be->get_errno(ctx);
                write_retn(state, riff_addr, result, err, NULL, 0);
            } else {
                /* result = bytes NOT read */
                size_t bytes_read = count - (size_t)result;
                write_retn(state, riff_addr, result, 0, read_buf, bytes_read);
            }
        } else {
            write_retn(state, riff_addr, -1, ENOSYS, NULL, 0);
        }
        break;

    case SH_SYS_SEEK:
        if (be->seek && call.parm_count >= 2) {
            result = be->seek(ctx, call.parms[0], call.parms[1]);
            if (result < 0 && be->get_errno) {
                err = be->get_errno(ctx);
            }
        } else {
            result = -1;
            err = ENOSYS;
        }
        write_retn(state, riff_addr, result, err, NULL, 0);
        break;

    case SH_SYS_FLEN:
        if (be->flen && call.parm_count >= 1) {
            result = be->flen(ctx, call.parms[0]);
            if (result < 0 && be->get_errno) {
                err = be->get_errno(ctx);
            }
        } else {
            result = -1;
            err = ENOSYS;
        }
        write_retn(state, riff_addr, result, err, NULL, 0);
        break;

    case SH_SYS_ISTTY:
        if (be->istty && call.parm_count >= 1) {
            result = be->istty(ctx, call.parms[0]);
        } else {
            result = 0;
        }
        write_retn(state, riff_addr, result, 0, NULL, 0);
        break;

    case SH_SYS_REMOVE:
        if (be->remove && call.data_count > 0) {
            result = be->remove(ctx, (const char *)call.data[0].ptr,
                                call.data[0].size);
            if (result < 0 && be->get_errno) {
                err = be->get_errno(ctx);
            }
        } else {
            result = -1;
            err = ENOSYS;
        }
        write_retn(state, riff_addr, result, err, NULL, 0);
        break;

    case SH_SYS_RENAME:
        if (be->rename && call.data_count >= 2) {
            result = be->rename(ctx,
                                (const char *)call.data[0].ptr, call.data[0].size,
                                (const char *)call.data[1].ptr, call.data[1].size);
            if (result < 0 && be->get_errno) {
                err = be->get_errno(ctx);
            }
        } else {
            result = -1;
            err = ENOSYS;
        }
        write_retn(state, riff_addr, result, err, NULL, 0);
        break;

    case SH_SYS_TMPNAM:
        if (be->tmpnam && call.parm_count >= 2) {
            size_t maxlen = (size_t)call.parms[1];
            char *tmp_buf = (char *)(state->work_buf + state->work_buf_size / 2);
            size_t max_tmp = state->work_buf_size / 2;

            if (maxlen > max_tmp) {
                maxlen = max_tmp;
            }

            result = be->tmpnam(ctx, tmp_buf, maxlen, call.parms[0]);
            if (result == 0) {
                size_t len = zbc_strlen(tmp_buf) + 1;
                write_retn(state, riff_addr, 0, 0, tmp_buf, len);
            } else {
                if (be->get_errno) {
                    err = be->get_errno(ctx);
                }
                write_retn(state, riff_addr, -1, err, NULL, 0);
            }
        } else {
            write_retn(state, riff_addr, -1, ENOSYS, NULL, 0);
        }
        break;

    /* Console operations */

    case SH_SYS_WRITEC:
        if (be->writec && call.data_count > 0 && call.data[0].size > 0) {
            be->writec(ctx, call.data[0].ptr[0]);
        }
        write_retn(state, riff_addr, 0, 0, NULL, 0);
        break;

    case SH_SYS_WRITE0:
        if (be->write0 && call.data_count > 0) {
            /* Ensure null-terminated */
            if (call.data[0].size > 0) {
                call.data[0].ptr[call.data[0].size - 1] = '\0';
            }
            be->write0(ctx, (const char *)call.data[0].ptr);
        }
        write_retn(state, riff_addr, 0, 0, NULL, 0);
        break;

    case SH_SYS_READC:
        result = be->readc ? be->readc(ctx) : -1;
        write_retn(state, riff_addr, result, 0, NULL, 0);
        break;

    /* System operations */

    case SH_SYS_ISERROR:
        if (call.parm_count >= 1) {
            result = (call.parms[0] < 0) ? 1 : 0;
        }
        write_retn(state, riff_addr, result, 0, NULL, 0);
        break;

    case SH_SYS_CLOCK:
        result = be->clock ? be->clock(ctx) : -1;
        write_retn(state, riff_addr, result, 0, NULL, 0);
        break;

    case SH_SYS_TIME:
        result = be->time ? be->time(ctx) : -1;
        write_retn(state, riff_addr, result, 0, NULL, 0);
        break;

    case SH_SYS_TICKFREQ:
        result = be->tickfreq ? be->tickfreq(ctx) : -1;
        write_retn(state, riff_addr, result, 0, NULL, 0);
        break;

    case SH_SYS_ERRNO:
        result = be->get_errno ? be->get_errno(ctx) : 0;
        write_retn(state, riff_addr, result, 0, NULL, 0);
        break;

    case SH_SYS_SYSTEM:
        if (be->do_system && call.data_count > 0) {
            result = be->do_system(ctx, (const char *)call.data[0].ptr,
                                   call.data[0].size);
        } else {
            result = -1;
        }
        write_retn(state, riff_addr, result, 0, NULL, 0);
        break;

    case SH_SYS_GET_CMDLINE:
        /* Not commonly used, return empty */
        write_retn(state, riff_addr, -1, ENOSYS, NULL, 0);
        break;

    case SH_SYS_HEAPINFO:
        if (be->heapinfo) {
            unsigned int heap_base, heap_limit, stack_base, stack_limit;
            result = be->heapinfo(ctx, &heap_base, &heap_limit,
                                  &stack_base, &stack_limit);
            /* TODO: Write 4 PARM chunks with pointer values */
        }
        write_retn(state, riff_addr, -1, ENOSYS, NULL, 0);
        break;

    case SH_SYS_EXIT:
    case SH_SYS_EXIT_EXTENDED:
        if (be->do_exit && call.parm_count >= 1) {
            unsigned int subcode = (call.parm_count >= 2) ?
                                   (unsigned int)call.parms[1] : 0;
            be->do_exit(ctx, (unsigned int)call.parms[0], subcode);
        }
        write_retn(state, riff_addr, 0, 0, NULL, 0);
        break;

    case SH_SYS_ELAPSED:
        if (be->elapsed) {
            unsigned int lo, hi;
            result = be->elapsed(ctx, &lo, &hi);
            if (result == 0) {
                uint8_t tick_data[8];
                ZBC_WRITE_U32_LE(tick_data, lo);
                ZBC_WRITE_U32_LE(tick_data + 4, hi);
                write_retn(state, riff_addr, 0, 0, tick_data, 8);
                break;
            }
        }
        write_retn(state, riff_addr, -1, ENOSYS, NULL, 0);
        break;

    default:
        write_erro(state, riff_addr, ZBC_PROTO_ERR_UNSUPPORTED_OP);
        break;
    }

    return ZBC_OK;
}
