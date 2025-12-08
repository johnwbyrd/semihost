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
 * Request building (struct-based, no magic offsets)
 *========================================================================*/

/*
 * Write CNFG chunk if not yet sent.
 * Uses zbc_chunk_t and zbc_cnfg_payload_t structs.
 */
static int write_cnfg_if_needed(uint8_t *buf, size_t capacity, size_t *pos,
                                zbc_client_state_t *state)
{
    zbc_chunk_t *chunk;
    zbc_cnfg_payload_t *cnfg;
    size_t total_size;

    if (state->cnfg_sent) {
        return ZBC_OK;
    }

    total_size = ZBC_CHUNK_HDR_SIZE + ZBC_CNFG_PAYLOAD_SIZE;
    if (*pos + total_size > capacity) {
        return ZBC_ERR_BUFFER_FULL;
    }

    chunk = (zbc_chunk_t *)(buf + *pos);
    chunk->id = ZBC_ID_CNFG;
    chunk->size = ZBC_CNFG_PAYLOAD_SIZE;

    cnfg = (zbc_cnfg_payload_t *)chunk->data;
    cnfg->int_size = state->int_size;
    cnfg->ptr_size = state->ptr_size;
    cnfg->endianness = state->endianness;
    cnfg->reserved = 0;

    *pos += total_size;
    state->cnfg_sent = 1;

    return ZBC_OK;
}

/*
 * Write a PARM chunk with an integer value.
 * Uses zbc_chunk_t and zbc_parm_payload_t structs.
 */
static int write_parm_chunk(uint8_t *buf, size_t capacity, size_t *pos,
                            unsigned int value, int is_signed,
                            const zbc_client_state_t *state)
{
    zbc_chunk_t *chunk;
    zbc_parm_payload_t *parm;
    size_t payload_size;
    size_t total_size;

    /* Payload = type(1) + reserved(3) + value(int_size) */
    payload_size = ZBC_PARM_HDR_SIZE + state->int_size;
    total_size = ZBC_CHUNK_HDR_SIZE + ZBC_PAD_SIZE(payload_size);

    if (*pos + total_size > capacity) {
        return ZBC_ERR_BUFFER_FULL;
    }

    chunk = (zbc_chunk_t *)(buf + *pos);
    chunk->id = ZBC_ID_PARM;
    chunk->size = (uint32_t)payload_size;

    parm = (zbc_parm_payload_t *)chunk->data;
    parm->type = ZBC_PARM_TYPE_INT;
    parm->reserved[0] = 0;
    parm->reserved[1] = 0;
    parm->reserved[2] = 0;
    zbc_write_native_uint(parm->value, value, state->int_size, state->endianness);

    *pos += total_size;

    (void)is_signed;  /* Currently unused, same encoding */
    return ZBC_OK;
}

/*
 * Write a DATA chunk with binary content.
 * Uses zbc_chunk_t and zbc_data_payload_t structs.
 */
static int write_data_chunk(uint8_t *buf, size_t capacity, size_t *pos,
                            const void *data, size_t len, int data_type)
{
    zbc_chunk_t *chunk;
    zbc_data_payload_t *data_payload;
    size_t payload_size;
    size_t total_size;
    size_t i;
    const uint8_t *src;

    /* Payload = type(1) + reserved(3) + data(len) */
    payload_size = ZBC_DATA_HDR_SIZE + len;
    total_size = ZBC_CHUNK_HDR_SIZE + ZBC_PAD_SIZE(payload_size);

    if (*pos + total_size > capacity) {
        return ZBC_ERR_BUFFER_FULL;
    }

    chunk = (zbc_chunk_t *)(buf + *pos);
    chunk->id = ZBC_ID_DATA;
    chunk->size = (uint32_t)payload_size;

    data_payload = (zbc_data_payload_t *)chunk->data;
    data_payload->type = (uint8_t)data_type;
    data_payload->reserved[0] = 0;
    data_payload->reserved[1] = 0;
    data_payload->reserved[2] = 0;

    /* Copy data */
    src = (const uint8_t *)data;
    for (i = 0; i < len; i++) {
        data_payload->payload[i] = src[i];
    }

    /* Add padding byte if odd */
    if (payload_size & 1) {
        chunk->data[payload_size] = 0;
    }

    *pos += total_size;
    return ZBC_OK;
}

/*
 * Build request from opcode table entry and args array.
 * Uses zbc_riff_t, zbc_chunk_t, and zbc_call_header_t structs.
 */
static int build_request(uint8_t *buf, size_t capacity, size_t *out_size,
                         zbc_client_state_t *state,
                         const zbc_opcode_entry_t *entry,
                         uintptr_t *args)
{
    zbc_riff_t *riff;
    zbc_chunk_t *call_chunk;
    zbc_call_header_t *call_hdr;
    size_t pos;
    size_t call_data_start;
    int i;
    int rc;

    /* Minimum size for RIFF header */
    if (capacity < ZBC_RIFF_HDR_SIZE) {
        return ZBC_ERR_BUFFER_FULL;
    }

    /* Write RIFF header (size patched later) */
    riff = (zbc_riff_t *)buf;
    riff->riff_id = ZBC_ID_RIFF;
    riff->size = 0;  /* Placeholder */
    riff->form_type = ZBC_ID_SEMI;
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

    call_chunk = (zbc_chunk_t *)(buf + pos);
    call_chunk->id = ZBC_ID_CALL;
    call_chunk->size = 0;  /* Placeholder */
    pos += ZBC_CHUNK_HDR_SIZE;
    call_data_start = pos;

    /* CALL header (opcode) */
    call_hdr = (zbc_call_header_t *)(buf + pos);
    call_hdr->opcode = entry->opcode;
    call_hdr->reserved[0] = 0;
    call_hdr->reserved[1] = 0;
    call_hdr->reserved[2] = 0;
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
                                  (unsigned int)args[desc->slot], 1, state);
            break;

        case ZBC_CHUNK_PARM_UINT:
            rc = write_parm_chunk(buf, capacity, &pos,
                                  (unsigned int)args[desc->slot], 0, state);
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

    /* Patch CALL chunk size */
    call_chunk->size = (uint32_t)(pos - call_data_start);

    /* Patch RIFF size (everything after the size field: form_type + chunks) */
    riff->size = (uint32_t)(pos - 4 - 4);

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
 * Uses the unified zbc_riff_parse() to extract all fields at once.
 *========================================================================*/

int zbc_parse_response(zbc_response_t *response, const uint8_t *buf,
                       size_t capacity, const zbc_client_state_t *state)
{
    zbc_parsed_t parsed;
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

    /* Parse the entire RIFF structure */
    rc = zbc_riff_parse(buf, capacity, state->int_size, state->endianness, &parsed);
    if (rc != ZBC_OK) {
        return ZBC_ERR_PARSE_ERROR;
    }

    /* Check for ERRO */
    if (parsed.has_erro) {
        response->is_error = 1;
        response->proto_error = parsed.proto_error;
        return ZBC_OK;
    }

    /* Check for RETN */
    if (parsed.has_retn) {
        response->result = parsed.result;
        response->error_code = parsed.host_errno;

        /* Get DATA if present */
        if (parsed.data_count > 0) {
            response->data = parsed.data[0].ptr;
            response->data_size = parsed.data[0].size;
        }
        return ZBC_OK;
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
    if (rc != ZBC_OK) {
        return (uintptr_t)-1;
    }

    /* Submit and wait */
    rc = zbc_client_submit_poll(state, buf, riff_size);
    if (rc != ZBC_OK) {
        return (uintptr_t)-1;
    }

    /* Parse response */
    rc = zbc_parse_response(&response, (uint8_t *)buf, buf_size, state);
    if (rc != ZBC_OK) {
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
