/*
 * ZBC Semihosting Client Library
 *
 * Table-driven implementation using opcode metadata.
 * Uses struct-based RIFF chunk access (no magic offsets).
 */

#include "zbc_semihost.h"

/*========================================================================
 * Initialization
 *========================================================================*/

void zbc_client_init(zbc_client_state_t *state, volatile void *dev_base)
{
    if (!state) {
        return;
    }

    state->dev_base = (volatile uint8_t *)dev_base;
    state->cnfg_sent = 0;
    state->int_size = (uint8_t)ZBC_CLIENT_INT_SIZE;
    state->ptr_size = (uint8_t)ZBC_CLIENT_PTR_SIZE;
    state->endianness = (uint8_t)ZBC_CLIENT_ENDIANNESS;
    state->doorbell_callback = (void (*)(void *))0;
    state->doorbell_ctx = (void *)0;
}

int zbc_client_check_signature(const zbc_client_state_t *state)
{
    if (!state || !state->dev_base) {
        return ZBC_ERR_NULL_ARG;
    }

    if (state->dev_base[ZBC_REG_SIGNATURE + 0] == ZBC_SIGNATURE_BYTE0 &&
        state->dev_base[ZBC_REG_SIGNATURE + 1] == ZBC_SIGNATURE_BYTE1 &&
        state->dev_base[ZBC_REG_SIGNATURE + 2] == ZBC_SIGNATURE_BYTE2 &&
        state->dev_base[ZBC_REG_SIGNATURE + 3] == ZBC_SIGNATURE_BYTE3 &&
        state->dev_base[ZBC_REG_SIGNATURE + 4] == ZBC_SIGNATURE_BYTE4 &&
        state->dev_base[ZBC_REG_SIGNATURE + 5] == ZBC_SIGNATURE_BYTE5 &&
        state->dev_base[ZBC_REG_SIGNATURE + 6] == ZBC_SIGNATURE_BYTE6 &&
        state->dev_base[ZBC_REG_SIGNATURE + 7] == ZBC_SIGNATURE_BYTE7) {
        return ZBC_OK;
    }
    return ZBC_ERR_DEVICE_ERROR;
}

int zbc_client_device_present(const zbc_client_state_t *state)
{
    uint8_t status;

    if (!state || !state->dev_base) {
        return ZBC_ERR_NULL_ARG;
    }

    status = state->dev_base[ZBC_REG_STATUS];
    if (status & ZBC_STATUS_DEVICE_PRESENT) {
        return ZBC_OK;
    }
    return ZBC_ERR_DEVICE_ERROR;
}

void zbc_client_reset_cnfg(zbc_client_state_t *state)
{
    if (state) {
        state->cnfg_sent = 0;
    }
}

/*========================================================================
 * RETN capacity calculation
 *========================================================================*/

/*
 * Calculate the required RETN payload capacity for a given opcode.
 *
 * RETN payload layout:
 *   result[int_size] + errno[ZBC_RETN_ERRNO_SIZE] + optional DATA sub-chunk
 *
 * The DATA sub-chunk (if needed) is:
 *   chunk header (ZBC_CHUNK_HDR_SIZE) + type/reserved (ZBC_DATA_HDR_SIZE) + payload
 */
static size_t calculate_retn_capacity(const zbc_opcode_entry_t *entry,
                                      const zbc_client_state_t *state,
                                      uintptr_t *args)
{
    size_t base_size;
    size_t data_len;

    /* Base: result[int_size] + errno[ZBC_RETN_ERRNO_SIZE] */
    base_size = state->int_size + ZBC_RETN_ERRNO_SIZE;

    switch (entry->resp_type) {
    case ZBC_RESP_INT:
        /* No DATA sub-chunk needed */
        return base_size;

    case ZBC_RESP_DATA:
        /* DATA sub-chunk for read operations; length from args[resp_len_slot] */
        data_len = args ? (size_t)args[entry->resp_len_slot] : 256;
        return base_size + ZBC_CHUNK_HDR_SIZE + ZBC_DATA_HDR_SIZE +
               ZBC_PAD_SIZE(data_len);

    case ZBC_RESP_HEAPINFO:
        /* 4 pointers worth of data */
        data_len = 4 * state->ptr_size;
        return base_size + ZBC_CHUNK_HDR_SIZE + ZBC_DATA_HDR_SIZE +
               ZBC_PAD_SIZE(data_len);

    case ZBC_RESP_ELAPSED:
        /* Spec says 8 bytes for 64-bit tick count (line 1027-1046) */
        return base_size + ZBC_CHUNK_HDR_SIZE + ZBC_DATA_HDR_SIZE +
               ZBC_PAD_SIZE(8);

    default:
        return base_size;
    }
}

/*========================================================================
 * Request building (struct-based, no magic offsets)
 *========================================================================*/

/*
 * Write CNFG chunk if not yet sent.
 */
static int write_cnfg_if_needed(uint8_t *buf, size_t capacity, size_t *pos,
                                zbc_client_state_t *state)
{
    size_t total_size;
    uint8_t *payload;

    if (state->cnfg_sent) {
        return ZBC_OK;
    }

    total_size = ZBC_CHUNK_HDR_SIZE + ZBC_CNFG_PAYLOAD_SIZE;
    if (*pos + total_size > capacity) {
        ZBC_LOG_ERROR("write_cnfg: buffer full (need %u, have %u)",
                 (unsigned)(*pos + total_size), (unsigned)capacity);
        return ZBC_ERR_BUFFER_FULL;
    }

    /* Write chunk header (alignment-safe) */
    ZBC_CHUNK_WRITE_HDR(buf + *pos, ZBC_ID_CNFG, ZBC_CNFG_PAYLOAD_SIZE);

    /* Write CNFG payload (all single bytes, no alignment issues) */
    payload = buf + *pos + ZBC_CHUNK_HDR_SIZE;
    payload[0] = state->int_size;
    payload[1] = state->ptr_size;
    payload[2] = state->endianness;
    payload[3] = 0;  /* reserved */

    *pos += total_size;
    state->cnfg_sent = 1;

    return ZBC_OK;
}

/*
 * Write a PARM chunk with an integer value.
 */
static int write_parm_chunk(uint8_t *buf, size_t capacity, size_t *pos,
                            uintptr_t value, int is_signed,
                            const zbc_client_state_t *state)
{
    size_t payload_size;
    size_t total_size;
    uint8_t *payload;

    /* Payload = type(1) + reserved(3) + value(int_size) */
    payload_size = ZBC_PARM_HDR_SIZE + state->int_size;
    total_size = ZBC_CHUNK_HDR_SIZE + ZBC_PAD_SIZE(payload_size);

    if (*pos + total_size > capacity) {
        return ZBC_ERR_BUFFER_FULL;
    }

    /* Write chunk header (alignment-safe) */
    ZBC_CHUNK_WRITE_HDR(buf + *pos, ZBC_ID_PARM, (uint32_t)payload_size);

    /* Write PARM payload (single bytes + zbc_write_native_uint handles value) */
    payload = buf + *pos + ZBC_CHUNK_HDR_SIZE;
    payload[0] = ZBC_PARM_TYPE_INT;
    payload[1] = 0;  /* reserved */
    payload[2] = 0;
    payload[3] = 0;
    zbc_write_native_uint(payload + 4, (uint64_t)value, state->int_size,
                          state->endianness);

    *pos += total_size;

    (void)is_signed;  /* Currently unused, same encoding */
    return ZBC_OK;
}

/*
 * Write a DATA chunk with binary content.
 */
static int write_data_chunk(uint8_t *buf, size_t capacity, size_t *pos,
                            const void *data, size_t len, int data_type)
{
    size_t payload_size;
    size_t total_size;
    size_t i;
    const uint8_t *src;
    uint8_t *payload;

    /* Payload = type(1) + reserved(3) + data(len) */
    payload_size = ZBC_DATA_HDR_SIZE + len;
    total_size = ZBC_CHUNK_HDR_SIZE + ZBC_PAD_SIZE(payload_size);

    if (*pos + total_size > capacity) {
        return ZBC_ERR_BUFFER_FULL;
    }

    /* Write chunk header (alignment-safe) */
    ZBC_CHUNK_WRITE_HDR(buf + *pos, ZBC_ID_DATA, (uint32_t)payload_size);

    /* Write DATA payload header (single bytes, no alignment issues) */
    payload = buf + *pos + ZBC_CHUNK_HDR_SIZE;
    payload[0] = (uint8_t)data_type;
    payload[1] = 0;  /* reserved */
    payload[2] = 0;
    payload[3] = 0;

    /* Copy data */
    src = (const uint8_t *)data;
    for (i = 0; i < len; i++) {
        payload[4 + i] = src[i];
    }

    /* Add padding byte if odd */
    if (payload_size & 1) {
        payload[payload_size] = 0;
    }

    *pos += total_size;
    return ZBC_OK;
}

/*
 * Build request from opcode table entry and args array.
 */
static int build_request(uint8_t *buf, size_t capacity, size_t *out_size,
                         zbc_client_state_t *state,
                         const zbc_opcode_entry_t *entry,
                         uintptr_t *args)
{
    size_t pos;
    size_t call_chunk_pos;
    size_t call_data_start;
    uint8_t *call_hdr;
    int i;
    int rc;

    /* Minimum size for RIFF header */
    if (capacity < ZBC_RIFF_HDR_SIZE) {
        return ZBC_ERR_BUFFER_FULL;
    }

    /* Write RIFF header (size patched later) */
    ZBC_RIFF_WRITE_HDR(buf, 0, ZBC_ID_SEMI);
    pos = ZBC_RIFF_HDR_SIZE;

    /* CNFG chunk if needed */
    rc = write_cnfg_if_needed(buf, capacity, &pos, state);
    if (rc != ZBC_OK) {
        return rc;
    }

    /* CALL chunk - write header, then sub-chunks */
    if (pos + ZBC_CHUNK_HDR_SIZE + ZBC_CALL_HDR_PAYLOAD_SIZE > capacity) {
        return ZBC_ERR_BUFFER_FULL;
    }

    call_chunk_pos = pos;
    ZBC_CHUNK_WRITE_HDR(buf + pos, ZBC_ID_CALL, 0);  /* size patched later */
    pos += ZBC_CHUNK_HDR_SIZE;
    call_data_start = pos;

    /* CALL header (opcode) - all single bytes */
    call_hdr = buf + pos;
    call_hdr[0] = entry->opcode;
    call_hdr[1] = 0;  /* reserved */
    call_hdr[2] = 0;
    call_hdr[3] = 0;
    pos += ZBC_CALL_HDR_PAYLOAD_SIZE;

    /* Emit sub-chunks based on table */
    for (i = 0; i < 4; i++) {
        const zbc_chunk_desc_t *desc = &entry->params[i];

        if (desc->type == ZBC_CHUNK_NONE) {
            break;
        }

        switch (desc->type) {
        case ZBC_CHUNK_PARM_INT:
            rc = write_parm_chunk(buf, capacity, &pos,
                                  args[desc->slot], 1, state);
            break;

        case ZBC_CHUNK_PARM_UINT:
            rc = write_parm_chunk(buf, capacity, &pos,
                                  args[desc->slot], 0, state);
            break;

        case ZBC_CHUNK_DATA_PTR:
            rc = write_data_chunk(buf, capacity, &pos,
                                  (void *)args[desc->slot],
                                  (size_t)args[desc->len_slot],
                                  ZBC_DATA_TYPE_BINARY);
            break;

        case ZBC_CHUNK_DATA_STR:
            {
                const char *str = (const char *)args[desc->slot];
                size_t len = zbc_strlen(str) + 1;  /* Include null */
                rc = write_data_chunk(buf, capacity, &pos, str, len,
                                      ZBC_DATA_TYPE_STRING);
            }
            break;

        case ZBC_CHUNK_DATA_BYTE:
            {
                uint8_t byte = *(uint8_t *)args[desc->slot];
                rc = write_data_chunk(buf, capacity, &pos, &byte, 1,
                                      ZBC_DATA_TYPE_BINARY);
            }
            break;

        default:
            rc = ZBC_ERR_UNKNOWN_OPCODE;
            break;
        }

        if (rc != ZBC_OK) {
            return rc;
        }
    }

    /* Patch CALL chunk size (alignment-safe) */
    ZBC_PATCH_U32(buf + call_chunk_pos + 4, (uint32_t)(pos - call_data_start));

    /*
     * Write pre-allocated RETN chunk.
     * The host will fill this with: result[int_size] + errno[ZBC_RETN_ERRNO_SIZE]
     * + optional DATA sub-chunk.
     */
    {
        size_t retn_capacity = calculate_retn_capacity(entry, state, args);
        size_t retn_total = ZBC_CHUNK_HDR_SIZE + ZBC_PAD_SIZE(retn_capacity);
        size_t j;

        if (pos + retn_total > capacity) {
            ZBC_LOG_ERROR("build_request: no space for RETN (need %u, have %u)",
                     (unsigned)(pos + retn_total), (unsigned)capacity);
            return ZBC_ERR_BUFFER_FULL;
        }

        ZBC_CHUNK_WRITE_HDR(buf + pos, ZBC_ID_RETN, (uint32_t)retn_capacity);
        /* Zero-fill the RETN payload area */
        for (j = 0; j < ZBC_PAD_SIZE(retn_capacity); j++) {
            buf[pos + ZBC_CHUNK_HDR_SIZE + j] = 0;
        }
        pos += retn_total;
    }

    /*
     * Write pre-allocated ERRO chunk.
     * The host fills this only on protocol errors.
     */
    {
        size_t erro_total = ZBC_CHUNK_HDR_SIZE + ZBC_ERRO_PREALLOC_SIZE;
        size_t j;

        if (pos + erro_total > capacity) {
            ZBC_LOG_ERROR("build_request: no space for ERRO (need %u, have %u)",
                     (unsigned)(pos + erro_total), (unsigned)capacity);
            return ZBC_ERR_BUFFER_FULL;
        }

        ZBC_CHUNK_WRITE_HDR(buf + pos, ZBC_ID_ERRO, ZBC_ERRO_PREALLOC_SIZE);
        /* Zero-fill the ERRO payload area */
        for (j = 0; j < ZBC_ERRO_PREALLOC_SIZE; j++) {
            buf[pos + ZBC_CHUNK_HDR_SIZE + j] = 0;
        }
        pos += erro_total;
    }

    /* Patch RIFF size: everything after the size field = form_type + all chunks */
    ZBC_PATCH_U32(buf + 4, (uint32_t)(pos - ZBC_RIFF_HDR_SIZE + 4));

    *out_size = pos;
    return ZBC_OK;
}

/*========================================================================
 * Device communication
 *========================================================================*/

int zbc_client_submit_poll(zbc_client_state_t *state, void *buf, size_t size)
{
    volatile uint8_t *dev;
    uintptr_t addr;
    int i;
    uint8_t status;

    (void)size;

    if (!state || !state->dev_base || !buf) {
        return ZBC_ERR_INVALID_ARG;
    }

    dev = state->dev_base;
    addr = (uintptr_t)buf;

    /* Write buffer address to RIFF_PTR register (native byte order) */
    for (i = 0; i < ZBC_CLIENT_PTR_SIZE && i < 16; i++) {
#if ZBC_CLIENT_ENDIANNESS == ZBC_ENDIAN_LITTLE
        dev[ZBC_REG_RIFF_PTR + i] = (uint8_t)(addr & 0xFFU);
        addr >>= 8;
#else
        dev[ZBC_REG_RIFF_PTR + ZBC_CLIENT_PTR_SIZE - 1 - i] =
            (uint8_t)(addr & 0xFFU);
        addr >>= 8;
#endif
    }

    /* Clear remaining bytes */
    for (; i < 16; i++) {
        dev[ZBC_REG_RIFF_PTR + i] = 0;
    }

    /* Memory barrier */
#if defined(__GNUC__) || defined(__clang__)
    __asm__ volatile("" ::: "memory");
#endif

    /* Trigger request */
    dev[ZBC_REG_DOORBELL] = 0x01;

    /* Testing callback */
    if (state->doorbell_callback) {
        state->doorbell_callback(state->doorbell_ctx);
    }

#if defined(__GNUC__) || defined(__clang__)
    __asm__ volatile("" ::: "memory");
#endif

    /* Poll for response */
    do {
        status = dev[ZBC_REG_STATUS];
    } while (!(status & ZBC_STATUS_RESPONSE_READY));

    return ZBC_OK;
}

/*========================================================================
 * Response parsing
 *
 * Client-specific parser that only extracts RETN and ERRO chunks.
 * This is much smaller than zbc_riff_parse() which also parses
 * CNFG/CALL/PARM (host-only).
 *========================================================================*/

int zbc_parse_response(zbc_response_t *response, const uint8_t *buf,
                       size_t capacity, const zbc_client_state_t *state)
{
    const zbc_riff_t *riff;
    const uint8_t *riff_end;
    const uint8_t *pos;
    uint32_t id, size;
    const uint8_t *chunk_data;
    int int_size;
    int endian;
    int found_retn = 0;
    int found_erro = 0;
    int rc;

    if (!response || !buf || !state) {
        return ZBC_ERR_INVALID_ARG;
    }

    /* Initialize response */
    response->result = 0;
    response->error_code = 0;
    response->data = (const uint8_t *)0;
    response->data_size = 0;
    response->is_error = 0;
    response->proto_error = 0;

    int_size = state->int_size;
    endian = state->endianness;

    /* Validate RIFF container */
    riff = (const zbc_riff_t *)buf;
    rc = zbc_riff_validate(riff, capacity, ZBC_ID_SEMI);
    if (rc != ZBC_OK) {
        return rc;
    }

    rc = zbc_riff_end(&riff_end, riff);
    if (rc != ZBC_OK) {
        return rc;
    }

    /* Walk top-level chunks looking for RETN and ERRO only */
    pos = buf + ZBC_RIFF_HDR_SIZE;

    while (pos + ZBC_CHUNK_HDR_SIZE <= riff_end) {
        id = ZBC_READ_U32_LE(pos);
        size = ZBC_READ_U32_LE(pos + 4);
        chunk_data = pos + ZBC_CHUNK_HDR_SIZE;

        /* Bounds check */
        if (chunk_data + size > riff_end) {
            return ZBC_ERR_DATA_OVERFLOW;
        }

        if (id == ZBC_ID_RETN) {
            found_retn = 1;
            /* RETN: result[int_size] + errno[4] + optional DATA sub-chunk */
            if (size >= (size_t)int_size + ZBC_RETN_ERRNO_SIZE) {
                response->result = zbc_read_native_int(chunk_data, int_size, endian);
                response->error_code = (int)ZBC_READ_U32_LE(chunk_data + int_size);

                /* Check for DATA sub-chunk within RETN */
                {
                    const uint8_t *sub_pos = chunk_data + int_size + ZBC_RETN_ERRNO_SIZE;
                    const uint8_t *sub_end = chunk_data + size;

                    while (sub_pos + ZBC_CHUNK_HDR_SIZE <= sub_end) {
                        uint32_t sub_id = ZBC_READ_U32_LE(sub_pos);
                        uint32_t sub_size = ZBC_READ_U32_LE(sub_pos + 4);
                        const uint8_t *sub_data = sub_pos + ZBC_CHUNK_HDR_SIZE;

                        if (sub_data + sub_size > sub_end) {
                            break;
                        }

                        if (sub_id == ZBC_ID_DATA && sub_size >= ZBC_DATA_HDR_SIZE) {
                            response->data = sub_data + ZBC_DATA_HDR_SIZE;
                            response->data_size = sub_size - ZBC_DATA_HDR_SIZE;
                            break;  /* Only need first DATA chunk */
                        }

                        sub_pos += ZBC_CHUNK_HDR_SIZE + ZBC_PAD_SIZE(sub_size);
                    }
                }
            }
        } else if (id == ZBC_ID_ERRO) {
            found_erro = 1;
            /* ERRO: error_code[2] + reserved[2] */
            if (size >= ZBC_ERRO_PAYLOAD_SIZE) {
                response->proto_error = ZBC_READ_U16_LE(chunk_data);
            }
        }
        /* Skip CNFG, CALL, and any other chunks - client doesn't need them */

        /* Advance to next chunk (with padding) */
        pos += ZBC_CHUNK_HDR_SIZE + ZBC_PAD_SIZE(size);
    }

    /* Check for actual error first (non-zero proto_error) */
    if (found_erro && response->proto_error != 0) {
        response->is_error = 1;
        return ZBC_OK;
    }

    /* Check for RETN (normal response) */
    if (found_retn) {
        return ZBC_OK;
    }

    /* No RETN found and no actual error - this shouldn't happen */
    return ZBC_ERR_PARSE_ERROR;
}

/*========================================================================
 * Main entry points
 *========================================================================*/

int zbc_call(zbc_response_t *response, zbc_client_state_t *state,
             void *buf, size_t buf_size, int opcode, uintptr_t *args)
{
    const zbc_opcode_entry_t *entry;
    size_t riff_size;
    int rc;

    if (!response || !state || !buf) {
        return ZBC_ERR_INVALID_ARG;
    }

    entry = zbc_opcode_lookup(opcode);
    if (!entry) {
        ZBC_LOG_WARN("unknown opcode 0x%02x", (unsigned)opcode);
        return ZBC_ERR_UNKNOWN_OPCODE;
    }

    /* args may be NULL for operations with no parameters */

    /* Build request */
    rc = build_request((uint8_t *)buf, buf_size, &riff_size, state, entry, args);
    if (rc != ZBC_OK) {
        return rc;
    }

    /* Submit and wait */
    rc = zbc_client_submit_poll(state, buf, riff_size);
    if (rc != ZBC_OK) {
        return rc;
    }

    /* Parse response */
    rc = zbc_parse_response(response, (uint8_t *)buf, buf_size, state);
    if (rc != ZBC_OK) {
        ZBC_LOG_ERROR("zbc_call: response parse failed (%d)", rc);
        return rc;
    }

    if (response->is_error) {
        ZBC_LOG_WARN("zbc_call: protocol error %u", (unsigned)response->proto_error);
        return ZBC_ERR_DEVICE_ERROR;
    }

    /*
     * Copy response DATA to destination buffer for ZBC_RESP_DATA operations.
     * The opcode table specifies:
     *   resp_dest     - args[] index of destination pointer
     *   resp_len_slot - args[] index of max length
     */
    if (entry->resp_type == ZBC_RESP_DATA && args && response->data && response->data_size > 0) {
        uint8_t *dest = (uint8_t *)args[entry->resp_dest];
        size_t max_len = (size_t)args[entry->resp_len_slot];
        size_t copy_len = response->data_size;
        size_t i;

        if (copy_len > max_len) {
            copy_len = max_len;
        }

        for (i = 0; i < copy_len; i++) {
            dest[i] = response->data[i];
        }
    }

    return ZBC_OK;
}

uintptr_t zbc_semihost(zbc_client_state_t *state, uint8_t *riff_buf,
                       size_t riff_buf_size, uintptr_t op, uintptr_t param)
{
    zbc_response_t response;
    uintptr_t *args = (uintptr_t *)param;
    int rc;

    rc = zbc_call(&response, state, riff_buf, riff_buf_size, (int)op, args);
    if (rc != ZBC_OK) {
        return (uintptr_t)-1;
    }

    return (uintptr_t)response.result;
}
