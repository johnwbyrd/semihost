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

int64_t zbc_host_read_guest_int(const zbc_host_state_t *state,
                                const uint8_t *data, size_t size)
{
    return zbc_read_native_int(data, (int)size, state->guest_endianness);
}

void zbc_host_write_guest_int(const zbc_host_state_t *state,
                              uint8_t *data, uint64_t value, size_t size)
{
    zbc_write_native_uint(data, value, (int)size, state->guest_endianness);
}

/*========================================================================
 * Response building
 *
 * The client pre-allocates RETN and ERRO chunks. The host writes only
 * the payload contents within those pre-allocated bounds. The RIFF
 * structure is never modified by the host.
 *========================================================================*/

/*
 * Write ERRO payload to pre-allocated ERRO chunk.
 * Only writes the payload (error_code + reserved), not the chunk header.
 */
static void write_erro_payload(zbc_host_state_t *state, uint64_t riff_addr,
                               const zbc_parsed_t *parsed, int error_code)
{
    uint8_t buf[ZBC_ERRO_PAYLOAD_SIZE];

    if (!parsed->has_erro || parsed->erro_payload_capacity < ZBC_ERRO_PAYLOAD_SIZE) {
        ZBC_LOG_ERROR_S("write_erro_payload: no pre-allocated ERRO chunk");
        return;
    }

    /* ERRO payload: error_code(2) + reserved(2) */
    ZBC_WRITE_U16_LE(buf, (uint16_t)error_code);
    buf[2] = 0;
    buf[3] = 0;

    write_guest(state, riff_addr + parsed->erro_payload_offset, buf, ZBC_ERRO_PAYLOAD_SIZE);
}

/*
 * Write RETN payload to pre-allocated RETN chunk.
 * Only writes the payload contents, not the chunk header.
 */
static void write_retn_payload(zbc_host_state_t *state, uint64_t riff_addr,
                               const zbc_parsed_t *parsed,
                               int64_t result, int err,
                               const void *data, size_t data_size)
{
    uint8_t buf[256];
    size_t pos;
    int int_size;
    size_t i;
    const uint8_t *src;

    if (!parsed->has_retn) {
        ZBC_LOG_ERROR_S("write_retn_payload: no pre-allocated RETN chunk");
        return;
    }

    int_size = state->guest_int_size;
    pos = 0;

    /* RETN payload: result[int_size] + errno[ZBC_RETN_ERRNO_SIZE] */
    zbc_host_write_guest_int(state, buf + pos, (uint64_t)result, int_size);
    pos += (size_t)int_size;
    ZBC_WRITE_U32_LE(buf + pos, (uint32_t)err);
    pos += ZBC_RETN_ERRNO_SIZE;

    /* Add DATA sub-chunk if present */
    if (data && data_size > 0) {
        size_t data_payload_size = ZBC_DATA_HDR_SIZE + data_size;
        size_t padded_size = ZBC_PAD_SIZE(data_payload_size);

        /* DATA chunk header */
        ZBC_WRITE_U32_LE(buf + pos, ZBC_ID_DATA);
        pos += 4;
        ZBC_WRITE_U32_LE(buf + pos, (uint32_t)data_payload_size);
        pos += 4;

        /* DATA payload: type + reserved + data */
        buf[pos] = ZBC_DATA_TYPE_BINARY;
        buf[pos + 1] = 0;
        buf[pos + 2] = 0;
        buf[pos + 3] = 0;
        pos += ZBC_DATA_HDR_SIZE;

        src = (const uint8_t *)data;
        for (i = 0; i < data_size; i++) {
            buf[pos + i] = src[i];
        }
        pos += data_size;

        /* Pad to even boundary if needed */
        if (padded_size > data_payload_size) {
            buf[pos] = 0;
            pos++;
        }
    }

    /* Check capacity before writing */
    if (pos > parsed->retn_payload_capacity) {
        ZBC_LOG_ERROR("write_retn_payload: response %u exceeds capacity %u",
                     (unsigned)pos, (unsigned)parsed->retn_payload_capacity);
        return;
    }

    write_guest(state, riff_addr + parsed->retn_payload_offset, buf, pos);
}

/*
 * Write ERRO chunk for early errors (before parsing completes).
 * This is the fallback when we can't use the pre-allocated chunk.
 * It overwrites at the first chunk position - not ideal but necessary
 * for protocol errors that prevent parsing the RIFF structure.
 */
static void write_erro_early(zbc_host_state_t *state, uint64_t addr, int error_code)
{
    uint8_t buf[ZBC_CHUNK_HDR_SIZE + ZBC_ERRO_PAYLOAD_SIZE];

    /* Build ERRO chunk: id + size + payload */
    ZBC_WRITE_U32_LE(buf, ZBC_ID_ERRO);
    ZBC_WRITE_U32_LE(buf + 4, ZBC_ERRO_PAYLOAD_SIZE);
    ZBC_WRITE_U16_LE(buf + 8, (uint16_t)error_code);
    buf[10] = 0;
    buf[11] = 0;

    /* Write at fixed offset - this is a fallback for parse failures */
    write_guest(state, addr + ZBC_RIFF_HDR_SIZE, buf, sizeof(buf));
}

/*========================================================================
 * Request parsing
 *
 * Uses the unified zbc_riff_parse() to extract all fields at once.
 *========================================================================*/

static int parse_request(zbc_host_state_t *state, uint64_t riff_addr,
                         zbc_parsed_t *parsed)
{
    uint8_t *buf = state->work_buf;
    size_t capacity = state->work_buf_size;
    size_t riff_total_size;
    uint32_t riff_size;
    int rc;

    /* Read RIFF header first to get size */
    read_guest(state, buf, riff_addr, ZBC_RIFF_HDR_SIZE);

    /* Check magic and get size */
    if (ZBC_READ_U32_LE(buf) != ZBC_ID_RIFF) {
        ZBC_LOG_ERROR_S("parse_request: bad RIFF magic");
        write_erro_early(state, riff_addr, ZBC_PROTO_ERR_MALFORMED_RIFF);
        return ZBC_ERR_PARSE_ERROR;
    }

    riff_size = ZBC_READ_U32_LE(buf + 4);
    riff_total_size = 4 + 4 + riff_size;

    if (riff_total_size > capacity) {
        ZBC_LOG_ERROR("parse_request: RIFF size=%u exceeds work_buf=%u",
                 (unsigned)riff_total_size, (unsigned)capacity);
        return ZBC_ERR_BUFFER_FULL;
    }

    /* Read entire RIFF structure */
    read_guest(state, buf, riff_addr, riff_total_size);

    /* Parse everything at once */
    rc = zbc_riff_parse(parsed, buf, riff_total_size,
                        state->guest_int_size, state->guest_endianness);
    if (rc != ZBC_OK) {
        ZBC_LOG_ERROR("parse_request: zbc_riff_parse failed (%d)", rc);
        write_erro_early(state, riff_addr, ZBC_PROTO_ERR_MALFORMED_RIFF);
        return ZBC_ERR_PARSE_ERROR;
    }

    /* Update state from CNFG if present */
    if (parsed->has_cnfg) {
        state->guest_int_size = parsed->int_size;
        state->guest_ptr_size = parsed->ptr_size;
        state->guest_endianness = parsed->endianness;
        state->cnfg_received = 1;
        ZBC_LOG_INFO("CNFG: int_size=%u ptr_size=%u endian=%u",
                 (unsigned)parsed->int_size, (unsigned)parsed->ptr_size,
                 (unsigned)parsed->endianness);
    }

    if (!state->cnfg_received) {
        ZBC_LOG_ERROR_S("parse_request: missing CNFG chunk");
        /* At this point parsing succeeded, so we can try the pre-allocated ERRO */
        if (parsed->has_erro) {
            write_erro_payload(state, riff_addr, parsed, ZBC_PROTO_ERR_MISSING_CNFG);
        } else {
            write_erro_early(state, riff_addr, ZBC_PROTO_ERR_MISSING_CNFG);
        }
        return ZBC_ERR_PARSE_ERROR;
    }

    if (!parsed->has_call) {
        ZBC_LOG_ERROR_S("parse_request: missing CALL chunk");
        if (parsed->has_erro) {
            write_erro_payload(state, riff_addr, parsed, ZBC_PROTO_ERR_INVALID_CHUNK);
        } else {
            write_erro_early(state, riff_addr, ZBC_PROTO_ERR_INVALID_CHUNK);
        }
        return ZBC_ERR_PARSE_ERROR;
    }

    return ZBC_OK;
}

/*========================================================================
 * Backend dispatch
 *========================================================================*/

int zbc_host_process(zbc_host_state_t *state, uint64_t riff_addr)
{
    zbc_parsed_t parsed;
    const zbc_backend_t *be;
    void *ctx;
    int64_t result = 0;
    int err = 0;
    int rc;

    if (!state || !state->work_buf || !state->backend) {
        ZBC_LOG_ERROR_S("zbc_host_process: invalid arguments");
        return ZBC_ERR_INVALID_ARG;
    }

    rc = parse_request(state, riff_addr, &parsed);
    if (rc != ZBC_OK) {
        return rc;
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

    switch (parsed.opcode) {

    /* File operations */

    case SH_SYS_OPEN:
        if (be->open && parsed.data_count > 0 && parsed.parm_count >= 2) {
            result = be->open(ctx, (const char *)parsed.data[0].ptr,
                              parsed.data[0].size, parsed.parms[0]);
            if (result < 0 && be->get_errno) {
                err = be->get_errno(ctx);
            }
        } else {
            result = -1;
            err = ENOSYS;
        }
        write_retn_payload(state, riff_addr, &parsed, result, err, NULL, 0);
        break;

    case SH_SYS_CLOSE:
        if (be->close && parsed.parm_count >= 1) {
            result = be->close(ctx, parsed.parms[0]);
            if (result < 0 && be->get_errno) {
                err = be->get_errno(ctx);
            }
        } else {
            result = -1;
            err = ENOSYS;
        }
        write_retn_payload(state, riff_addr, &parsed, result, err, NULL, 0);
        break;

    case SH_SYS_WRITE:
        if (be->write && parsed.parm_count >= 2 && parsed.data_count > 0) {
            result = be->write(ctx, parsed.parms[0],
                               parsed.data[0].ptr, parsed.data[0].size);
            if (result < 0 && be->get_errno) {
                err = be->get_errno(ctx);
            }
        } else {
            result = -1;
            err = ENOSYS;
        }
        write_retn_payload(state, riff_addr, &parsed, result, err, NULL, 0);
        break;

    case SH_SYS_READ:
        if (be->read && parsed.parm_count >= 2) {
            size_t count = (size_t)parsed.parms[1];
            uint8_t *read_buf = state->work_buf + state->work_buf_size / 2;
            size_t max_read = state->work_buf_size / 2;

            if (count > max_read) {
                count = max_read;
            }

            result = be->read(ctx, parsed.parms[0], read_buf, count);
            if (result < 0 && be->get_errno) {
                err = be->get_errno(ctx);
                write_retn_payload(state, riff_addr, &parsed, result, err, NULL, 0);
            } else {
                /* result = bytes NOT read */
                size_t bytes_read = count - (size_t)result;
                write_retn_payload(state, riff_addr, &parsed, result, 0, read_buf, bytes_read);
            }
        } else {
            write_retn_payload(state, riff_addr, &parsed, -1, ENOSYS, NULL, 0);
        }
        break;

    case SH_SYS_SEEK:
        if (be->seek && parsed.parm_count >= 2) {
            result = be->seek(ctx, parsed.parms[0], parsed.parms[1]);
            if (result < 0 && be->get_errno) {
                err = be->get_errno(ctx);
            }
        } else {
            result = -1;
            err = ENOSYS;
        }
        write_retn_payload(state, riff_addr, &parsed, result, err, NULL, 0);
        break;

    case SH_SYS_FLEN:
        if (be->flen && parsed.parm_count >= 1) {
            result = be->flen(ctx, parsed.parms[0]);
            if (result < 0 && be->get_errno) {
                err = be->get_errno(ctx);
            }
        } else {
            result = -1;
            err = ENOSYS;
        }
        write_retn_payload(state, riff_addr, &parsed, result, err, NULL, 0);
        break;

    case SH_SYS_ISTTY:
        if (be->istty && parsed.parm_count >= 1) {
            result = be->istty(ctx, parsed.parms[0]);
        } else {
            result = 0;
        }
        write_retn_payload(state, riff_addr, &parsed, result, 0, NULL, 0);
        break;

    case SH_SYS_REMOVE:
        if (be->remove && parsed.data_count > 0) {
            result = be->remove(ctx, (const char *)parsed.data[0].ptr,
                                parsed.data[0].size);
            if (result < 0 && be->get_errno) {
                err = be->get_errno(ctx);
            }
        } else {
            result = -1;
            err = ENOSYS;
        }
        write_retn_payload(state, riff_addr, &parsed, result, err, NULL, 0);
        break;

    case SH_SYS_RENAME:
        if (be->rename && parsed.data_count >= 2) {
            result = be->rename(ctx,
                                (const char *)parsed.data[0].ptr, parsed.data[0].size,
                                (const char *)parsed.data[1].ptr, parsed.data[1].size);
            if (result < 0 && be->get_errno) {
                err = be->get_errno(ctx);
            }
        } else {
            result = -1;
            err = ENOSYS;
        }
        write_retn_payload(state, riff_addr, &parsed, result, err, NULL, 0);
        break;

    case SH_SYS_TMPNAM:
        if (be->tmpnam && parsed.parm_count >= 2) {
            size_t maxlen = (size_t)parsed.parms[1];
            char *tmp_buf = (char *)(state->work_buf + state->work_buf_size / 2);
            size_t max_tmp = state->work_buf_size / 2;

            if (maxlen > max_tmp) {
                maxlen = max_tmp;
            }

            result = be->tmpnam(ctx, tmp_buf, maxlen, parsed.parms[0]);
            if (result == 0) {
                size_t len = zbc_strlen(tmp_buf) + 1;
                write_retn_payload(state, riff_addr, &parsed, 0, 0, tmp_buf, len);
            } else {
                if (be->get_errno) {
                    err = be->get_errno(ctx);
                }
                write_retn_payload(state, riff_addr, &parsed, -1, err, NULL, 0);
            }
        } else {
            write_retn_payload(state, riff_addr, &parsed, -1, ENOSYS, NULL, 0);
        }
        break;

    /* Console operations */

    case SH_SYS_WRITEC:
        if (be->writec && parsed.data_count > 0 && parsed.data[0].size > 0) {
            be->writec(ctx, parsed.data[0].ptr[0]);
        }
        write_retn_payload(state, riff_addr, &parsed, 0, 0, NULL, 0);
        break;

    case SH_SYS_WRITE0:
        if (be->write0 && parsed.data_count > 0) {
            be->write0(ctx, (const char *)parsed.data[0].ptr);
        }
        write_retn_payload(state, riff_addr, &parsed, 0, 0, NULL, 0);
        break;

    case SH_SYS_READC:
        result = be->readc ? be->readc(ctx) : -1;
        write_retn_payload(state, riff_addr, &parsed, result, 0, NULL, 0);
        break;

    /* System operations */

    case SH_SYS_ISERROR:
        if (parsed.parm_count >= 1) {
            result = (parsed.parms[0] < 0) ? 1 : 0;
        }
        write_retn_payload(state, riff_addr, &parsed, result, 0, NULL, 0);
        break;

    case SH_SYS_CLOCK:
        result = be->clock ? be->clock(ctx) : -1;
        write_retn_payload(state, riff_addr, &parsed, result, 0, NULL, 0);
        break;

    case SH_SYS_TIME:
        result = be->time ? be->time(ctx) : -1;
        write_retn_payload(state, riff_addr, &parsed, result, 0, NULL, 0);
        break;

    case SH_SYS_TICKFREQ:
        result = be->tickfreq ? be->tickfreq(ctx) : -1;
        write_retn_payload(state, riff_addr, &parsed, result, 0, NULL, 0);
        break;

    case SH_SYS_ERRNO:
        result = be->get_errno ? be->get_errno(ctx) : 0;
        write_retn_payload(state, riff_addr, &parsed, result, 0, NULL, 0);
        break;

    case SH_SYS_SYSTEM:
        if (be->do_system && parsed.data_count > 0) {
            result = be->do_system(ctx, (const char *)parsed.data[0].ptr,
                                   parsed.data[0].size);
        } else {
            result = -1;
        }
        write_retn_payload(state, riff_addr, &parsed, result, 0, NULL, 0);
        break;

    case SH_SYS_GET_CMDLINE:
        /* Not commonly used, return empty */
        write_retn_payload(state, riff_addr, &parsed, -1, ENOSYS, NULL, 0);
        break;

    case SH_SYS_HEAPINFO:
        /* Heap info retrieval not yet fully implemented */
        write_retn_payload(state, riff_addr, &parsed, -1, ENOSYS, NULL, 0);
        break;

    case SH_SYS_EXIT:
    case SH_SYS_EXIT_EXTENDED:
        if (be->do_exit && parsed.parm_count >= 1) {
            unsigned int subcode = (parsed.parm_count >= 2) ?
                                   (unsigned int)parsed.parms[1] : 0;
            be->do_exit(ctx, (unsigned int)parsed.parms[0], subcode);
        }
        write_retn_payload(state, riff_addr, &parsed, 0, 0, NULL, 0);
        break;

    case SH_SYS_ELAPSED:
        if (be->elapsed) {
            unsigned int lo, hi;
            result = be->elapsed(ctx, &lo, &hi);
            if (result == 0) {
                uint8_t tick_data[8];
                ZBC_WRITE_U32_LE(tick_data, lo);
                ZBC_WRITE_U32_LE(tick_data + 4, hi);
                write_retn_payload(state, riff_addr, &parsed, 0, 0, tick_data, 8);
                break;
            }
        }
        write_retn_payload(state, riff_addr, &parsed, -1, ENOSYS, NULL, 0);
        break;

    default:
        ZBC_LOG_WARN("unknown opcode 0x%02x", (unsigned)parsed.opcode);
        write_erro_payload(state, riff_addr, &parsed, ZBC_PROTO_ERR_UNSUPPORTED_OP);
        break;
    }

    return ZBC_OK;
}
