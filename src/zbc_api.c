/*
 * ZBC Semihosting High-Level API Implementation
 *
 * Type-safe wrapper functions for all semihosting operations.
 */

#include "zbc_api.h"

/*========================================================================
 * Initialization
 *========================================================================*/

void zbc_api_init(zbc_api_t *api, zbc_client_state_t *client,
                  void *buf, size_t buf_size) {
    api->client = client;
    api->buf = buf;
    api->buf_size = buf_size;
    api->last_errno = 0;
}

int zbc_api_errno(zbc_api_t *api) {
    return api->last_errno;
}

/*========================================================================
 * File Operations
 *========================================================================*/

int zbc_api_open(zbc_api_t *api, const char *path, int mode) {
    zbc_response_t response;
    uintptr_t args[3];
    int rc;

    args[0] = (uintptr_t)path;
    args[1] = (uintptr_t)mode;
    args[2] = (uintptr_t)zbc_strlen(path);

    rc = zbc_call(&response, api->client, api->buf, api->buf_size,
                  SH_SYS_OPEN, args);
    api->last_errno = (rc == ZBC_OK) ? response.error_code : 0;
    return (rc == ZBC_OK) ? response.result : -1;
}

int zbc_api_close(zbc_api_t *api, int fd) {
    zbc_response_t response;
    uintptr_t args[1];
    int rc;

    args[0] = (uintptr_t)fd;

    rc = zbc_call(&response, api->client, api->buf, api->buf_size,
                  SH_SYS_CLOSE, args);
    api->last_errno = (rc == ZBC_OK) ? response.error_code : 0;
    return (rc == ZBC_OK) ? response.result : -1;
}

int zbc_api_read(zbc_api_t *api, int fd, void *dest, size_t count) {
    zbc_response_t response;
    uintptr_t args[3];
    int rc;

    args[0] = (uintptr_t)fd;
    args[1] = (uintptr_t)dest;
    args[2] = (uintptr_t)count;

    rc = zbc_call(&response, api->client, api->buf, api->buf_size,
                  SH_SYS_READ, args);
    api->last_errno = (rc == ZBC_OK) ? response.error_code : 0;
    return (rc == ZBC_OK) ? response.result : -1;
}

int zbc_api_write(zbc_api_t *api, int fd, const void *data, size_t count) {
    zbc_response_t response;
    uintptr_t args[3];
    int rc;

    args[0] = (uintptr_t)fd;
    args[1] = (uintptr_t)data;
    args[2] = (uintptr_t)count;

    rc = zbc_call(&response, api->client, api->buf, api->buf_size,
                  SH_SYS_WRITE, args);
    api->last_errno = (rc == ZBC_OK) ? response.error_code : 0;
    return (rc == ZBC_OK) ? response.result : -1;
}

int zbc_api_seek(zbc_api_t *api, int fd, int pos) {
    zbc_response_t response;
    uintptr_t args[2];
    int rc;

    args[0] = (uintptr_t)fd;
    args[1] = (uintptr_t)pos;

    rc = zbc_call(&response, api->client, api->buf, api->buf_size,
                  SH_SYS_SEEK, args);
    api->last_errno = (rc == ZBC_OK) ? response.error_code : 0;
    return (rc == ZBC_OK) ? response.result : -1;
}

intmax_t zbc_api_flen(zbc_api_t *api, int fd) {
    zbc_response_t response;
    uintptr_t args[1];
    int rc;

    args[0] = (uintptr_t)fd;

    rc = zbc_call(&response, api->client, api->buf, api->buf_size,
                  SH_SYS_FLEN, args);
    api->last_errno = (rc == ZBC_OK) ? response.error_code : 0;
    return (rc == ZBC_OK) ? (intmax_t)response.result : -1;
}

int zbc_api_istty(zbc_api_t *api, int fd) {
    zbc_response_t response;
    uintptr_t args[1];
    int rc;

    args[0] = (uintptr_t)fd;

    rc = zbc_call(&response, api->client, api->buf, api->buf_size,
                  SH_SYS_ISTTY, args);
    api->last_errno = (rc == ZBC_OK) ? response.error_code : 0;
    return (rc == ZBC_OK) ? response.result : -1;
}

int zbc_api_remove(zbc_api_t *api, const char *path) {
    zbc_response_t response;
    uintptr_t args[2];
    int rc;

    args[0] = (uintptr_t)path;
    args[1] = (uintptr_t)zbc_strlen(path);

    rc = zbc_call(&response, api->client, api->buf, api->buf_size,
                  SH_SYS_REMOVE, args);
    api->last_errno = (rc == ZBC_OK) ? response.error_code : 0;
    return (rc == ZBC_OK) ? response.result : -1;
}

int zbc_api_rename(zbc_api_t *api, const char *old_path, const char *new_path) {
    zbc_response_t response;
    uintptr_t args[4];
    int rc;

    args[0] = (uintptr_t)old_path;
    args[1] = (uintptr_t)zbc_strlen(old_path);
    args[2] = (uintptr_t)new_path;
    args[3] = (uintptr_t)zbc_strlen(new_path);

    rc = zbc_call(&response, api->client, api->buf, api->buf_size,
                  SH_SYS_RENAME, args);
    api->last_errno = (rc == ZBC_OK) ? response.error_code : 0;
    return (rc == ZBC_OK) ? response.result : -1;
}

int zbc_api_tmpnam(zbc_api_t *api, char *dest, size_t maxlen, int id) {
    zbc_response_t response;
    uintptr_t args[3];
    int rc;

    args[0] = (uintptr_t)dest;
    args[1] = (uintptr_t)id;
    args[2] = (uintptr_t)maxlen;

    rc = zbc_call(&response, api->client, api->buf, api->buf_size,
                  SH_SYS_TMPNAM, args);
    api->last_errno = (rc == ZBC_OK) ? response.error_code : 0;
    return (rc == ZBC_OK) ? response.result : -1;
}

/*========================================================================
 * Console Operations
 *========================================================================*/

void zbc_api_writec(zbc_api_t *api, char c) {
    zbc_response_t response;
    uintptr_t args[1];

    args[0] = (uintptr_t)&c;

    zbc_call(&response, api->client, api->buf, api->buf_size,
             SH_SYS_WRITEC, args);
    api->last_errno = 0;
}

void zbc_api_write0(zbc_api_t *api, const char *str) {
    zbc_response_t response;
    uintptr_t args[1];

    args[0] = (uintptr_t)str;

    zbc_call(&response, api->client, api->buf, api->buf_size,
             SH_SYS_WRITE0, args);
    api->last_errno = 0;
}

int zbc_api_readc(zbc_api_t *api) {
    zbc_response_t response;
    int rc;

    rc = zbc_call(&response, api->client, api->buf, api->buf_size,
                  SH_SYS_READC, (uintptr_t *)0);
    api->last_errno = (rc == ZBC_OK) ? response.error_code : 0;
    return (rc == ZBC_OK) ? response.result : -1;
}

/*========================================================================
 * Time Operations
 *========================================================================*/

int zbc_api_clock(zbc_api_t *api) {
    zbc_response_t response;
    int rc;

    rc = zbc_call(&response, api->client, api->buf, api->buf_size,
                  SH_SYS_CLOCK, (uintptr_t *)0);
    api->last_errno = (rc == ZBC_OK) ? response.error_code : 0;
    return (rc == ZBC_OK) ? response.result : -1;
}

int zbc_api_time(zbc_api_t *api) {
    zbc_response_t response;
    int rc;

    rc = zbc_call(&response, api->client, api->buf, api->buf_size,
                  SH_SYS_TIME, (uintptr_t *)0);
    api->last_errno = (rc == ZBC_OK) ? response.error_code : 0;
    return (rc == ZBC_OK) ? response.result : -1;
}

int zbc_api_tickfreq(zbc_api_t *api) {
    zbc_response_t response;
    int rc;

    rc = zbc_call(&response, api->client, api->buf, api->buf_size,
                  SH_SYS_TICKFREQ, (uintptr_t *)0);
    api->last_errno = (rc == ZBC_OK) ? response.error_code : 0;
    return (rc == ZBC_OK) ? response.result : -1;
}

int zbc_api_elapsed(zbc_api_t *api, uint64_t *ticks_out) {
    zbc_response_t response;
    uintptr_t args[1];
    int rc;

    args[0] = (uintptr_t)ticks_out;

    rc = zbc_call(&response, api->client, api->buf, api->buf_size,
                  SH_SYS_ELAPSED, args);
    api->last_errno = (rc == ZBC_OK) ? response.error_code : 0;
    return (rc == ZBC_OK) ? response.result : -1;
}

int zbc_api_timer_config(zbc_api_t *api, unsigned int rate_hz) {
    zbc_response_t response;
    uintptr_t args[1];
    int rc;

    args[0] = (uintptr_t)rate_hz;

    rc = zbc_call(&response, api->client, api->buf, api->buf_size,
                  SH_SYS_TIMER_CONFIG, args);
    api->last_errno = (rc == ZBC_OK) ? response.error_code : 0;
    return (rc == ZBC_OK) ? response.result : -1;
}

/*========================================================================
 * System Operations
 *========================================================================*/

int zbc_api_iserror(int status) {
    return status < 0 ? 1 : 0;
}

int zbc_api_get_errno(zbc_api_t *api) {
    zbc_response_t response;
    int rc;

    rc = zbc_call(&response, api->client, api->buf, api->buf_size,
                  SH_SYS_ERRNO, (uintptr_t *)0);
    api->last_errno = 0;
    return (rc == ZBC_OK) ? response.result : -1;
}

int zbc_api_system(zbc_api_t *api, const char *cmd) {
    zbc_response_t response;
    uintptr_t args[2];
    int rc;

    args[0] = (uintptr_t)cmd;
    args[1] = (uintptr_t)zbc_strlen(cmd);

    rc = zbc_call(&response, api->client, api->buf, api->buf_size,
                  SH_SYS_SYSTEM, args);
    api->last_errno = (rc == ZBC_OK) ? response.error_code : 0;
    return (rc == ZBC_OK) ? response.result : -1;
}

int zbc_api_get_cmdline(zbc_api_t *api, char *dest, size_t maxlen) {
    zbc_response_t response;
    uintptr_t args[2];
    int rc;

    args[0] = (uintptr_t)dest;
    args[1] = (uintptr_t)maxlen;

    rc = zbc_call(&response, api->client, api->buf, api->buf_size,
                  SH_SYS_GET_CMDLINE, args);
    api->last_errno = (rc == ZBC_OK) ? response.error_code : 0;
    return (rc == ZBC_OK) ? response.result : -1;
}

int zbc_api_heapinfo(zbc_api_t *api, uintptr_t *heap_base,
                     uintptr_t *heap_limit, uintptr_t *stack_base,
                     uintptr_t *stack_limit) {
    zbc_response_t response;
    uintptr_t block[4];
    uintptr_t args[1];
    int rc;

    args[0] = (uintptr_t)block;

    rc = zbc_call(&response, api->client, api->buf, api->buf_size,
                  SH_SYS_HEAPINFO, args);
    api->last_errno = (rc == ZBC_OK) ? response.error_code : 0;

    if (rc == ZBC_OK && response.result == 0) {
        if (heap_base) *heap_base = block[0];
        if (heap_limit) *heap_limit = block[1];
        if (stack_base) *stack_base = block[2];
        if (stack_limit) *stack_limit = block[3];
    }

    return (rc == ZBC_OK) ? response.result : -1;
}

void zbc_api_exit(zbc_api_t *api, int status) {
    zbc_response_t response;
    uintptr_t args[2];

    /* Use ADP_Stopped_ApplicationExit (0x20026) for normal exit */
    args[0] = 0x20026;
    args[1] = (uintptr_t)status;

    zbc_call(&response, api->client, api->buf, api->buf_size,
             SH_SYS_EXIT, args);
    /* Does not return */
}

void zbc_api_exit_extended(zbc_api_t *api, unsigned int reason,
                           unsigned int subcode) {
    zbc_response_t response;
    uintptr_t args[2];

    args[0] = (uintptr_t)reason;
    args[1] = (uintptr_t)subcode;

    zbc_call(&response, api->client, api->buf, api->buf_size,
             SH_SYS_EXIT_EXTENDED, args);
    /* Does not return */
}
