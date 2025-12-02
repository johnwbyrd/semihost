/*
 * ZBC Semihosting RIFF Helpers
 *
 * Shared functions for reading and writing RIFF chunks.
 * Used by both client and host libraries.
 */

/*
 * This file is compiled into both client and host libraries.
 * ZBC_CLIENT or ZBC_HOST is defined via CMakeLists.txt.
 */
#include "zbc_semihost.h"

/*========================================================================
 * String length helper (no libc dependency)
 *========================================================================*/

size_t zbc_strlen(const char *s)
{
    size_t len = 0;
    if (s) {
        while (*s++) {
            len++;
        }
    }
    return len;
}

/*========================================================================
 * Native endianness read/write helpers
 *========================================================================*/

/*
 * Write an unsigned integer in native endianness.
 * Used for PARM/DATA values that are in guest's native format.
 */
void zbc_write_native_uint(uint8_t *buf, unsigned int value, int size,
                           int endianness)
{
    int i;

    if (endianness == ZBC_ENDIAN_LITTLE) {
        for (i = 0; i < size; i++) {
            buf[i] = (uint8_t)(value & 0xFFU);
            value >>= 8;
        }
    } else {
        /* Big endian */
        for (i = size - 1; i >= 0; i--) {
            buf[i] = (uint8_t)(value & 0xFFU);
            value >>= 8;
        }
    }
}

/*
 * Read a signed integer in native endianness with sign extension.
 */
int zbc_read_native_int(const uint8_t *buf, int size, int endianness)
{
    unsigned int value = 0;
    int i;
    unsigned int sign_bit;
    unsigned int sign_extend;

    if (endianness == ZBC_ENDIAN_LITTLE) {
        for (i = size - 1; i >= 0; i--) {
            value = (value << 8) | buf[i];
        }
    } else {
        /* Big endian */
        for (i = 0; i < size; i++) {
            value = (value << 8) | buf[i];
        }
    }

    /* Sign extend if necessary */
    if (size < (int)sizeof(int)) {
        sign_bit = 1U << (size * 8 - 1);
        if (value & sign_bit) {
            sign_extend = ~((1U << (size * 8)) - 1);
            value |= sign_extend;
        }
    }

    return (int)value;
}

/*
 * Read an unsigned integer in native endianness.
 */
unsigned int zbc_read_native_uint(const uint8_t *buf, int size, int endianness)
{
    unsigned int value = 0;
    int i;

    if (endianness == ZBC_ENDIAN_LITTLE) {
        for (i = size - 1; i >= 0; i--) {
            value = (value << 8) | buf[i];
        }
    } else {
        /* Big endian */
        for (i = 0; i < size; i++) {
            value = (value << 8) | buf[i];
        }
    }

    return value;
}

/*========================================================================
 * RIFF chunk writing helpers
 *========================================================================*/

/*
 * Write a chunk header (FourCC + size placeholder).
 * Returns pointer to size field for later patching.
 */
uint8_t *zbc_riff_begin_chunk(uint8_t *buf, size_t capacity, size_t *offset,
                              uint32_t fourcc)
{
    uint8_t *size_ptr;

    if (*offset + ZBC_CHUNK_HDR_SIZE > capacity) {
        return (uint8_t *)0;  /* Buffer overflow */
    }

    ZBC_WRITE_U32_LE(buf + *offset, fourcc);
    size_ptr = buf + *offset + 4;
    ZBC_WRITE_U32_LE(size_ptr, 0);  /* Placeholder */
    *offset += ZBC_CHUNK_HDR_SIZE;

    return size_ptr;
}

/*
 * Patch a chunk size field.
 * data_size is the size of chunk data (not including header).
 */
void zbc_riff_patch_size(uint8_t *size_ptr, size_t data_size)
{
    ZBC_WRITE_U32_LE(size_ptr, (uint32_t)data_size);
}

/*
 * Write bytes to buffer with bounds checking.
 * Returns 0 on success, -1 on overflow.
 */
int zbc_riff_write_bytes(uint8_t *buf, size_t capacity, size_t *offset,
                         const void *data, size_t size)
{
    size_t i;
    const uint8_t *src;

    if (*offset + size > capacity) {
        return -1;
    }

    src = (const uint8_t *)data;
    for (i = 0; i < size; i++) {
        buf[*offset + i] = src[i];
    }
    *offset += size;

    return 0;
}

/*
 * Write padding byte if needed (RIFF chunks are word-aligned).
 */
void zbc_riff_pad(uint8_t *buf, size_t capacity, size_t *offset)
{
    if ((*offset & 1) && *offset < capacity) {
        buf[*offset] = 0;
        (*offset)++;
    }
}

/*========================================================================
 * RIFF chunk reading helpers
 *========================================================================*/

/*
 * Read a chunk header.
 * Returns 0 on success, fills fourcc and size.
 * Returns -1 if not enough data.
 */
int zbc_riff_read_header(const uint8_t *buf, size_t capacity, size_t offset,
                         uint32_t *fourcc, uint32_t *size)
{
    if (offset + ZBC_CHUNK_HDR_SIZE > capacity) {
        return -1;
    }

    *fourcc = ZBC_READ_U32_LE(buf + offset);
    *size = ZBC_READ_U32_LE(buf + offset + 4);

    return 0;
}

/*
 * Skip over a chunk (header + data + padding).
 * Returns new offset, or 0 on error.
 */
size_t zbc_riff_skip_chunk(const uint8_t *buf, size_t capacity, size_t offset)
{
    uint32_t fourcc, size;

    if (zbc_riff_read_header(buf, capacity, offset, &fourcc, &size) < 0) {
        return 0;
    }

    offset += ZBC_CHUNK_HDR_SIZE + ZBC_PAD_SIZE(size);

    if (offset > capacity) {
        return 0;
    }

    return offset;
}

/*========================================================================
 * RIFF container helpers
 *========================================================================*/

/*
 * Write RIFF container header.
 * Returns pointer to size field for later patching.
 */
uint8_t *zbc_riff_begin_container(uint8_t *buf, size_t capacity, size_t *offset,
                                  uint32_t form_type)
{
    uint8_t *size_ptr;

    if (capacity < ZBC_HDR_SIZE) {
        return (uint8_t *)0;
    }

    ZBC_WRITE_U32_LE(buf, ZBC_ID_RIFF);
    size_ptr = buf + 4;
    ZBC_WRITE_U32_LE(size_ptr, 0);  /* Placeholder */
    ZBC_WRITE_U32_LE(buf + 8, form_type);
    *offset = ZBC_HDR_SIZE;

    return size_ptr;
}

/*
 * Validate RIFF container header.
 * Returns 0 on success, -1 on error.
 */
int zbc_riff_validate_container(const uint8_t *buf, size_t capacity,
                                uint32_t expected_form_type)
{
    uint32_t id, form_type;

    if (capacity < ZBC_HDR_SIZE) {
        return -1;
    }

    id = ZBC_READ_U32_LE(buf);
    if (id != ZBC_ID_RIFF) {
        return -1;
    }

    form_type = ZBC_READ_U32_LE(buf + 8);
    if (form_type != expected_form_type) {
        return -1;
    }

    return 0;
}
