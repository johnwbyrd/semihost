/*
 * ZBC Semihosting Client - High-Level Syscall Functions
 *
 * Each function builds a RIFF request, submits it, and parses the response.
 */

#include "zbc_semi_client.h"

/*------------------------------------------------------------------------
 * String length helper
 *------------------------------------------------------------------------*/

static size_t zbc_strlen(const char *s)
{
    size_t len = 0;
    if (s) {
        while (*s++) len++;
    }
    return len;
}

/*------------------------------------------------------------------------
 * Internal helper: execute simple syscall with no input parameters
 *------------------------------------------------------------------------*/

static int exec_syscall_noargs(zbc_client_state_t *state,
                               void *buf, size_t buf_size,
                               uint8_t opcode,
                               zbc_response_t *response)
{
    zbc_builder_t builder;
    size_t riff_size;
    int rc;

    rc = zbc_builder_start(&builder, (uint8_t *)buf, buf_size, state);
    if (rc < 0) return rc;

    rc = zbc_builder_begin_call(&builder, opcode);
    if (rc < 0) return rc;

    rc = zbc_builder_finish(&builder, &riff_size);
    if (rc < 0) return rc;

    rc = zbc_client_submit_poll(state, buf, riff_size);
    if (rc < 0) return rc;

    rc = zbc_parse_response(response, (const uint8_t *)buf, buf_size, state);
    if (rc < 0) return rc;

    if (response->is_error) {
        return ZBC_ERR_DEVICE_ERROR;
    }

    return ZBC_OK;
}

/*------------------------------------------------------------------------
 * Internal helper: execute syscall with single int parameter
 *------------------------------------------------------------------------*/

static int exec_syscall_1int(zbc_client_state_t *state,
                             void *buf, size_t buf_size,
                             uint8_t opcode,
                             long arg,
                             zbc_response_t *response)
{
    zbc_builder_t builder;
    size_t riff_size;
    int rc;

    rc = zbc_builder_start(&builder, (uint8_t *)buf, buf_size, state);
    if (rc < 0) return rc;

    rc = zbc_builder_begin_call(&builder, opcode);
    if (rc < 0) return rc;

    rc = zbc_builder_add_parm_int(&builder, arg);
    if (rc < 0) return rc;

    rc = zbc_builder_finish(&builder, &riff_size);
    if (rc < 0) return rc;

    rc = zbc_client_submit_poll(state, buf, riff_size);
    if (rc < 0) return rc;

    rc = zbc_parse_response(response, (const uint8_t *)buf, buf_size, state);
    if (rc < 0) return rc;

    if (response->is_error) {
        return ZBC_ERR_DEVICE_ERROR;
    }

    return ZBC_OK;
}

/*========================================================================
 * File Operations
 *========================================================================*/

int zbc_sys_open(zbc_client_state_t *state,
                 void *buf, size_t buf_size,
                 const char *pathname, int mode)
{
    zbc_builder_t builder;
    zbc_response_t response;
    size_t riff_size;
    size_t pathlen;
    int rc;

    if (!pathname) return ZBC_ERR_INVALID_ARG;

    pathlen = zbc_strlen(pathname);

    rc = zbc_builder_start(&builder, (uint8_t *)buf, buf_size, state);
    if (rc < 0) return rc;

    rc = zbc_builder_begin_call(&builder, SH_SYS_OPEN);
    if (rc < 0) return rc;

    /* Per spec: DATA(filename), PARM(mode), PARM(len) */
    rc = zbc_builder_add_data_string(&builder, pathname);
    if (rc < 0) return rc;

    rc = zbc_builder_add_parm_int(&builder, mode);
    if (rc < 0) return rc;

    rc = zbc_builder_add_parm_uint(&builder, (unsigned long)pathlen);
    if (rc < 0) return rc;

    rc = zbc_builder_finish(&builder, &riff_size);
    if (rc < 0) return rc;

    rc = zbc_client_submit_poll(state, buf, riff_size);
    if (rc < 0) return rc;

    rc = zbc_parse_response(&response, (const uint8_t *)buf, buf_size, state);
    if (rc < 0) return rc;

    if (response.is_error) {
        return ZBC_ERR_DEVICE_ERROR;
    }

    return (int)response.result;
}

int zbc_sys_close(zbc_client_state_t *state,
                  void *buf, size_t buf_size,
                  int fd)
{
    zbc_response_t response;
    int rc;

    rc = exec_syscall_1int(state, buf, buf_size, SH_SYS_CLOSE, fd, &response);
    if (rc < 0) return rc;

    return (int)response.result;
}

long zbc_sys_read(zbc_client_state_t *state,
                  void *buf, size_t buf_size,
                  int fd, void *dest, size_t count)
{
    zbc_builder_t builder;
    zbc_response_t response;
    size_t riff_size;
    size_t i;
    int rc;

    if (!dest && count > 0) return ZBC_ERR_INVALID_ARG;

    rc = zbc_builder_start(&builder, (uint8_t *)buf, buf_size, state);
    if (rc < 0) return rc;

    rc = zbc_builder_begin_call(&builder, SH_SYS_READ);
    if (rc < 0) return rc;

    /* Per spec: PARM(fd), PARM(len) */
    rc = zbc_builder_add_parm_int(&builder, fd);
    if (rc < 0) return rc;

    rc = zbc_builder_add_parm_uint(&builder, (unsigned long)count);
    if (rc < 0) return rc;

    rc = zbc_builder_finish(&builder, &riff_size);
    if (rc < 0) return rc;

    rc = zbc_client_submit_poll(state, buf, riff_size);
    if (rc < 0) return rc;

    rc = zbc_parse_response(&response, (const uint8_t *)buf, buf_size, state);
    if (rc < 0) return rc;

    if (response.is_error) {
        return ZBC_ERR_DEVICE_ERROR;
    }

    /* Copy data from response DATA chunk to user buffer */
    if (response.data && response.data_size > 0) {
        size_t copy_size = response.data_size;
        if (copy_size > count) copy_size = count;

        for (i = 0; i < copy_size; i++) {
            ((uint8_t *)dest)[i] = response.data[i];
        }
    }

    return response.result;
}

long zbc_sys_write(zbc_client_state_t *state,
                   void *buf, size_t buf_size,
                   int fd, const void *src, size_t count)
{
    zbc_builder_t builder;
    zbc_response_t response;
    size_t riff_size;
    int rc;

    if (!src && count > 0) return ZBC_ERR_INVALID_ARG;

    rc = zbc_builder_start(&builder, (uint8_t *)buf, buf_size, state);
    if (rc < 0) return rc;

    rc = zbc_builder_begin_call(&builder, SH_SYS_WRITE);
    if (rc < 0) return rc;

    /* Per spec: PARM(fd), DATA(buffer), PARM(len) */
    rc = zbc_builder_add_parm_int(&builder, fd);
    if (rc < 0) return rc;

    rc = zbc_builder_add_data_binary(&builder, src, count);
    if (rc < 0) return rc;

    rc = zbc_builder_add_parm_uint(&builder, (unsigned long)count);
    if (rc < 0) return rc;

    rc = zbc_builder_finish(&builder, &riff_size);
    if (rc < 0) return rc;

    rc = zbc_client_submit_poll(state, buf, riff_size);
    if (rc < 0) return rc;

    rc = zbc_parse_response(&response, (const uint8_t *)buf, buf_size, state);
    if (rc < 0) return rc;

    if (response.is_error) {
        return ZBC_ERR_DEVICE_ERROR;
    }

    return response.result;
}

int zbc_sys_seek(zbc_client_state_t *state,
                 void *buf, size_t buf_size,
                 int fd, unsigned long pos)
{
    zbc_builder_t builder;
    zbc_response_t response;
    size_t riff_size;
    int rc;

    rc = zbc_builder_start(&builder, (uint8_t *)buf, buf_size, state);
    if (rc < 0) return rc;

    rc = zbc_builder_begin_call(&builder, SH_SYS_SEEK);
    if (rc < 0) return rc;

    /* Per spec: PARM(fd), PARM(pos) */
    rc = zbc_builder_add_parm_int(&builder, fd);
    if (rc < 0) return rc;

    rc = zbc_builder_add_parm_uint(&builder, pos);
    if (rc < 0) return rc;

    rc = zbc_builder_finish(&builder, &riff_size);
    if (rc < 0) return rc;

    rc = zbc_client_submit_poll(state, buf, riff_size);
    if (rc < 0) return rc;

    rc = zbc_parse_response(&response, (const uint8_t *)buf, buf_size, state);
    if (rc < 0) return rc;

    if (response.is_error) {
        return ZBC_ERR_DEVICE_ERROR;
    }

    return (int)response.result;
}

long zbc_sys_flen(zbc_client_state_t *state,
                  void *buf, size_t buf_size,
                  int fd)
{
    zbc_response_t response;
    int rc;

    rc = exec_syscall_1int(state, buf, buf_size, SH_SYS_FLEN, fd, &response);
    if (rc < 0) return rc;

    return response.result;
}

/*========================================================================
 * Character I/O
 *========================================================================*/

int zbc_sys_writec(zbc_client_state_t *state,
                   void *buf, size_t buf_size,
                   char c)
{
    zbc_builder_t builder;
    zbc_response_t response;
    size_t riff_size;
    uint8_t byte;
    int rc;

    rc = zbc_builder_start(&builder, (uint8_t *)buf, buf_size, state);
    if (rc < 0) return rc;

    rc = zbc_builder_begin_call(&builder, SH_SYS_WRITEC);
    if (rc < 0) return rc;

    /* Per spec: DATA(1 byte) */
    byte = (uint8_t)c;
    rc = zbc_builder_add_data_binary(&builder, &byte, 1);
    if (rc < 0) return rc;

    rc = zbc_builder_finish(&builder, &riff_size);
    if (rc < 0) return rc;

    rc = zbc_client_submit_poll(state, buf, riff_size);
    if (rc < 0) return rc;

    rc = zbc_parse_response(&response, (const uint8_t *)buf, buf_size, state);
    if (rc < 0) return rc;

    if (response.is_error) {
        return ZBC_ERR_DEVICE_ERROR;
    }

    return (int)response.result;
}

int zbc_sys_write0(zbc_client_state_t *state,
                   void *buf, size_t buf_size,
                   const char *str)
{
    zbc_builder_t builder;
    zbc_response_t response;
    size_t riff_size;
    int rc;

    if (!str) return ZBC_ERR_INVALID_ARG;

    rc = zbc_builder_start(&builder, (uint8_t *)buf, buf_size, state);
    if (rc < 0) return rc;

    rc = zbc_builder_begin_call(&builder, SH_SYS_WRITE0);
    if (rc < 0) return rc;

    /* Per spec: DATA(string with null) */
    rc = zbc_builder_add_data_string(&builder, str);
    if (rc < 0) return rc;

    rc = zbc_builder_finish(&builder, &riff_size);
    if (rc < 0) return rc;

    rc = zbc_client_submit_poll(state, buf, riff_size);
    if (rc < 0) return rc;

    rc = zbc_parse_response(&response, (const uint8_t *)buf, buf_size, state);
    if (rc < 0) return rc;

    if (response.is_error) {
        return ZBC_ERR_DEVICE_ERROR;
    }

    return (int)response.result;
}

int zbc_sys_readc(zbc_client_state_t *state,
                  void *buf, size_t buf_size)
{
    zbc_response_t response;
    int rc;

    rc = exec_syscall_noargs(state, buf, buf_size, SH_SYS_READC, &response);
    if (rc < 0) return rc;

    return (int)response.result;
}

/*========================================================================
 * Status and Query Operations
 *========================================================================*/

int zbc_sys_iserror(zbc_client_state_t *state,
                    void *buf, size_t buf_size,
                    long status)
{
    zbc_response_t response;
    int rc;

    rc = exec_syscall_1int(state, buf, buf_size, SH_SYS_ISERROR, status, &response);
    if (rc < 0) return rc;

    return (int)response.result;
}

int zbc_sys_istty(zbc_client_state_t *state,
                  void *buf, size_t buf_size,
                  int fd)
{
    zbc_response_t response;
    int rc;

    rc = exec_syscall_1int(state, buf, buf_size, SH_SYS_ISTTY, fd, &response);
    if (rc < 0) return rc;

    return (int)response.result;
}

int zbc_sys_errno(zbc_client_state_t *state,
                  void *buf, size_t buf_size)
{
    zbc_response_t response;
    int rc;

    rc = exec_syscall_noargs(state, buf, buf_size, SH_SYS_ERRNO, &response);
    if (rc < 0) return rc;

    return (int)response.result;
}

/*========================================================================
 * File System Operations
 *========================================================================*/

int zbc_sys_remove(zbc_client_state_t *state,
                   void *buf, size_t buf_size,
                   const char *pathname)
{
    zbc_builder_t builder;
    zbc_response_t response;
    size_t riff_size;
    size_t pathlen;
    int rc;

    if (!pathname) return ZBC_ERR_INVALID_ARG;

    pathlen = zbc_strlen(pathname);

    rc = zbc_builder_start(&builder, (uint8_t *)buf, buf_size, state);
    if (rc < 0) return rc;

    rc = zbc_builder_begin_call(&builder, SH_SYS_REMOVE);
    if (rc < 0) return rc;

    /* Per spec: DATA(filename), PARM(len) */
    rc = zbc_builder_add_data_string(&builder, pathname);
    if (rc < 0) return rc;

    rc = zbc_builder_add_parm_uint(&builder, (unsigned long)pathlen);
    if (rc < 0) return rc;

    rc = zbc_builder_finish(&builder, &riff_size);
    if (rc < 0) return rc;

    rc = zbc_client_submit_poll(state, buf, riff_size);
    if (rc < 0) return rc;

    rc = zbc_parse_response(&response, (const uint8_t *)buf, buf_size, state);
    if (rc < 0) return rc;

    if (response.is_error) {
        return ZBC_ERR_DEVICE_ERROR;
    }

    return (int)response.result;
}

int zbc_sys_rename(zbc_client_state_t *state,
                   void *buf, size_t buf_size,
                   const char *old_path, const char *new_path)
{
    zbc_builder_t builder;
    zbc_response_t response;
    size_t riff_size;
    size_t old_len, new_len;
    int rc;

    if (!old_path || !new_path) return ZBC_ERR_INVALID_ARG;

    old_len = zbc_strlen(old_path);
    new_len = zbc_strlen(new_path);

    rc = zbc_builder_start(&builder, (uint8_t *)buf, buf_size, state);
    if (rc < 0) return rc;

    rc = zbc_builder_begin_call(&builder, SH_SYS_RENAME);
    if (rc < 0) return rc;

    /* Per spec: DATA(old), PARM(old_len), DATA(new), PARM(new_len) */
    rc = zbc_builder_add_data_string(&builder, old_path);
    if (rc < 0) return rc;

    rc = zbc_builder_add_parm_uint(&builder, (unsigned long)old_len);
    if (rc < 0) return rc;

    rc = zbc_builder_add_data_string(&builder, new_path);
    if (rc < 0) return rc;

    rc = zbc_builder_add_parm_uint(&builder, (unsigned long)new_len);
    if (rc < 0) return rc;

    rc = zbc_builder_finish(&builder, &riff_size);
    if (rc < 0) return rc;

    rc = zbc_client_submit_poll(state, buf, riff_size);
    if (rc < 0) return rc;

    rc = zbc_parse_response(&response, (const uint8_t *)buf, buf_size, state);
    if (rc < 0) return rc;

    if (response.is_error) {
        return ZBC_ERR_DEVICE_ERROR;
    }

    return (int)response.result;
}

int zbc_sys_tmpnam(zbc_client_state_t *state,
                   void *buf, size_t buf_size,
                   char *pathname, int id, int maxpath)
{
    zbc_builder_t builder;
    zbc_response_t response;
    size_t riff_size;
    size_t i;
    int rc;

    if (!pathname) return ZBC_ERR_INVALID_ARG;

    rc = zbc_builder_start(&builder, (uint8_t *)buf, buf_size, state);
    if (rc < 0) return rc;

    rc = zbc_builder_begin_call(&builder, SH_SYS_TMPNAM);
    if (rc < 0) return rc;

    /* Per spec: PARM(id), PARM(maxpath) */
    rc = zbc_builder_add_parm_int(&builder, id);
    if (rc < 0) return rc;

    rc = zbc_builder_add_parm_int(&builder, maxpath);
    if (rc < 0) return rc;

    rc = zbc_builder_finish(&builder, &riff_size);
    if (rc < 0) return rc;

    rc = zbc_client_submit_poll(state, buf, riff_size);
    if (rc < 0) return rc;

    rc = zbc_parse_response(&response, (const uint8_t *)buf, buf_size, state);
    if (rc < 0) return rc;

    if (response.is_error) {
        return ZBC_ERR_DEVICE_ERROR;
    }

    /* Copy path from DATA chunk */
    if (response.data && response.data_size > 0) {
        size_t copy_size = response.data_size;
        if (copy_size > (size_t)maxpath - 1) {
            copy_size = (size_t)maxpath - 1;
        }
        for (i = 0; i < copy_size; i++) {
            pathname[i] = (char)response.data[i];
        }
        pathname[copy_size] = '\0';
    } else {
        pathname[0] = '\0';
    }

    return (int)response.result;
}

/*========================================================================
 * Time Operations
 *========================================================================*/

unsigned long zbc_sys_clock(zbc_client_state_t *state,
                            void *buf, size_t buf_size)
{
    zbc_response_t response;
    int rc;

    rc = exec_syscall_noargs(state, buf, buf_size, SH_SYS_CLOCK, &response);
    if (rc < 0) return 0;  /* Return 0 on error */

    return (unsigned long)response.result;
}

unsigned long zbc_sys_time(zbc_client_state_t *state,
                           void *buf, size_t buf_size)
{
    zbc_response_t response;
    int rc;

    rc = exec_syscall_noargs(state, buf, buf_size, SH_SYS_TIME, &response);
    if (rc < 0) return 0;

    return (unsigned long)response.result;
}

unsigned long zbc_sys_tickfreq(zbc_client_state_t *state,
                               void *buf, size_t buf_size)
{
    zbc_response_t response;
    int rc;

    rc = exec_syscall_noargs(state, buf, buf_size, SH_SYS_TICKFREQ, &response);
    if (rc < 0) return 0;

    return (unsigned long)response.result;
}

int zbc_sys_elapsed(zbc_client_state_t *state,
                    void *buf, size_t buf_size,
                    uint32_t *low, uint32_t *high)
{
    zbc_response_t response;
    int rc;

    if (!low || !high) return ZBC_ERR_INVALID_ARG;

    rc = exec_syscall_noargs(state, buf, buf_size, SH_SYS_ELAPSED, &response);
    if (rc < 0) return rc;

    /* Check if result is in DATA chunk (for small int_size) or result field */
    if (response.data && response.data_size >= 8) {
        /* 64-bit value in DATA chunk, little-endian */
        *low = ZBC_READ_U32_LE(response.data);
        *high = ZBC_READ_U32_LE(response.data + 4);
    } else {
        /* Value in result field */
        *low = (uint32_t)(response.result & 0xFFFFFFFFUL);
        *high = (uint32_t)((uint64_t)response.result >> 32);
    }

    return ZBC_OK;
}

/*========================================================================
 * System Operations
 *========================================================================*/

int zbc_sys_system(zbc_client_state_t *state,
                   void *buf, size_t buf_size,
                   const char *command)
{
    zbc_builder_t builder;
    zbc_response_t response;
    size_t riff_size;
    size_t cmdlen;
    int rc;

    if (!command) return ZBC_ERR_INVALID_ARG;

    cmdlen = zbc_strlen(command);

    rc = zbc_builder_start(&builder, (uint8_t *)buf, buf_size, state);
    if (rc < 0) return rc;

    rc = zbc_builder_begin_call(&builder, SH_SYS_SYSTEM);
    if (rc < 0) return rc;

    /* Per spec: DATA(command), PARM(len) */
    rc = zbc_builder_add_data_string(&builder, command);
    if (rc < 0) return rc;

    rc = zbc_builder_add_parm_uint(&builder, (unsigned long)cmdlen);
    if (rc < 0) return rc;

    rc = zbc_builder_finish(&builder, &riff_size);
    if (rc < 0) return rc;

    rc = zbc_client_submit_poll(state, buf, riff_size);
    if (rc < 0) return rc;

    rc = zbc_parse_response(&response, (const uint8_t *)buf, buf_size, state);
    if (rc < 0) return rc;

    if (response.is_error) {
        return ZBC_ERR_DEVICE_ERROR;
    }

    return (int)response.result;
}

int zbc_sys_get_cmdline(zbc_client_state_t *state,
                        void *buf, size_t buf_size,
                        char *dest, int max_size)
{
    zbc_builder_t builder;
    zbc_response_t response;
    size_t riff_size;
    size_t i;
    int rc;

    if (!dest) return ZBC_ERR_INVALID_ARG;

    rc = zbc_builder_start(&builder, (uint8_t *)buf, buf_size, state);
    if (rc < 0) return rc;

    rc = zbc_builder_begin_call(&builder, SH_SYS_GET_CMDLINE);
    if (rc < 0) return rc;

    /* Per spec: PARM(max_size) */
    rc = zbc_builder_add_parm_int(&builder, max_size);
    if (rc < 0) return rc;

    rc = zbc_builder_finish(&builder, &riff_size);
    if (rc < 0) return rc;

    rc = zbc_client_submit_poll(state, buf, riff_size);
    if (rc < 0) return rc;

    rc = zbc_parse_response(&response, (const uint8_t *)buf, buf_size, state);
    if (rc < 0) return rc;

    if (response.is_error) {
        return ZBC_ERR_DEVICE_ERROR;
    }

    /* Copy command line from DATA chunk */
    if (response.data && response.data_size > 0) {
        size_t copy_size = response.data_size;
        if (copy_size > (size_t)max_size - 1) {
            copy_size = (size_t)max_size - 1;
        }
        for (i = 0; i < copy_size; i++) {
            dest[i] = (char)response.data[i];
        }
        dest[copy_size] = '\0';
    } else {
        dest[0] = '\0';
    }

    return (int)response.result;
}

int zbc_sys_heapinfo(zbc_client_state_t *state,
                     void *buf, size_t buf_size,
                     zbc_heapinfo_t *info)
{
    zbc_response_t response;
    size_t ptr_size;
    int rc;

    if (!info) return ZBC_ERR_INVALID_ARG;

    rc = exec_syscall_noargs(state, buf, buf_size, SH_SYS_HEAPINFO, &response);
    if (rc < 0) return rc;

    ptr_size = state->ptr_size;

    /* Initialize to zeros */
    info->heap_base = (void *)0;
    info->heap_limit = (void *)0;
    info->stack_base = (void *)0;
    info->stack_limit = (void *)0;

    /* Parse 4 pointers from DATA chunk */
    if (response.data && response.data_size >= ptr_size * 4) {
        const uint8_t *p = response.data;
        size_t i;
        uintptr_t val;

        /* heap_base */
        val = 0;
#if ZBC_CLIENT_ENDIANNESS == ZBC_ENDIAN_LITTLE
        for (i = ptr_size; i > 0; i--) val = (val << 8) | p[i - 1];
#else
        for (i = 0; i < ptr_size; i++) val = (val << 8) | p[i];
#endif
        info->heap_base = (void *)val;
        p += ptr_size;

        /* heap_limit */
        val = 0;
#if ZBC_CLIENT_ENDIANNESS == ZBC_ENDIAN_LITTLE
        for (i = ptr_size; i > 0; i--) val = (val << 8) | p[i - 1];
#else
        for (i = 0; i < ptr_size; i++) val = (val << 8) | p[i];
#endif
        info->heap_limit = (void *)val;
        p += ptr_size;

        /* stack_base */
        val = 0;
#if ZBC_CLIENT_ENDIANNESS == ZBC_ENDIAN_LITTLE
        for (i = ptr_size; i > 0; i--) val = (val << 8) | p[i - 1];
#else
        for (i = 0; i < ptr_size; i++) val = (val << 8) | p[i];
#endif
        info->stack_base = (void *)val;
        p += ptr_size;

        /* stack_limit */
        val = 0;
#if ZBC_CLIENT_ENDIANNESS == ZBC_ENDIAN_LITTLE
        for (i = ptr_size; i > 0; i--) val = (val << 8) | p[i - 1];
#else
        for (i = 0; i < ptr_size; i++) val = (val << 8) | p[i];
#endif
        info->stack_limit = (void *)val;
    }

    return (int)response.result;
}

/*========================================================================
 * Exit Operations
 *========================================================================*/

void zbc_sys_exit(zbc_client_state_t *state,
                  void *buf, size_t buf_size,
                  unsigned long exception, unsigned long subcode)
{
    zbc_builder_t builder;
    size_t riff_size;
    int rc;

    rc = zbc_builder_start(&builder, (uint8_t *)buf, buf_size, state);
    if (rc < 0) return;

    rc = zbc_builder_begin_call(&builder, SH_SYS_EXIT);
    if (rc < 0) return;

    /* Per spec: PARM(exception), PARM(subcode) */
    rc = zbc_builder_add_parm_uint(&builder, exception);
    if (rc < 0) return;

    rc = zbc_builder_add_parm_uint(&builder, subcode);
    if (rc < 0) return;

    rc = zbc_builder_finish(&builder, &riff_size);
    if (rc < 0) return;

    /* Submit but don't wait for response - this doesn't return */
    (void)zbc_client_submit_poll(state, buf, riff_size);

    /* Should not reach here, but loop forever if we do */
    for (;;) {
        /* Halt */
    }
}

void zbc_sys_exit_extended(zbc_client_state_t *state,
                           void *buf, size_t buf_size,
                           unsigned long code)
{
    zbc_builder_t builder;
    size_t riff_size;
    int rc;

    rc = zbc_builder_start(&builder, (uint8_t *)buf, buf_size, state);
    if (rc < 0) return;

    rc = zbc_builder_begin_call(&builder, SH_SYS_EXIT_EXTENDED);
    if (rc < 0) return;

    /* Per spec: uses same format as SYS_EXIT */
    rc = zbc_builder_add_parm_uint(&builder, code);
    if (rc < 0) return;

    rc = zbc_builder_add_parm_uint(&builder, 0);  /* subcode */
    if (rc < 0) return;

    rc = zbc_builder_finish(&builder, &riff_size);
    if (rc < 0) return;

    (void)zbc_client_submit_poll(state, buf, riff_size);

    for (;;) {
        /* Halt */
    }
}
