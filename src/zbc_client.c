/*
 * ZBC Semihosting Client Library
 *
 * Table-driven implementation using opcode metadata.
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
        return 0;
    }

    return (state->dev_base[ZBC_REG_SIGNATURE + 0] == ZBC_SIGNATURE_BYTE0 &&
            state->dev_base[ZBC_REG_SIGNATURE + 1] == ZBC_SIGNATURE_BYTE1 &&
            state->dev_base[ZBC_REG_SIGNATURE + 2] == ZBC_SIGNATURE_BYTE2 &&
            state->dev_base[ZBC_REG_SIGNATURE + 3] == ZBC_SIGNATURE_BYTE3 &&
            state->dev_base[ZBC_REG_SIGNATURE + 4] == ZBC_SIGNATURE_BYTE4 &&
            state->dev_base[ZBC_REG_SIGNATURE + 5] == ZBC_SIGNATURE_BYTE5 &&
            state->dev_base[ZBC_REG_SIGNATURE + 6] == ZBC_SIGNATURE_BYTE6 &&
            state->dev_base[ZBC_REG_SIGNATURE + 7] == ZBC_SIGNATURE_BYTE7);
}

int zbc_client_device_present(const zbc_client_state_t *state)
{
    uint8_t status;

    if (!state || !state->dev_base) {
        return 0;
    }

    status = state->dev_base[ZBC_REG_STATUS];
    return (status & ZBC_STATUS_DEVICE_PRESENT) != 0;
}

void zbc_client_reset_cnfg(zbc_client_state_t *state)
{
    if (state) {
        state->cnfg_sent = 0;
    }
}

/*========================================================================
 * Request building (table-driven)
 *========================================================================*/

/*
 * Write CNFG chunk if not yet sent.
 */
static int write_cnfg_if_needed(uint8_t *buf, size_t capacity, size_t *offset,
                                zbc_client_state_t *state)
{
    uint8_t *size_ptr;
    uint8_t cnfg_data[4];

    if (state->cnfg_sent) {
        return 0;
    }

    if (*offset + ZBC_CNFG_TOTAL_SIZE > capacity) {
        return ZBC_ERR_BUFFER_TOO_SMALL;
    }

    size_ptr = zbc_riff_begin_chunk(buf, capacity, offset, ZBC_ID_CNFG);
    if (!size_ptr) {
        return ZBC_ERR_BUFFER_TOO_SMALL;
    }

    cnfg_data[0] = state->int_size;
    cnfg_data[1] = state->ptr_size;
    cnfg_data[2] = state->endianness;
    cnfg_data[3] = 0;  /* Reserved */

    if (zbc_riff_write_bytes(buf, capacity, offset, cnfg_data, 4) < 0) {
        return ZBC_ERR_BUFFER_TOO_SMALL;
    }

    zbc_riff_patch_size(size_ptr, ZBC_CNFG_DATA_SIZE);
    state->cnfg_sent = 1;

    return 0;
}

/*
 * Write a PARM chunk with an integer value.
 */
static int write_parm_chunk(uint8_t *buf, size_t capacity, size_t *offset,
                            unsigned int value, int is_signed,
                            const zbc_client_state_t *state)
{
    uint8_t *size_ptr;
    size_t data_size;
    uint8_t type_hdr[4];

    data_size = 4 + state->int_size;  /* type(1) + reserved(3) + value */

    size_ptr = zbc_riff_begin_chunk(buf, capacity, offset, ZBC_ID_PARM);
    if (!size_ptr) {
        return ZBC_ERR_BUFFER_TOO_SMALL;
    }

    type_hdr[0] = ZBC_PARM_TYPE_INT;
    type_hdr[1] = 0;
    type_hdr[2] = 0;
    type_hdr[3] = 0;

    if (zbc_riff_write_bytes(buf, capacity, offset, type_hdr, 4) < 0) {
        return ZBC_ERR_BUFFER_TOO_SMALL;
    }

    if (*offset + state->int_size > capacity) {
        return ZBC_ERR_BUFFER_TOO_SMALL;
    }

    zbc_write_native_uint(buf + *offset, value, state->int_size,
                          state->endianness);
    *offset += state->int_size;

    zbc_riff_patch_size(size_ptr, data_size);

    (void)is_signed;  /* Currently unused, same encoding */
    return 0;
}

/*
 * Write a DATA chunk with binary content.
 */
static int write_data_chunk(uint8_t *buf, size_t capacity, size_t *offset,
                            const void *data, size_t len, int data_type)
{
    uint8_t *size_ptr;
    size_t data_size;
    size_t padded_size;
    uint8_t type_hdr[4];

    data_size = 4 + len;  /* type(1) + reserved(3) + payload */
    padded_size = ZBC_PAD_SIZE(data_size);

    if (*offset + ZBC_CHUNK_HDR_SIZE + padded_size > capacity) {
        return ZBC_ERR_BUFFER_TOO_SMALL;
    }

    size_ptr = zbc_riff_begin_chunk(buf, capacity, offset, ZBC_ID_DATA);
    if (!size_ptr) {
        return ZBC_ERR_BUFFER_TOO_SMALL;
    }

    type_hdr[0] = (uint8_t)data_type;
    type_hdr[1] = 0;
    type_hdr[2] = 0;
    type_hdr[3] = 0;

    if (zbc_riff_write_bytes(buf, capacity, offset, type_hdr, 4) < 0) {
        return ZBC_ERR_BUFFER_TOO_SMALL;
    }

    if (len > 0 && data) {
        if (zbc_riff_write_bytes(buf, capacity, offset, data, len) < 0) {
            return ZBC_ERR_BUFFER_TOO_SMALL;
        }
    }

    /* Pad if odd size */
    if (data_size & 1) {
        if (*offset < capacity) {
            buf[*offset] = 0;
            (*offset)++;
        }
    }

    zbc_riff_patch_size(size_ptr, data_size);
    return 0;
}

/*
 * Build request from opcode table entry and args array.
 */
static int build_request(uint8_t *buf, size_t capacity, size_t *out_size,
                         zbc_client_state_t *state,
                         const zbc_opcode_entry_t *entry,
                         uintptr_t *args)
{
    uint8_t *riff_size_ptr;
    uint8_t *call_size_ptr;
    size_t offset;
    size_t call_start;
    int i;
    int rc;
    uint8_t opcode_hdr[4];

    /* Start RIFF container */
    riff_size_ptr = zbc_riff_begin_container(buf, capacity, &offset, ZBC_ID_SEMI);
    if (!riff_size_ptr) {
        return ZBC_ERR_BUFFER_TOO_SMALL;
    }

    /* CNFG chunk if needed */
    rc = write_cnfg_if_needed(buf, capacity, &offset, state);
    if (rc < 0) {
        return rc;
    }

    /* CALL chunk */
    call_start = offset;
    call_size_ptr = zbc_riff_begin_chunk(buf, capacity, &offset, ZBC_ID_CALL);
    if (!call_size_ptr) {
        return ZBC_ERR_BUFFER_TOO_SMALL;
    }

    opcode_hdr[0] = entry->opcode;
    opcode_hdr[1] = 0;
    opcode_hdr[2] = 0;
    opcode_hdr[3] = 0;

    if (zbc_riff_write_bytes(buf, capacity, &offset, opcode_hdr, 4) < 0) {
        return ZBC_ERR_BUFFER_TOO_SMALL;
    }

    /* Emit chunks based on table */
    for (i = 0; i < 4; i++) {
        const zbc_chunk_desc_t *desc = &entry->params[i];

        if (desc->type == ZBC_CHUNK_NONE) {
            break;
        }

        switch (desc->type) {
        case ZBC_CHUNK_PARM_INT:
            rc = write_parm_chunk(buf, capacity, &offset,
                                  (unsigned int)args[desc->slot], 1, state);
            break;

        case ZBC_CHUNK_PARM_UINT:
            rc = write_parm_chunk(buf, capacity, &offset,
                                  (unsigned int)args[desc->slot], 0, state);
            break;

        case ZBC_CHUNK_DATA_PTR:
            rc = write_data_chunk(buf, capacity, &offset,
                                  (void *)args[desc->slot],
                                  (size_t)args[desc->len_slot],
                                  ZBC_DATA_TYPE_BINARY);
            break;

        case ZBC_CHUNK_DATA_STR:
            {
                const char *str = (const char *)args[desc->slot];
                size_t len = zbc_strlen(str) + 1;  /* Include null */
                rc = write_data_chunk(buf, capacity, &offset, str, len,
                                      ZBC_DATA_TYPE_STRING);
            }
            break;

        case ZBC_CHUNK_DATA_BYTE:
            {
                uint8_t byte = *(uint8_t *)args[desc->slot];
                rc = write_data_chunk(buf, capacity, &offset, &byte, 1,
                                      ZBC_DATA_TYPE_BINARY);
            }
            break;

        default:
            rc = ZBC_ERR_INVALID_ARG;
            break;
        }

        if (rc < 0) {
            return rc;
        }
    }

    /* Patch CALL size */
    zbc_riff_patch_size(call_size_ptr, offset - call_start - ZBC_CHUNK_HDR_SIZE);

    /* Patch RIFF size */
    zbc_riff_patch_size(riff_size_ptr, offset - 8);

    *out_size = offset;
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
 *========================================================================*/

int zbc_parse_response(zbc_response_t *response, const uint8_t *buf,
                       size_t capacity, const zbc_client_state_t *state)
{
    uint32_t chunk_id;
    uint32_t chunk_size;
    size_t offset;
    int int_size;
    const uint8_t *retn_data;
    size_t retn_offset;

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

    /* Validate RIFF header */
    if (zbc_riff_validate_container(buf, capacity, ZBC_ID_SEMI) < 0) {
        return ZBC_ERR_PARSE_ERROR;
    }

    /* Iterate through chunks looking for RETN or ERRO */
    offset = ZBC_HDR_SIZE;
    while (offset + ZBC_CHUNK_HDR_SIZE <= capacity) {
        if (zbc_riff_read_header(buf, capacity, offset, &chunk_id, &chunk_size) < 0) {
            break;
        }

        if (chunk_id == ZBC_ID_ERRO) {
            /* Found error response */
            response->is_error = 1;
            if (offset + ZBC_CHUNK_HDR_SIZE + 2 <= capacity) {
                response->proto_error = ZBC_READ_U16_LE(buf + offset + 8);
            }
            return ZBC_OK;
        }

        if (chunk_id == ZBC_ID_RETN) {
            /* Found return response - parse it */
            if (offset + ZBC_CHUNK_HDR_SIZE + int_size + 4 > capacity) {
                return ZBC_ERR_PARSE_ERROR;
            }

            retn_data = buf + offset + ZBC_CHUNK_HDR_SIZE;
            response->result = zbc_read_native_int(retn_data, int_size,
                                                   state->endianness);
            response->error_code = (int)ZBC_READ_U32_LE(retn_data + int_size);

            /* Check for DATA sub-chunk within RETN */
            retn_offset = offset + ZBC_CHUNK_HDR_SIZE + int_size + 4;
            if (retn_offset + ZBC_CHUNK_HDR_SIZE <= offset + ZBC_CHUNK_HDR_SIZE + chunk_size) {
                uint32_t sub_id, sub_size;
                if (zbc_riff_read_header(buf, capacity, retn_offset,
                                         &sub_id, &sub_size) == 0) {
                    if (sub_id == ZBC_ID_DATA && sub_size >= 4) {
                        if (retn_offset + ZBC_CHUNK_HDR_SIZE + sub_size <= capacity) {
                            response->data = buf + retn_offset + ZBC_CHUNK_HDR_SIZE + 4;
                            response->data_size = sub_size - 4;
                        }
                    }
                }
            }

            return ZBC_OK;
        }

        /* Skip to next chunk */
        offset = zbc_riff_skip_chunk(buf, capacity, offset);
        if (offset == 0) {
            break;
        }
    }

    /* No RETN or ERRO found */
    return ZBC_ERR_PARSE_ERROR;
}

/*========================================================================
 * Copy helper (no libc)
 *========================================================================*/

static void copy_bytes(void *dest, const void *src, size_t n)
{
    size_t i;
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

/*========================================================================
 * Main entry points
 *========================================================================*/

uintptr_t zbc_call(zbc_client_state_t *state, void *buf, size_t buf_size,
                   int opcode, uintptr_t *args)
{
    const zbc_opcode_entry_t *entry;
    zbc_response_t response;
    size_t riff_size;
    int rc;

    if (!state || !buf) {
        return (uintptr_t)-1;
    }

    entry = zbc_opcode_lookup(opcode);
    if (!entry) {
        return (uintptr_t)-1;
    }

    /* args may be NULL for operations with no parameters */

    /* Build request */
    rc = build_request((uint8_t *)buf, buf_size, &riff_size, state, entry, args);
    if (rc < 0) {
        return (uintptr_t)-1;
    }

    /* Submit and wait */
    rc = zbc_client_submit_poll(state, buf, riff_size);
    if (rc < 0) {
        return (uintptr_t)-1;
    }

    /* Parse response */
    rc = zbc_parse_response(&response, (uint8_t *)buf, buf_size, state);
    if (rc < 0) {
        return (uintptr_t)-1;
    }

    if (response.is_error) {
        return (uintptr_t)-1;
    }

    /* Handle response data based on response type */
    switch (entry->resp_type) {
    case ZBC_RESP_DATA:
        if (response.data && response.data_size > 0) {
            size_t max_len = (size_t)args[entry->resp_len_slot];
            size_t copy_size = (response.data_size < max_len) ?
                               response.data_size : max_len;
            copy_bytes((void *)args[entry->resp_dest], response.data, copy_size);
            /* Null-terminate if it's a string destination */
            if (copy_size < max_len) {
                ((char *)args[entry->resp_dest])[copy_size] = '\0';
            }
        }
        break;

    case ZBC_RESP_ELAPSED:
        if (response.data && response.data_size >= 8) {
            copy_bytes((void *)args[entry->resp_dest], response.data, 8);
        }
        break;

    case ZBC_RESP_HEAPINFO:
        /* TODO: Parse 4 PARM chunks for heap info */
        break;

    default:
        break;
    }

    return (uintptr_t)response.result;
}

uintptr_t zbc_semihost(zbc_client_state_t *state, uint8_t *riff_buf,
                       size_t riff_buf_size, uintptr_t op, uintptr_t param)
{
    uintptr_t *args = (uintptr_t *)param;
    return zbc_call(state, riff_buf, riff_buf_size, (int)op, args);
}
