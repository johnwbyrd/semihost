/*
 * ZBC Semihosting Backend Interface
 *
 * Backend vtable definition and factory declarations.
 */

#ifndef ZBC_BACKEND_H
#define ZBC_BACKEND_H

#include "zbc_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/*========================================================================
 * Backend Vtable
 *========================================================================*/

typedef struct zbc_backend_s {
    /* File operations */
    int (*open)(void *ctx, const char *path, size_t path_len, int mode);
    int (*close)(void *ctx, int fd);
    int (*read)(void *ctx, int fd, void *buf, size_t count);
    int (*write)(void *ctx, int fd, const void *buf, size_t count);
    int (*seek)(void *ctx, int fd, int pos);
    int (*flen)(void *ctx, int fd);
    int (*remove)(void *ctx, const char *path, size_t path_len);
    int (*rename)(void *ctx, const char *old_path, size_t old_len,
                  const char *new_path, size_t new_len);
    int (*tmpnam)(void *ctx, char *buf, size_t buf_size, int id);

    /* Console operations */
    void (*writec)(void *ctx, char c);
    void (*write0)(void *ctx, const char *str);
    int (*readc)(void *ctx);

    /* Status operations */
    int (*iserror)(void *ctx, int status);
    int (*istty)(void *ctx, int fd);
    int (*clock)(void *ctx);
    int (*time)(void *ctx);
    int (*elapsed)(void *ctx, unsigned int *lo, unsigned int *hi);
    int (*tickfreq)(void *ctx);

    /* System operations */
    int (*do_system)(void *ctx, const char *cmd, size_t cmd_len);
    int (*get_cmdline)(void *ctx, char *buf, size_t buf_size);
    int (*heapinfo)(void *ctx, unsigned int *heap_base, unsigned int *heap_limit,
                    unsigned int *stack_base, unsigned int *stack_limit);
    void (*do_exit)(void *ctx, unsigned int reason, unsigned int subcode);
    int (*get_errno)(void *ctx);
} zbc_backend_t;

/*========================================================================
 * Built-in Backends
 *========================================================================*/

/* Secure ANSI backend (sandboxed) - recommended for production */
const zbc_backend_t *zbc_backend_ansi(void);

/* Insecure ANSI backend (unrestricted) - for trusted code only */
const zbc_backend_t *zbc_backend_ansi_insecure(void);

/* Dummy backend (no-op) - for testing */
const zbc_backend_t *zbc_backend_dummy(void);

#ifdef __cplusplus
}
#endif

#endif /* ZBC_BACKEND_H */
