/*
 * ZBC Semihosting Host Library - Core Functions
 *
 * Initialization, RIFF parsing, dispatch, and response building.
 */

#include "zbc_semi_host.h"

/*------------------------------------------------------------------------
 * Default handler: returns -1 with ENOSYS
 *------------------------------------------------------------------------*/

#ifndef ENOSYS
#define ENOSYS 38  /* Function not implemented */
#endif

static int default_handler(zbc_syscall_ctx_t *ctx, zbc_syscall_result_t *result)
{
    (void)ctx;
    result->result = -1;
    result->error = ENOSYS;
    result->data = (void *)0;
    result->data_size = 0;
    result->parm_count = 0;
    return 0;
}

/*========================================================================
 * Initialization
 *========================================================================*/

void zbc_host_init(zbc_host_state_t *state,
                   const zbc_host_mem_ops_t *mem_ops,
                   void *mem_context,
                   uint8_t *work_buf,
                   size_t work_buf_size)
{
    size_t i;
    zbc_syscall_handler_t *handler_array;

    if (!state) return;

    /* Initialize configuration as unknown */
    state->int_size = 0;
    state->ptr_size = 0;
    state->endianness = ZBC_ENDIAN_LITTLE;
    state->cnfg_received = 0;

    /* Set memory ops */
    if (mem_ops) {
        state->mem_ops = *mem_ops;
    } else {
        state->mem_ops.read_u8 = (uint8_t (*)(uint64_t, void *))0;
        state->mem_ops.write_u8 = (void (*)(uint64_t, uint8_t, void *))0;
        state->mem_ops.read_block = (void (*)(void *, uint64_t, size_t, void *))0;
        state->mem_ops.write_block = (void (*)(uint64_t, const void *, size_t, void *))0;
    }
    state->mem_context = mem_context;

    /* Initialize all handlers to default */
    handler_array = (zbc_syscall_handler_t *)&state->handlers;
    for (i = 0; i < sizeof(state->handlers) / sizeof(zbc_syscall_handler_t); i++) {
        handler_array[i] = default_handler;
    }
    state->handler_context = (void *)0;

    /* Set work buffer */
    state->work_buf = work_buf;
    state->work_buf_size = work_buf_size;

    state->last_errno = 0;
}

void zbc_host_set_handlers(zbc_host_state_t *state,
                           const zbc_host_handlers_t *handlers,
                           void *context)
{
    size_t i;
    const zbc_syscall_handler_t *src_array;
    zbc_syscall_handler_t *dst_array;

    if (!state) return;

    if (handlers) {
        src_array = (const zbc_syscall_handler_t *)handlers;
        dst_array = (zbc_syscall_handler_t *)&state->handlers;

        for (i = 0; i < sizeof(*handlers) / sizeof(zbc_syscall_handler_t); i++) {
            dst_array[i] = src_array[i] ? src_array[i] : default_handler;
        }
    }

    state->handler_context = context;
}

void zbc_host_set_handler(zbc_host_state_t *state,
                          uint8_t opcode,
                          zbc_syscall_handler_t handler)
{
    if (!state) return;

    /* Map opcode to handler index */
    switch (opcode) {
    case SH_SYS_OPEN:          state->handlers.sys_open = handler ? handler : default_handler; break;
    case SH_SYS_CLOSE:         state->handlers.sys_close = handler ? handler : default_handler; break;
    case SH_SYS_WRITEC:        state->handlers.sys_writec = handler ? handler : default_handler; break;
    case SH_SYS_WRITE0:        state->handlers.sys_write0 = handler ? handler : default_handler; break;
    case SH_SYS_WRITE:         state->handlers.sys_write = handler ? handler : default_handler; break;
    case SH_SYS_READ:          state->handlers.sys_read = handler ? handler : default_handler; break;
    case SH_SYS_READC:         state->handlers.sys_readc = handler ? handler : default_handler; break;
    case SH_SYS_ISERROR:       state->handlers.sys_iserror = handler ? handler : default_handler; break;
    case SH_SYS_ISTTY:         state->handlers.sys_istty = handler ? handler : default_handler; break;
    case SH_SYS_SEEK:          state->handlers.sys_seek = handler ? handler : default_handler; break;
    case SH_SYS_FLEN:          state->handlers.sys_flen = handler ? handler : default_handler; break;
    case SH_SYS_TMPNAM:        state->handlers.sys_tmpnam = handler ? handler : default_handler; break;
    case SH_SYS_REMOVE:        state->handlers.sys_remove = handler ? handler : default_handler; break;
    case SH_SYS_RENAME:        state->handlers.sys_rename = handler ? handler : default_handler; break;
    case SH_SYS_CLOCK:         state->handlers.sys_clock = handler ? handler : default_handler; break;
    case SH_SYS_TIME:          state->handlers.sys_time = handler ? handler : default_handler; break;
    case SH_SYS_SYSTEM:        state->handlers.sys_system = handler ? handler : default_handler; break;
    case SH_SYS_ERRNO:         state->handlers.sys_errno = handler ? handler : default_handler; break;
    case SH_SYS_GET_CMDLINE:   state->handlers.sys_get_cmdline = handler ? handler : default_handler; break;
    case SH_SYS_HEAPINFO:      state->handlers.sys_heapinfo = handler ? handler : default_handler; break;
    case SH_SYS_EXIT:          state->handlers.sys_exit = handler ? handler : default_handler; break;
    case SH_SYS_EXIT_EXTENDED: state->handlers.sys_exit_extended = handler ? handler : default_handler; break;
    case SH_SYS_ELAPSED:       state->handlers.sys_elapsed = handler ? handler : default_handler; break;
    case SH_SYS_TICKFREQ:      state->handlers.sys_tickfreq = handler ? handler : default_handler; break;
    default:
        break;
    }
}

void zbc_host_get_default_handlers(zbc_host_handlers_t *handlers)
{
    size_t i;
    zbc_syscall_handler_t *handler_array;

    if (!handlers) return;

    handler_array = (zbc_syscall_handler_t *)handlers;
    for (i = 0; i < sizeof(*handlers) / sizeof(zbc_syscall_handler_t); i++) {
        handler_array[i] = default_handler;
    }
}

void zbc_host_reset_cnfg(zbc_host_state_t *state)
{
    if (state) {
        state->cnfg_received = 0;
        state->int_size = 0;
        state->ptr_size = 0;
    }
}

/*========================================================================
 * Guest Memory Access
 *========================================================================*/

void zbc_host_read_guest(zbc_host_state_t *state,
                         void *dest,
                         uint64_t addr,
                         size_t size)
{
    uint8_t *d;
    size_t i;

    if (!state || !dest || !state->mem_ops.read_u8) return;

    if (state->mem_ops.read_block) {
        state->mem_ops.read_block(dest, addr, size, state->mem_context);
    } else {
        d = (uint8_t *)dest;
        for (i = 0; i < size; i++) {
            d[i] = state->mem_ops.read_u8(addr + i, state->mem_context);
        }
    }
}

void zbc_host_write_guest(zbc_host_state_t *state,
                          uint64_t addr,
                          const void *src,
                          size_t size)
{
    const uint8_t *s;
    size_t i;

    if (!state || !src || !state->mem_ops.write_u8) return;

    if (state->mem_ops.write_block) {
        state->mem_ops.write_block(addr, src, size, state->mem_context);
    } else {
        s = (const uint8_t *)src;
        for (i = 0; i < size; i++) {
            state->mem_ops.write_u8(addr + i, s[i], state->mem_context);
        }
    }
}

/*========================================================================
 * Value Conversion
 *========================================================================*/

int64_t zbc_host_read_int(const zbc_host_state_t *state, const uint8_t *buf)
{
    uint64_t value = 0;
    size_t size;
    size_t i;
    uint64_t sign_bit;

    if (!state || !buf) return 0;

    size = state->int_size;
    if (size == 0 || size > 8) return 0;

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

    return (int64_t)value;
}

void zbc_host_write_int(const zbc_host_state_t *state, uint8_t *buf, int64_t value)
{
    uint64_t uval;
    size_t size;
    size_t i;

    if (!state || !buf) return;

    size = state->int_size;
    if (size == 0 || size > 8) return;

    uval = (uint64_t)value;

    if (state->endianness == ZBC_ENDIAN_LITTLE) {
        for (i = 0; i < size; i++) {
            buf[i] = (uint8_t)(uval & 0xFF);
            uval >>= 8;
        }
    } else if (state->endianness == ZBC_ENDIAN_BIG) {
        for (i = size; i > 0; i--) {
            buf[i - 1] = (uint8_t)(uval & 0xFF);
            uval >>= 8;
        }
    } else {
        /* PDP endian */
        for (i = 0; i < size; i += 2) {
            if (i + 1 < size) {
                buf[i + 1] = (uint8_t)(uval & 0xFF);
                uval >>= 8;
                buf[i] = (uint8_t)(uval & 0xFF);
                uval >>= 8;
            } else {
                buf[i] = (uint8_t)(uval & 0xFF);
            }
        }
    }
}

uint64_t zbc_host_read_ptr(const zbc_host_state_t *state, const uint8_t *buf)
{
    uint64_t value = 0;
    size_t size;
    size_t i;

    if (!state || !buf) return 0;

    size = state->ptr_size;
    if (size == 0 || size > 8) return 0;

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

void zbc_host_write_ptr(const zbc_host_state_t *state, uint8_t *buf, uint64_t value)
{
    size_t size;
    size_t i;

    if (!state || !buf) return;

    size = state->ptr_size;
    if (size == 0 || size > 8) return;

    if (state->endianness == ZBC_ENDIAN_LITTLE) {
        for (i = 0; i < size; i++) {
            buf[i] = (uint8_t)(value & 0xFF);
            value >>= 8;
        }
    } else if (state->endianness == ZBC_ENDIAN_BIG) {
        for (i = size; i > 0; i--) {
            buf[i - 1] = (uint8_t)(value & 0xFF);
            value >>= 8;
        }
    } else {
        /* PDP endian */
        for (i = 0; i < size; i += 2) {
            if (i + 1 < size) {
                buf[i + 1] = (uint8_t)(value & 0xFF);
                value >>= 8;
                buf[i] = (uint8_t)(value & 0xFF);
                value >>= 8;
            } else {
                buf[i] = (uint8_t)(value & 0xFF);
            }
        }
    }
}

/*========================================================================
 * Internal: Get handler for opcode
 *========================================================================*/

static zbc_syscall_handler_t get_handler(zbc_host_state_t *state, uint8_t opcode)
{
    switch (opcode) {
    case SH_SYS_OPEN:          return state->handlers.sys_open;
    case SH_SYS_CLOSE:         return state->handlers.sys_close;
    case SH_SYS_WRITEC:        return state->handlers.sys_writec;
    case SH_SYS_WRITE0:        return state->handlers.sys_write0;
    case SH_SYS_WRITE:         return state->handlers.sys_write;
    case SH_SYS_READ:          return state->handlers.sys_read;
    case SH_SYS_READC:         return state->handlers.sys_readc;
    case SH_SYS_ISERROR:       return state->handlers.sys_iserror;
    case SH_SYS_ISTTY:         return state->handlers.sys_istty;
    case SH_SYS_SEEK:          return state->handlers.sys_seek;
    case SH_SYS_FLEN:          return state->handlers.sys_flen;
    case SH_SYS_TMPNAM:        return state->handlers.sys_tmpnam;
    case SH_SYS_REMOVE:        return state->handlers.sys_remove;
    case SH_SYS_RENAME:        return state->handlers.sys_rename;
    case SH_SYS_CLOCK:         return state->handlers.sys_clock;
    case SH_SYS_TIME:          return state->handlers.sys_time;
    case SH_SYS_SYSTEM:        return state->handlers.sys_system;
    case SH_SYS_ERRNO:         return state->handlers.sys_errno;
    case SH_SYS_GET_CMDLINE:   return state->handlers.sys_get_cmdline;
    case SH_SYS_HEAPINFO:      return state->handlers.sys_heapinfo;
    case SH_SYS_EXIT:          return state->handlers.sys_exit;
    case SH_SYS_EXIT_EXTENDED: return state->handlers.sys_exit_extended;
    case SH_SYS_ELAPSED:       return state->handlers.sys_elapsed;
    case SH_SYS_TICKFREQ:      return state->handlers.sys_tickfreq;
    default:                   return (zbc_syscall_handler_t)0;
    }
}

/*========================================================================
 * Internal: Write ERRO chunk to guest memory
 *========================================================================*/

static int write_erro(zbc_host_state_t *state, uint64_t addr, uint16_t error_code)
{
    uint8_t erro[16];
    size_t offset;

    /* Build ERRO chunk */
    ZBC_WRITE_FOURCC(erro, 'E', 'R', 'R', 'O');
    ZBC_WRITE_U32_LE(erro + 4, 4);  /* chunk size: error_code(2) + reserved(2) */
    ZBC_WRITE_U16_LE(erro + 8, error_code);
    erro[10] = 0;  /* reserved */
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

static int write_retn(zbc_host_state_t *state,
                      uint64_t addr,
                      const zbc_syscall_result_t *result)
{
    uint8_t retn_buf[64];  /* Should be enough for header + result + errno */
    size_t offset;
    size_t retn_size;
    size_t int_size;
    size_t ptr_size;
    size_t write_offset;
    size_t data_chunk_size;
    size_t padded_data_size;
    size_t parm_chunk_size;
    size_t padded_parm_size;
    size_t i;
    const uint8_t *src;
    int p;

    int_size = state->int_size;
    ptr_size = state->ptr_size;

    /* Calculate RETN chunk data size */
    retn_size = int_size + 4;  /* result + errno */

    /* Add size for PARM sub-chunks */
    for (p = 0; p < result->parm_count && p < ZBC_HOST_MAX_RESULT_PARMS; p++) {
        size_t value_size = (result->parm_types[p] == ZBC_PARM_TYPE_PTR) ? ptr_size : int_size;
        parm_chunk_size = 4 + value_size;  /* type(1) + reserved(3) + value */
        padded_parm_size = ZBC_PAD_SIZE(parm_chunk_size);
        retn_size += ZBC_CHUNK_HDR_SIZE + padded_parm_size;
    }

    /* Add size for DATA sub-chunk */
    if (result->data && result->data_size > 0) {
        data_chunk_size = 4 + result->data_size;  /* type(1) + reserved(3) + payload */
        padded_data_size = ZBC_PAD_SIZE(data_chunk_size);
        retn_size += ZBC_CHUNK_HDR_SIZE + padded_data_size;
    }

    /* Build RETN chunk header */
    ZBC_WRITE_FOURCC(retn_buf, 'R', 'E', 'T', 'N');
    ZBC_WRITE_U32_LE(retn_buf + 4, (uint32_t)retn_size);

    /* Write result in guest endianness */
    zbc_host_write_int(state, retn_buf + 8, result->result);

    /* Write errno (always little-endian per spec) */
    ZBC_WRITE_U32_LE(retn_buf + 8 + int_size, (uint32_t)result->error);

    write_offset = 8 + int_size + 4;

    /* Calculate position in guest memory */
    offset = ZBC_HDR_SIZE;
    if (state->cnfg_received) {
        offset += ZBC_CNFG_TOTAL_SIZE;
    }

    /* Write RETN header + result + errno */
    zbc_host_write_guest(state, addr + offset, retn_buf, write_offset);

    /* Write PARM sub-chunks if present */
    for (p = 0; p < result->parm_count && p < ZBC_HOST_MAX_RESULT_PARMS; p++) {
        uint8_t parm_buf[32];  /* header(8) + type(1) + reserved(3) + value(up to 16) */
        size_t value_size = (result->parm_types[p] == ZBC_PARM_TYPE_PTR) ? ptr_size : int_size;
        size_t parm_data_size = 4 + value_size;

        ZBC_WRITE_FOURCC(parm_buf, 'P', 'A', 'R', 'M');
        ZBC_WRITE_U32_LE(parm_buf + 4, (uint32_t)parm_data_size);
        parm_buf[8] = result->parm_types[p];
        parm_buf[9] = 0;
        parm_buf[10] = 0;
        parm_buf[11] = 0;

        /* Write value in guest endianness */
        if (result->parm_types[p] == ZBC_PARM_TYPE_PTR) {
            zbc_host_write_ptr(state, parm_buf + 12, result->parm_values[p]);
        } else {
            zbc_host_write_int(state, parm_buf + 12, (int64_t)result->parm_values[p]);
        }

        zbc_host_write_guest(state, addr + offset + write_offset, parm_buf, 12 + value_size);
        write_offset += ZBC_CHUNK_HDR_SIZE + ZBC_PAD_SIZE(parm_data_size);
    }

    /* Write DATA sub-chunk if present */
    if (result->data && result->data_size > 0) {
        uint8_t data_hdr[12];

        data_chunk_size = 4 + result->data_size;

        ZBC_WRITE_FOURCC(data_hdr, 'D', 'A', 'T', 'A');
        ZBC_WRITE_U32_LE(data_hdr + 4, (uint32_t)data_chunk_size);
        data_hdr[8] = ZBC_DATA_TYPE_BINARY;
        data_hdr[9] = 0;
        data_hdr[10] = 0;
        data_hdr[11] = 0;

        zbc_host_write_guest(state, addr + offset + write_offset, data_hdr, 12);
        write_offset += 12;

        /* Write payload */
        src = (const uint8_t *)result->data;
        for (i = 0; i < result->data_size; i++) {
            state->mem_ops.write_u8(addr + offset + write_offset + i, src[i], state->mem_context);
        }

        /* Write padding byte if odd size */
        if (data_chunk_size & 1) {
            state->mem_ops.write_u8(addr + offset + write_offset + result->data_size, 0, state->mem_context);
        }
    }

    return ZBC_OK;
}

/*========================================================================
 * Main Processing Function
 *========================================================================*/

int zbc_host_process(zbc_host_state_t *state, uint64_t riff_addr)
{
    uint32_t chunk_id;
    uint32_t riff_size;
    size_t offset;
    size_t chunk_size;
    uint8_t opcode;
    zbc_syscall_ctx_t ctx;
    zbc_syscall_result_t result;
    zbc_syscall_handler_t handler;
    int rc;

    if (!state || !state->work_buf || !state->mem_ops.read_u8) {
        return ZBC_ERR_INVALID_ARG;
    }

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
    opcode = state->work_buf[offset + 8];

    /* Initialize context */
    ctx.state = state;
    ctx.user_context = state->handler_context;
    ctx.parm_count = 0;
    ctx.data_count = 0;

    /* Parse sub-chunks (PARM and DATA) */
    {
        size_t sub_offset = offset + ZBC_CALL_HDR_SIZE;  /* After CALL header */
        size_t call_end = offset + ZBC_CHUNK_HDR_SIZE + chunk_size;
        uint32_t sub_id;
        uint32_t sub_size;

        while (sub_offset + ZBC_CHUNK_HDR_SIZE <= call_end) {
            sub_id = ZBC_READ_U32_LE(state->work_buf + sub_offset);
            sub_size = ZBC_READ_U32_LE(state->work_buf + sub_offset + 4);

            if (sub_id == ZBC_ID_PARM) {
                /* Parse PARM chunk */
                if (ctx.parm_count < ZBC_HOST_MAX_PARMS && sub_size >= 4) {
                    uint8_t parm_type = state->work_buf[sub_offset + 8];
                    size_t value_size;

                    if (parm_type == ZBC_PARM_TYPE_INT) {
                        value_size = state->int_size;
                    } else if (parm_type == ZBC_PARM_TYPE_PTR) {
                        value_size = state->ptr_size;
                    } else {
                        value_size = 0;
                    }

                    if (value_size > 0 && sub_size >= 4 + value_size) {
                        ctx.parms[ctx.parm_count] = zbc_host_read_int(state,
                            state->work_buf + sub_offset + 12);
                        ctx.parm_count++;
                    }
                }
            } else if (sub_id == ZBC_ID_DATA) {
                /* Parse DATA chunk */
                if (ctx.data_count < ZBC_HOST_MAX_DATA && sub_size >= 4) {
                    ctx.data[ctx.data_count].type = state->work_buf[sub_offset + 8];
                    ctx.data[ctx.data_count].size = sub_size - 4;
                    ctx.data[ctx.data_count].data = state->work_buf + sub_offset + 12;
                    ctx.data_count++;
                }
            }

            sub_offset += ZBC_CHUNK_HDR_SIZE + ZBC_PAD_SIZE(sub_size);
        }
    }

    /* Get handler */
    handler = get_handler(state, opcode);
    if (!handler) {
        return write_erro(state, riff_addr, ZBC_PROTO_ERR_UNSUPPORTED_OP);
    }

    /* Initialize result */
    result.result = 0;
    result.error = 0;
    result.data = (void *)0;
    result.data_size = 0;
    result.parm_count = 0;

    /* Call handler */
    rc = handler(&ctx, &result);
    (void)rc;  /* Handler return value not used currently */

    /* Store errno for SYS_ERRNO */
    state->last_errno = result.error;

    /* Write response */
    return write_retn(state, riff_addr, &result);
}
