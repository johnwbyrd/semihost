/*
 * ZBC Semihosting Client - Core Functions
 *
 * Builder, response parser, and device communication.
 */

#include "zbc_semi_client.h"

/*------------------------------------------------------------------------
 * Internal helpers for writing values in native endianness
 *------------------------------------------------------------------------*/

static void write_native_uint(uint8_t *buf, unsigned long value, size_t size) {
  size_t i;

#if ZBC_CLIENT_ENDIANNESS == ZBC_ENDIAN_LITTLE
  for (i = 0; i < size; i++) {
    buf[i] = (uint8_t)(value & 0xFFU);
    value >>= 8;
  }
#elif ZBC_CLIENT_ENDIANNESS == ZBC_ENDIAN_BIG
  for (i = size; i > 0; i--) {
    buf[i - 1] = (uint8_t)(value & 0xFFU);
    value >>= 8;
  }
#else
  /* PDP endian: swap 16-bit words within 32-bit values */
  /* This is rare, but supported for completeness */
  for (i = 0; i < size; i += 2) {
    if (i + 1 < size) {
      buf[i + 1] = (uint8_t)(value & 0xFFU);
      value >>= 8;
      buf[i] = (uint8_t)(value & 0xFFU);
      value >>= 8;
    } else {
      buf[i] = (uint8_t)(value & 0xFFU);
    }
  }
#endif
}

static long read_native_int(const uint8_t *buf, size_t size) {
  unsigned long value = 0;
  size_t i;
  long result;
  unsigned long sign_bit;
  unsigned long sign_extend;

#if ZBC_CLIENT_ENDIANNESS == ZBC_ENDIAN_LITTLE
  for (i = size; i > 0; i--) {
    value = (value << 8) | buf[i - 1];
  }
#elif ZBC_CLIENT_ENDIANNESS == ZBC_ENDIAN_BIG
  for (i = 0; i < size; i++) {
    value = (value << 8) | buf[i];
  }
#else
  /* PDP endian */
  for (i = 0; i < size; i += 2) {
    if (i + 1 < size) {
      value = (value << 16) | ((unsigned long)buf[i] << 8) | buf[i + 1];
    } else {
      value = (value << 8) | buf[i];
    }
  }
#endif

  /* Sign extend if necessary */
  if (size < sizeof(long)) {
    sign_bit = 1UL << (size * 8 - 1);
    if (value & sign_bit) {
      sign_extend = ~((1UL << (size * 8)) - 1);
      value |= sign_extend;
    }
  }

  result = (long)value;
  return result;
}

static unsigned long read_native_uint(const uint8_t *buf, size_t size) {
  unsigned long value = 0;
  size_t i;

#if ZBC_CLIENT_ENDIANNESS == ZBC_ENDIAN_LITTLE
  for (i = size; i > 0; i--) {
    value = (value << 8) | buf[i - 1];
  }
#elif ZBC_CLIENT_ENDIANNESS == ZBC_ENDIAN_BIG
  for (i = 0; i < size; i++) {
    value = (value << 8) | buf[i];
  }
#else
  /* PDP endian */
  for (i = 0; i < size; i += 2) {
    if (i + 1 < size) {
      value = (value << 16) | ((unsigned long)buf[i] << 8) | buf[i + 1];
    } else {
      value = (value << 8) | buf[i];
    }
  }
#endif

  return value;
}

/*------------------------------------------------------------------------
 * String length helper (avoids strlen dependency)
 *------------------------------------------------------------------------*/

static size_t zbc_strlen(const char *s) {
  size_t len = 0;
  if (s) {
    while (*s++)
      len++;
  }
  return len;
}

/*========================================================================
 * Initialization
 *========================================================================*/

void zbc_client_init(zbc_client_state_t *state, volatile void *dev_base) {
  if (!state)
    return;

  state->dev_base = (volatile uint8_t *)dev_base;
  state->cnfg_sent = 0;
  state->int_size = (uint8_t)ZBC_CLIENT_INT_SIZE;
  state->ptr_size = (uint8_t)ZBC_CLIENT_PTR_SIZE;
  state->endianness = (uint8_t)ZBC_CLIENT_ENDIANNESS;
}

int zbc_client_check_signature(const zbc_client_state_t *state) {
  if (!state || !state->dev_base) {
    return 0;
  }

  /* Check for "SEMIHOST" signature at offset 0x00 */
  return (state->dev_base[ZBC_REG_SIGNATURE + 0] == ZBC_SIGNATURE_BYTE0 &&
          state->dev_base[ZBC_REG_SIGNATURE + 1] == ZBC_SIGNATURE_BYTE1 &&
          state->dev_base[ZBC_REG_SIGNATURE + 2] == ZBC_SIGNATURE_BYTE2 &&
          state->dev_base[ZBC_REG_SIGNATURE + 3] == ZBC_SIGNATURE_BYTE3 &&
          state->dev_base[ZBC_REG_SIGNATURE + 4] == ZBC_SIGNATURE_BYTE4 &&
          state->dev_base[ZBC_REG_SIGNATURE + 5] == ZBC_SIGNATURE_BYTE5 &&
          state->dev_base[ZBC_REG_SIGNATURE + 6] == ZBC_SIGNATURE_BYTE6 &&
          state->dev_base[ZBC_REG_SIGNATURE + 7] == ZBC_SIGNATURE_BYTE7);
}

int zbc_client_device_present(const zbc_client_state_t *state) {
  uint8_t status;

  if (!state || !state->dev_base) {
    return 0;
  }

  status = state->dev_base[ZBC_REG_STATUS];
  return (status & ZBC_STATUS_DEVICE_PRESENT) != 0;
}

void zbc_client_reset_cnfg(zbc_client_state_t *state) {
  if (state) {
    state->cnfg_sent = 0;
  }
}

/*========================================================================
 * Buffer Size Calculation
 *========================================================================*/

int zbc_calc_buffer_size(const zbc_client_state_t *state, uint8_t opcode,
                         size_t write_size, size_t read_size) {
  size_t size;
  size_t int_size;
  size_t ptr_size;

  if (!state) {
    return ZBC_ERR_INVALID_ARG;
  }

  int_size = state->int_size;
  ptr_size = state->ptr_size;

  /* Base: RIFF header */
  size = ZBC_HDR_SIZE;

  /* Add CNFG if not yet sent */
  if (!state->cnfg_sent) {
    size += ZBC_CNFG_TOTAL_SIZE;
  }

  /* Add CALL header */
  size += ZBC_CALL_HDR_SIZE;

  /* Add syscall-specific chunks */
  switch (opcode) {
  case SH_SYS_OPEN:
    /* DATA(filename) + PARM(mode) + PARM(len) */
    size += ZBC_DATA_HDR_SIZE + ZBC_PAD_SIZE(write_size);
    size += ZBC_PARM_HDR_SIZE + int_size;
    size += ZBC_PARM_HDR_SIZE + int_size;
    break;

  case SH_SYS_CLOSE:
  case SH_SYS_FLEN:
  case SH_SYS_ISTTY:
    /* PARM(fd) */
    size += ZBC_PARM_HDR_SIZE + int_size;
    break;

  case SH_SYS_WRITEC:
    /* DATA(1 byte) */
    size += ZBC_DATA_HDR_SIZE + ZBC_PAD_SIZE(1);
    break;

  case SH_SYS_WRITE0:
    /* DATA(string with null) */
    size += ZBC_DATA_HDR_SIZE + ZBC_PAD_SIZE(write_size);
    break;

  case SH_SYS_WRITE:
    /* PARM(fd) + DATA(buffer) + PARM(len) */
    size += ZBC_PARM_HDR_SIZE + int_size;
    size += ZBC_DATA_HDR_SIZE + ZBC_PAD_SIZE(write_size);
    size += ZBC_PARM_HDR_SIZE + int_size;
    break;

  case SH_SYS_READ:
    /* PARM(fd) + PARM(len) */
    /* Response will have DATA chunk, need space for it */
    size += ZBC_PARM_HDR_SIZE + int_size;
    size += ZBC_PARM_HDR_SIZE + int_size;
    /* Reserve space for RETN with DATA */
    size += ZBC_RETN_HDR_SIZE + int_size + 4; /* result + errno */
    size += ZBC_DATA_HDR_SIZE + ZBC_PAD_SIZE(read_size);
    break;

  case SH_SYS_READC:
  case SH_SYS_CLOCK:
  case SH_SYS_TIME:
  case SH_SYS_ERRNO:
  case SH_SYS_TICKFREQ:
    /* No parameters */
    break;

  case SH_SYS_ISERROR:
    /* PARM(status) */
    size += ZBC_PARM_HDR_SIZE + int_size;
    break;

  case SH_SYS_SEEK:
    /* PARM(fd) + PARM(pos) */
    size += ZBC_PARM_HDR_SIZE + int_size;
    size += ZBC_PARM_HDR_SIZE + int_size;
    break;

  case SH_SYS_TMPNAM:
    /* PARM(id) + PARM(maxpath) */
    size += ZBC_PARM_HDR_SIZE + int_size;
    size += ZBC_PARM_HDR_SIZE + int_size;
    /* Response has DATA(path) */
    size += ZBC_RETN_HDR_SIZE + int_size + 4;
    size += ZBC_DATA_HDR_SIZE + ZBC_PAD_SIZE(read_size);
    break;

  case SH_SYS_REMOVE:
    /* DATA(filename) + PARM(len) */
    size += ZBC_DATA_HDR_SIZE + ZBC_PAD_SIZE(write_size);
    size += ZBC_PARM_HDR_SIZE + int_size;
    break;

  case SH_SYS_RENAME:
    /* DATA(old) + PARM(old_len) + DATA(new) + PARM(new_len) */
    /* write_size is sum of both strings */
    size += ZBC_DATA_HDR_SIZE + ZBC_PAD_SIZE(write_size / 2);
    size += ZBC_PARM_HDR_SIZE + int_size;
    size += ZBC_DATA_HDR_SIZE + ZBC_PAD_SIZE(write_size / 2);
    size += ZBC_PARM_HDR_SIZE + int_size;
    break;

  case SH_SYS_SYSTEM:
    /* DATA(command) + PARM(len) */
    size += ZBC_DATA_HDR_SIZE + ZBC_PAD_SIZE(write_size);
    size += ZBC_PARM_HDR_SIZE + int_size;
    break;

  case SH_SYS_GET_CMDLINE:
    /* PARM(max_size) */
    size += ZBC_PARM_HDR_SIZE + int_size;
    /* Response has DATA(cmdline) */
    size += ZBC_RETN_HDR_SIZE + int_size + 4;
    size += ZBC_DATA_HDR_SIZE + ZBC_PAD_SIZE(read_size);
    break;

  case SH_SYS_HEAPINFO:
    /* No parameters, response has DATA(4 pointers) */
    size += ZBC_RETN_HDR_SIZE + int_size + 4;
    size += ZBC_DATA_HDR_SIZE + ZBC_PAD_SIZE(ptr_size * 4);
    break;

  case SH_SYS_EXIT:
  case SH_SYS_EXIT_EXTENDED:
    /* PARM(exception) + PARM(subcode) */
    size += ZBC_PARM_HDR_SIZE + int_size;
    size += ZBC_PARM_HDR_SIZE + int_size;
    break;

  case SH_SYS_ELAPSED:
    /* No parameters, response may have DATA(8 bytes) */
    if (int_size < 8) {
      size += ZBC_RETN_HDR_SIZE + int_size + 4;
      size += ZBC_DATA_HDR_SIZE + ZBC_PAD_SIZE(8);
    }
    break;

  default:
    return ZBC_ERR_INVALID_ARG;
  }

  /* Minimum space for RETN if not already accounted */
  if (size < ZBC_HDR_SIZE + ZBC_RETN_HDR_SIZE + int_size + 4) {
    size = ZBC_HDR_SIZE + ZBC_RETN_HDR_SIZE + int_size + 4 + 64;
  }

  return (int)size;
}

/*========================================================================
 * Builder Functions
 *========================================================================*/

int zbc_builder_start(zbc_builder_t *builder, uint8_t *buf, size_t capacity,
                      zbc_client_state_t *state) {
  size_t needed;

  if (!builder || !buf || !state) {
    if (builder)
      builder->error = ZBC_ERR_INVALID_ARG;
    return ZBC_ERR_INVALID_ARG;
  }

  builder->buf = buf;
  builder->capacity = capacity;
  builder->offset = 0;
  builder->call_offset = 0;
  builder->error = ZBC_OK;

  /* Calculate minimum needed: RIFF header + CNFG (maybe) */
  needed = ZBC_HDR_SIZE;
  if (!state->cnfg_sent) {
    needed += ZBC_CNFG_TOTAL_SIZE;
  }

  if (capacity < needed) {
    builder->error = ZBC_ERR_BUFFER_TOO_SMALL;
    return ZBC_ERR_BUFFER_TOO_SMALL;
  }

  /* Write RIFF header: 'RIFF' + size(placeholder) + 'SEMI' */
  ZBC_WRITE_FOURCC(buf, 'R', 'I', 'F', 'F');
  ZBC_WRITE_U32_LE(buf + 4, 0); /* Size placeholder, patched in finish */
  ZBC_WRITE_FOURCC(buf + 8, 'S', 'E', 'M', 'I');
  builder->offset = ZBC_HDR_SIZE;

  /* Write CNFG chunk if not yet sent */
  if (!state->cnfg_sent) {
    ZBC_WRITE_FOURCC(buf + builder->offset, 'C', 'N', 'F', 'G');
    ZBC_WRITE_U32_LE(buf + builder->offset + 4, ZBC_CNFG_DATA_SIZE);
    buf[builder->offset + 8] = state->int_size;
    buf[builder->offset + 9] = state->ptr_size;
    buf[builder->offset + 10] = state->endianness;
    buf[builder->offset + 11] = 0; /* Reserved */
    builder->offset += ZBC_CNFG_TOTAL_SIZE;
    state->cnfg_sent = 1;
  }

  return ZBC_OK;
}

int zbc_builder_begin_call(zbc_builder_t *builder, uint8_t opcode) {
  size_t needed;
  uint8_t *p;

  if (!builder)
    return ZBC_ERR_INVALID_ARG;
  if (builder->error)
    return builder->error;

  needed = ZBC_CALL_HDR_SIZE;
  if (builder->offset + needed > builder->capacity) {
    builder->error = ZBC_ERR_BUFFER_TOO_SMALL;
    return ZBC_ERR_BUFFER_TOO_SMALL;
  }

  builder->call_offset = builder->offset;
  p = builder->buf + builder->offset;

  /* Write CALL chunk header: 'CALL' + size(placeholder) + opcode + reserved */
  ZBC_WRITE_FOURCC(p, 'C', 'A', 'L', 'L');
  ZBC_WRITE_U32_LE(p + 4, 0); /* Size placeholder */
  p[8] = opcode;
  p[9] = 0; /* Reserved */
  p[10] = 0;
  p[11] = 0;

  builder->offset += ZBC_CALL_HDR_SIZE;
  return ZBC_OK;
}

int zbc_builder_add_parm_int(zbc_builder_t *builder, long value) {
  size_t parm_data_size;
  size_t needed;
  uint8_t *p;

  if (!builder)
    return ZBC_ERR_INVALID_ARG;
  if (builder->error)
    return builder->error;

  parm_data_size = 4 + ZBC_CLIENT_INT_SIZE; /* type(1) + reserved(3) + value */
  needed = ZBC_CHUNK_HDR_SIZE + parm_data_size;

  if (builder->offset + needed > builder->capacity) {
    builder->error = ZBC_ERR_BUFFER_TOO_SMALL;
    return ZBC_ERR_BUFFER_TOO_SMALL;
  }

  p = builder->buf + builder->offset;

  /* Write PARM chunk */
  ZBC_WRITE_FOURCC(p, 'P', 'A', 'R', 'M');
  ZBC_WRITE_U32_LE(p + 4, (uint32_t)parm_data_size);
  p[8] = ZBC_PARM_TYPE_INT;
  p[9] = 0; /* Reserved */
  p[10] = 0;
  p[11] = 0;

  /* Write value in native endianness */
  write_native_uint(p + 12, (unsigned long)value, ZBC_CLIENT_INT_SIZE);

  builder->offset += ZBC_CHUNK_HDR_SIZE + parm_data_size;
  return ZBC_OK;
}

int zbc_builder_add_parm_uint(zbc_builder_t *builder, unsigned long value) {
  size_t parm_data_size;
  size_t needed;
  uint8_t *p;

  if (!builder)
    return ZBC_ERR_INVALID_ARG;
  if (builder->error)
    return builder->error;

  parm_data_size = 4 + ZBC_CLIENT_INT_SIZE;
  needed = ZBC_CHUNK_HDR_SIZE + parm_data_size;

  if (builder->offset + needed > builder->capacity) {
    builder->error = ZBC_ERR_BUFFER_TOO_SMALL;
    return ZBC_ERR_BUFFER_TOO_SMALL;
  }

  p = builder->buf + builder->offset;

  ZBC_WRITE_FOURCC(p, 'P', 'A', 'R', 'M');
  ZBC_WRITE_U32_LE(p + 4, (uint32_t)parm_data_size);
  p[8] = ZBC_PARM_TYPE_INT;
  p[9] = 0;
  p[10] = 0;
  p[11] = 0;

  write_native_uint(p + 12, value, ZBC_CLIENT_INT_SIZE);

  builder->offset += ZBC_CHUNK_HDR_SIZE + parm_data_size;
  return ZBC_OK;
}

int zbc_builder_add_data_binary(zbc_builder_t *builder, const void *data,
                                size_t size) {
  size_t data_chunk_size;
  size_t padded_size;
  size_t needed;
  uint8_t *p;
  const uint8_t *src;
  size_t i;

  if (!builder)
    return ZBC_ERR_INVALID_ARG;
  if (builder->error)
    return builder->error;
  if (size > 0 && !data) {
    builder->error = ZBC_ERR_INVALID_ARG;
    return ZBC_ERR_INVALID_ARG;
  }

  data_chunk_size = 4 + size; /* type(1) + reserved(3) + payload */
  padded_size = ZBC_PAD_SIZE(data_chunk_size);
  needed = ZBC_CHUNK_HDR_SIZE + padded_size;

  if (builder->offset + needed > builder->capacity) {
    builder->error = ZBC_ERR_BUFFER_TOO_SMALL;
    return ZBC_ERR_BUFFER_TOO_SMALL;
  }

  p = builder->buf + builder->offset;

  /* Write DATA chunk header */
  ZBC_WRITE_FOURCC(p, 'D', 'A', 'T', 'A');
  ZBC_WRITE_U32_LE(p + 4, (uint32_t)data_chunk_size);
  p[8] = ZBC_DATA_TYPE_BINARY;
  p[9] = 0; /* Reserved */
  p[10] = 0;
  p[11] = 0;

  /* Copy payload */
  src = (const uint8_t *)data;
  for (i = 0; i < size; i++) {
    p[12 + i] = src[i];
  }

  /* Pad if odd size */
  if (data_chunk_size & 1) {
    p[12 + size] = 0;
  }

  builder->offset += ZBC_CHUNK_HDR_SIZE + padded_size;
  return ZBC_OK;
}

int zbc_builder_add_data_string(zbc_builder_t *builder, const char *str) {
  size_t len;
  size_t data_chunk_size;
  size_t padded_size;
  size_t needed;
  uint8_t *p;
  size_t i;

  if (!builder)
    return ZBC_ERR_INVALID_ARG;
  if (builder->error)
    return builder->error;
  if (!str) {
    builder->error = ZBC_ERR_INVALID_ARG;
    return ZBC_ERR_INVALID_ARG;
  }

  len = zbc_strlen(str) + 1; /* Include null terminator */
  data_chunk_size = 4 + len;
  padded_size = ZBC_PAD_SIZE(data_chunk_size);
  needed = ZBC_CHUNK_HDR_SIZE + padded_size;

  if (builder->offset + needed > builder->capacity) {
    builder->error = ZBC_ERR_BUFFER_TOO_SMALL;
    return ZBC_ERR_BUFFER_TOO_SMALL;
  }

  p = builder->buf + builder->offset;

  /* Write DATA chunk header */
  ZBC_WRITE_FOURCC(p, 'D', 'A', 'T', 'A');
  ZBC_WRITE_U32_LE(p + 4, (uint32_t)data_chunk_size);
  p[8] = ZBC_DATA_TYPE_STRING;
  p[9] = 0;
  p[10] = 0;
  p[11] = 0;

  /* Copy string including null terminator */
  for (i = 0; i < len; i++) {
    p[12 + i] = (uint8_t)str[i];
  }

  /* Pad if odd size */
  if (data_chunk_size & 1) {
    p[12 + len] = 0;
  }

  builder->offset += ZBC_CHUNK_HDR_SIZE + padded_size;
  return ZBC_OK;
}

int zbc_builder_finish(zbc_builder_t *builder, size_t *out_size) {
  uint32_t call_size;
  uint32_t riff_size;

  if (!builder)
    return ZBC_ERR_INVALID_ARG;
  if (builder->error)
    return builder->error;

  /* Patch CALL chunk size */
  call_size =
      (uint32_t)(builder->offset - builder->call_offset - ZBC_CHUNK_HDR_SIZE);
  ZBC_WRITE_U32_LE(builder->buf + builder->call_offset + 4, call_size);

  /* Patch RIFF container size (total - 8 for 'RIFF' + size field) */
  riff_size = (uint32_t)(builder->offset - 8);
  ZBC_WRITE_U32_LE(builder->buf + 4, riff_size);

  if (out_size) {
    *out_size = builder->offset;
  }

  return ZBC_OK;
}

/*========================================================================
 * Device Communication
 *========================================================================*/

int zbc_client_submit_poll(zbc_client_state_t *state, void *buf, size_t size) {
  volatile uint8_t *dev;
  uintptr_t addr;
  size_t i;
  uint8_t status;

  (void)size; /* Currently unused, could add size validation */

  if (!state || !state->dev_base || !buf) {
    return ZBC_ERR_INVALID_ARG;
  }

  dev = state->dev_base;
  addr = (uintptr_t)buf;

  /* Write buffer address to RIFF_PTR register (native byte order) */
  /* The register is 16 bytes to accommodate any pointer size */
  for (i = 0; i < ZBC_CLIENT_PTR_SIZE && i < 16; i++) {
#if ZBC_CLIENT_ENDIANNESS == ZBC_ENDIAN_LITTLE
    dev[ZBC_REG_RIFF_PTR + i] = (uint8_t)(addr & 0xFFU);
    addr >>= 8;
#elif ZBC_CLIENT_ENDIANNESS == ZBC_ENDIAN_BIG
    dev[ZBC_REG_RIFF_PTR + ZBC_CLIENT_PTR_SIZE - 1 - i] =
        (uint8_t)(addr & 0xFFU);
    addr >>= 8;
#else
    /* PDP: handle specially if needed */
    dev[ZBC_REG_RIFF_PTR + i] = (uint8_t)(addr & 0xFFU);
    addr >>= 8;
#endif
  }

  /* Clear remaining bytes of RIFF_PTR if pointer is smaller than 16 bytes */
  for (; i < 16; i++) {
    dev[ZBC_REG_RIFF_PTR + i] = 0;
  }

  /* Memory barrier - compiler fence at minimum */
#if defined(__GNUC__) || defined(__clang__)
  __asm__ volatile("" ::: "memory");
#endif

  /* Trigger the request */
  dev[ZBC_REG_DOORBELL] = 0x01;

  /* Another barrier after write */
#if defined(__GNUC__) || defined(__clang__)
  __asm__ volatile("" ::: "memory");
#endif

  /* Poll STATUS register until RESPONSE_READY */
  do {
    status = dev[ZBC_REG_STATUS];
  } while (!(status & ZBC_STATUS_RESPONSE_READY));

  return ZBC_OK;
}

/*========================================================================
 * Response Parsing
 *========================================================================*/

int zbc_parse_response(zbc_response_t *response, const uint8_t *buf,
                       size_t capacity, const zbc_client_state_t *state) {
  uint32_t chunk_id;
  uint32_t chunk_size;
  size_t offset;
  size_t int_size;
  const uint8_t *retn_data;

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

  /* Verify RIFF header */
  if (capacity < ZBC_HDR_SIZE) {
    return ZBC_ERR_PARSE_ERROR;
  }

  chunk_id = ZBC_READ_U32_LE(buf);
  if (chunk_id != ZBC_ID_RIFF) {
    return ZBC_ERR_PARSE_ERROR;
  }

  chunk_id = ZBC_READ_U32_LE(buf + 8);
  if (chunk_id != ZBC_ID_SEMI) {
    return ZBC_ERR_PARSE_ERROR;
  }

  /* Skip to after RIFF header, then skip CNFG if present */
  offset = ZBC_HDR_SIZE;

  if (offset + ZBC_CHUNK_HDR_SIZE > capacity) {
    return ZBC_ERR_PARSE_ERROR;
  }

  chunk_id = ZBC_READ_U32_LE(buf + offset);

  /* Skip CNFG chunk if present */
  if (chunk_id == ZBC_ID_CNFG) {
    chunk_size = ZBC_READ_U32_LE(buf + offset + 4);
    offset += ZBC_CHUNK_HDR_SIZE + ZBC_PAD_SIZE(chunk_size);
  }

  /* Now we should see RETN or ERRO */
  if (offset + ZBC_CHUNK_HDR_SIZE > capacity) {
    return ZBC_ERR_PARSE_ERROR;
  }

  chunk_id = ZBC_READ_U32_LE(buf + offset);
  chunk_size = ZBC_READ_U32_LE(buf + offset + 4);

  if (chunk_id == ZBC_ID_ERRO) {
    /* Error response */
    response->is_error = 1;
    if (offset + ZBC_CHUNK_HDR_SIZE + 2 <= capacity) {
      response->proto_error = ZBC_READ_U16_LE(buf + offset + 8);
    }
    return ZBC_OK;
  }

  if (chunk_id != ZBC_ID_RETN) {
    return ZBC_ERR_PARSE_ERROR;
  }

  /* Parse RETN chunk */
  /* Format: result (int_size bytes, native) + errno (4 bytes, LE) + optional
   * DATA */
  if (offset + ZBC_CHUNK_HDR_SIZE + int_size + 4 > capacity) {
    return ZBC_ERR_PARSE_ERROR;
  }

  retn_data = buf + offset + ZBC_CHUNK_HDR_SIZE;

  /* Read result in native endianness */
  response->result = read_native_int(retn_data, int_size);

  /* Read errno (always little-endian per spec) */
  response->error_code = (int)ZBC_READ_U32_LE(retn_data + int_size);

  /* Check for DATA sub-chunk */
  offset += ZBC_CHUNK_HDR_SIZE + int_size + 4;

  if (offset + ZBC_CHUNK_HDR_SIZE <= capacity) {
    chunk_id = ZBC_READ_U32_LE(buf + offset);
    if (chunk_id == ZBC_ID_DATA) {
      chunk_size = ZBC_READ_U32_LE(buf + offset + 4);
      /* DATA chunk has 4-byte header (type + reserved) before payload */
      if (offset + ZBC_CHUNK_HDR_SIZE + 4 + (chunk_size - 4) <= capacity) {
        response->data = buf + offset + ZBC_CHUNK_HDR_SIZE + 4;
        response->data_size = chunk_size - 4;
      }
    }
  }

  return ZBC_OK;
}
