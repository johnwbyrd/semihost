/*
 * ZBC Semihosting ANSI C Backend
 *
 * Implements semihosting using only standard C library functions.
 * Provides two backends:
 *   - zbc_backend_ansi(): Secure, sandboxed (recommended)
 *   - zbc_backend_ansi_insecure(): Unrestricted (explicit opt-in)
 */

#include "zbc_semihost.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

/*========================================================================
 * Shared helpers
 *========================================================================*/

/*
 * ARM semihosting open mode to fopen mode string mapping
 */
static const char *ansi_mode_string(int mode)
{
    switch (mode) {
        case 0:  return "r";      /* SH_OPEN_R */
        case 1:  return "rb";     /* SH_OPEN_RB */
        case 2:  return "r+";     /* SH_OPEN_R_PLUS */
        case 3:  return "r+b";    /* SH_OPEN_R_PLUS_B */
        case 4:  return "w";      /* SH_OPEN_W */
        case 5:  return "wb";     /* SH_OPEN_WB */
        case 6:  return "w+";     /* SH_OPEN_W_PLUS */
        case 7:  return "w+b";    /* SH_OPEN_W_PLUS_B */
        case 8:  return "a";      /* SH_OPEN_A */
        case 9:  return "ab";     /* SH_OPEN_AB */
        case 10: return "a+";     /* SH_OPEN_A_PLUS */
        case 11: return "a+b";    /* SH_OPEN_A_PLUS_B */
        default: return NULL;
    }
}

/*
 * Check if open mode is a write mode (modes 4+ involve writing)
 */
static int ansi_mode_is_write(int mode)
{
    return (mode >= 4);
}

/*========================================================================
 * SECURE BACKEND - Path Normalization and Validation
 *========================================================================*/

/*
 * Normalize a path in-place (C90 compatible).
 *
 * Operations:
 * 1. Collapse multiple slashes: "a//b" -> "a/b"
 * 2. Remove . components: "a/./b" -> "a/b"
 * 3. Resolve .. components: "a/b/../c" -> "a/c"
 *
 * Returns new length of normalized path.
 */
static size_t ansi_path_normalize(char *path, size_t path_len)
{
    size_t read_pos;
    size_t write_pos;
    size_t component_start;
    size_t comp_len;
    int is_absolute;
    char c;

    if (path_len == 0) {
        return 0;
    }

    /* Check if absolute path */
    is_absolute = (path[0] == '/' || path[0] == '\\');

    read_pos = 0;
    write_pos = 0;

    /* Preserve leading slash for absolute paths */
    if (is_absolute) {
        path[write_pos++] = '/';
        /* Skip all leading slashes */
        while (read_pos < path_len &&
               (path[read_pos] == '/' || path[read_pos] == '\\')) {
            read_pos++;
        }
    }

    component_start = write_pos;

    while (read_pos < path_len) {
        c = path[read_pos];

        if (c == '/' || c == '\\') {
            /* End of component */
            comp_len = write_pos - component_start;

            if (comp_len == 1 && path[component_start] == '.') {
                /* "." - remove it */
                write_pos = component_start;
            } else if (comp_len == 2 && path[component_start] == '.' &&
                       path[component_start + 1] == '.') {
                /* ".." - go up one level */
                write_pos = component_start;
                if (write_pos > 0) {
                    write_pos--;  /* Remove trailing slash */
                    /* Find previous slash (but not past root) */
                    while (write_pos > (is_absolute ? 1 : 0) &&
                           path[write_pos - 1] != '/' &&
                           path[write_pos - 1] != '\\') {
                        write_pos--;
                    }
                }
            }

            /* Skip multiple slashes */
            while (read_pos < path_len &&
                   (path[read_pos] == '/' || path[read_pos] == '\\')) {
                read_pos++;
            }

            /* Add slash if we have content and more to come */
            if (write_pos > 0 && read_pos < path_len) {
                path[write_pos++] = '/';
            }
            component_start = write_pos;
        } else {
            path[write_pos++] = c;
            read_pos++;
        }
    }

    /* Handle trailing component */
    comp_len = write_pos - component_start;
    if (comp_len == 1 && path[component_start] == '.') {
        write_pos = component_start;
        if (write_pos > 0) {
            write_pos--;  /* Remove trailing slash */
        }
    } else if (comp_len == 2 && path[component_start] == '.' &&
               path[component_start + 1] == '.') {
        write_pos = component_start;
        if (write_pos > 0) {
            write_pos--;
            while (write_pos > 0 && path[write_pos - 1] != '/' &&
                   path[write_pos - 1] != '\\') {
                write_pos--;
            }
            if (write_pos > 0) {
                write_pos--;
            }
        }
    }

    /* Remove trailing slash unless it's the root */
    if (write_pos > 1 && path[write_pos - 1] == '/') {
        write_pos--;
    }

    path[write_pos] = '\0';
    return write_pos;
}

/*
 * Validate a path against sandbox and path rules.
 *
 * Returns:
 *   0 if path is allowed (resolved path written to state->path_buf)
 *   -1 if path is denied
 */
static int ansi_validate_path(zbc_ansi_state_t *state, const char *path,
                              size_t path_len, int for_write,
                              size_t *resolved_len)
{
    size_t norm_len;
    int i;

    /* Use custom policy if provided */
    if (state->policy && state->policy->validate_path) {
        return state->policy->validate_path(
            state->policy_ctx, path, path_len, for_write,
            state->path_buf, sizeof(state->path_buf), resolved_len);
    }

    /* Check if path is absolute or relative */
    if (path_len > 0 && (path[0] == '/' || path[0] == '\\')) {
        /* Absolute path - copy and normalize */
        if (path_len >= sizeof(state->path_buf)) {
            return -1;  /* Path too long */
        }
        memcpy(state->path_buf, path, path_len);
        state->path_buf[path_len] = '\0';
        norm_len = ansi_path_normalize(state->path_buf, path_len);

        /* Check primary sandbox */
        if (norm_len >= state->sandbox_dir_len &&
            memcmp(state->path_buf, state->sandbox_dir,
                   state->sandbox_dir_len) == 0) {
            *resolved_len = norm_len;
            return 0;  /* Allowed */
        }

        /* Check additional path rules */
        for (i = 0; i < state->path_rule_count; i++) {
            const zbc_ansi_path_rule_t *rule = &state->path_rules[i];
            if (norm_len >= rule->prefix_len &&
                memcmp(state->path_buf, rule->prefix, rule->prefix_len) == 0) {
                if (for_write && !rule->allow_write) {
                    if (state->on_violation) {
                        state->on_violation(state->callback_ctx,
                                            ZBC_ANSI_VIOL_WRITE_BLOCKED, path);
                    }
                    return -1;  /* Write to read-only path */
                }
                *resolved_len = norm_len;
                return 0;  /* Allowed */
            }
        }

        /* No rule matched - deny */
        if (state->on_violation) {
            state->on_violation(state->callback_ctx,
                                ZBC_ANSI_VIOL_PATH_BLOCKED, path);
        }
        return -1;
    }

    /* Relative path - prepend sandbox FIRST, then normalize */
    if (state->sandbox_dir_len + path_len + 1 >= sizeof(state->path_buf)) {
        return -1;  /* Would overflow */
    }

    /* Copy sandbox, then original path (not pre-normalized) */
    memcpy(state->path_buf, state->sandbox_dir, state->sandbox_dir_len);
    memcpy(state->path_buf + state->sandbox_dir_len, path, path_len);
    norm_len = state->sandbox_dir_len + path_len;
    state->path_buf[norm_len] = '\0';

    /* Now normalize the combined path */
    norm_len = ansi_path_normalize(state->path_buf, norm_len);

    /* Verify result is still in sandbox */
    if (norm_len < state->sandbox_dir_len ||
        memcmp(state->path_buf, state->sandbox_dir,
               state->sandbox_dir_len) != 0) {
        if (state->on_violation) {
            state->on_violation(state->callback_ctx,
                                ZBC_ANSI_VIOL_PATH_TRAVERSAL, path);
        }
        return -1;
    }

    *resolved_len = norm_len;
    return 0;
}

/*========================================================================
 * SECURE BACKEND - File Descriptor Management
 *========================================================================*/

static int ansi_alloc_fd(zbc_ansi_state_t *state, FILE *fp)
{
    int fd;
    int idx;
    zbc_ansi_fd_node_t *node;

    if (state->free_fd_list != NULL) {
        /* Reuse from free list (LIFO) */
        node = state->free_fd_list;
        fd = node->fd;
        state->free_fd_list = node->next;
        node->fd = 0;
        node->next = NULL;
    } else {
        /* Allocate new */
        fd = state->next_fd++;
    }

    idx = fd - ZBC_ANSI_FIRST_FD;
    if (idx < 0 || idx >= ZBC_ANSI_MAX_FILES) {
        return -1;  /* Out of FDs */
    }

    state->files[idx] = fp;
    return fd;
}

static void ansi_free_fd(zbc_ansi_state_t *state, int fd)
{
    int idx;
    int pool_idx;

    idx = fd - ZBC_ANSI_FIRST_FD;
    if (idx < 0 || idx >= ZBC_ANSI_MAX_FILES) {
        return;
    }

    state->files[idx] = NULL;

    /* Find a free pool slot and add to free list */
    for (pool_idx = 0; pool_idx < ZBC_ANSI_MAX_FILES; pool_idx++) {
        if (state->fd_pool[pool_idx].fd == 0 ||
            state->fd_pool[pool_idx].fd == fd) {
            state->fd_pool[pool_idx].fd = fd;
            state->fd_pool[pool_idx].next = state->free_fd_list;
            state->free_fd_list = &state->fd_pool[pool_idx];
            break;
        }
    }
}

static FILE *ansi_get_file(zbc_ansi_state_t *state, int fd)
{
    int idx;

    /* Handle stdin/stdout/stderr */
    if (fd == 0) return stdin;
    if (fd == 1) return stdout;
    if (fd == 2) return stderr;

    idx = fd - ZBC_ANSI_FIRST_FD;
    if (idx < 0 || idx >= ZBC_ANSI_MAX_FILES) {
        return NULL;
    }

    return (FILE *)state->files[idx];
}

/*========================================================================
 * SECURE BACKEND - Implementation
 *========================================================================*/

static int ansi_open(void *ctx, const char *path, size_t path_len, int mode)
{
    zbc_ansi_state_t *state = (zbc_ansi_state_t *)ctx;
    const char *mode_str;
    FILE *fp;
    int fd;
    int for_write;
    size_t resolved_len;

    if (!state || !state->initialized) {
        return -1;
    }

    /* Check read-only flag */
    for_write = ansi_mode_is_write(mode);
    if (for_write && (state->flags & ZBC_ANSI_FLAG_READ_ONLY)) {
        if (state->on_violation) {
            state->on_violation(state->callback_ctx,
                                ZBC_ANSI_VIOL_WRITE_BLOCKED, path);
        }
        state->last_errno = EACCES;
        return -1;
    }

    /* Validate and resolve path */
    if (ansi_validate_path(state, path, path_len, for_write,
                           &resolved_len) != 0) {
        state->last_errno = EACCES;
        return -1;
    }

    /* Get mode string */
    mode_str = ansi_mode_string(mode);
    if (mode_str == NULL) {
        state->last_errno = EINVAL;
        return -1;
    }

    /* Open the file */
    fp = fopen(state->path_buf, mode_str);
    if (fp == NULL) {
        state->last_errno = errno;
        return -1;
    }

    /* Allocate FD */
    fd = ansi_alloc_fd(state, fp);
    if (fd < 0) {
        fclose(fp);
        state->last_errno = EMFILE;
        return -1;
    }

    return fd;
}

static int ansi_close(void *ctx, int fd)
{
    zbc_ansi_state_t *state = (zbc_ansi_state_t *)ctx;
    FILE *fp;

    if (!state || !state->initialized) {
        return -1;
    }

    /* Cannot close stdin/stdout/stderr */
    if (fd < ZBC_ANSI_FIRST_FD) {
        return 0;
    }

    fp = ansi_get_file(state, fd);
    if (fp == NULL) {
        state->last_errno = EBADF;
        return -1;
    }

    if (fclose(fp) != 0) {
        state->last_errno = errno;
        return -1;
    }

    ansi_free_fd(state, fd);
    return 0;
}

static int ansi_read(void *ctx, int fd, void *buf, size_t count)
{
    zbc_ansi_state_t *state = (zbc_ansi_state_t *)ctx;
    FILE *fp;
    size_t nread;

    if (!state || !state->initialized) {
        return -1;
    }

    fp = ansi_get_file(state, fd);
    if (fp == NULL) {
        state->last_errno = EBADF;
        return -1;
    }

    nread = fread(buf, 1, count, fp);
    if (nread < count && ferror(fp)) {
        state->last_errno = errno;
        return -1;
    }

    /* Return bytes NOT read */
    return (int)(count - nread);
}

static int ansi_write(void *ctx, int fd, const void *buf, size_t count)
{
    zbc_ansi_state_t *state = (zbc_ansi_state_t *)ctx;
    FILE *fp;
    size_t nwritten;

    if (!state || !state->initialized) {
        return -1;
    }

    fp = ansi_get_file(state, fd);
    if (fp == NULL) {
        state->last_errno = EBADF;
        return -1;
    }

    nwritten = fwrite(buf, 1, count, fp);
    if (nwritten < count) {
        state->last_errno = errno;
    }

    /* Flush stdout/stderr immediately for console output */
    if (fd == 1 || fd == 2) {
        fflush(fp);
    }

    /* Return bytes NOT written */
    return (int)(count - nwritten);
}

static int ansi_seek(void *ctx, int fd, int pos)
{
    zbc_ansi_state_t *state = (zbc_ansi_state_t *)ctx;
    FILE *fp;

    if (!state || !state->initialized) {
        return -1;
    }

    fp = ansi_get_file(state, fd);
    if (fp == NULL) {
        state->last_errno = EBADF;
        return -1;
    }

    if (fseek(fp, pos, SEEK_SET) != 0) {
        state->last_errno = errno;
        return -1;
    }

    return 0;
}

static int ansi_flen(void *ctx, int fd)
{
    zbc_ansi_state_t *state = (zbc_ansi_state_t *)ctx;
    FILE *fp;
    long cur_pos;
    long end_pos;

    if (!state || !state->initialized) {
        return -1;
    }

    fp = ansi_get_file(state, fd);
    if (fp == NULL) {
        state->last_errno = EBADF;
        return -1;
    }

    cur_pos = ftell(fp);
    if (cur_pos < 0) {
        state->last_errno = errno;
        return -1;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        state->last_errno = errno;
        return -1;
    }

    end_pos = ftell(fp);
    if (end_pos < 0) {
        state->last_errno = errno;
        fseek(fp, cur_pos, SEEK_SET);
        return -1;
    }

    if (fseek(fp, cur_pos, SEEK_SET) != 0) {
        state->last_errno = errno;
        return -1;
    }

    return (int)end_pos;
}

static int ansi_remove_file(void *ctx, const char *path, size_t path_len)
{
    zbc_ansi_state_t *state = (zbc_ansi_state_t *)ctx;
    size_t resolved_len;

    if (!state || !state->initialized) {
        return -1;
    }

    /* Check read-only flag */
    if (state->flags & ZBC_ANSI_FLAG_READ_ONLY) {
        if (state->on_violation) {
            state->on_violation(state->callback_ctx,
                                ZBC_ANSI_VIOL_REMOVE_BLOCKED, path);
        }
        state->last_errno = EACCES;
        return -1;
    }

    /* Validate path (remove is a write operation) */
    if (ansi_validate_path(state, path, path_len, 1, &resolved_len) != 0) {
        state->last_errno = EACCES;
        return -1;
    }

    if (remove(state->path_buf) != 0) {
        state->last_errno = errno;
        return -1;
    }

    return 0;
}

static int ansi_rename_file(void *ctx, const char *old_path, size_t old_len,
                            const char *new_path, size_t new_len)
{
    zbc_ansi_state_t *state = (zbc_ansi_state_t *)ctx;
    char old_resolved[ZBC_ANSI_PATH_BUF_MAX];
    size_t old_resolved_len;
    size_t new_resolved_len;

    if (!state || !state->initialized) {
        return -1;
    }

    /* Check read-only flag */
    if (state->flags & ZBC_ANSI_FLAG_READ_ONLY) {
        if (state->on_violation) {
            state->on_violation(state->callback_ctx,
                                ZBC_ANSI_VIOL_RENAME_BLOCKED, old_path);
        }
        state->last_errno = EACCES;
        return -1;
    }

    /* Validate old path */
    if (ansi_validate_path(state, old_path, old_len, 1,
                           &old_resolved_len) != 0) {
        state->last_errno = EACCES;
        return -1;
    }

    /* Save old resolved path */
    memcpy(old_resolved, state->path_buf, old_resolved_len + 1);

    /* Validate new path */
    if (ansi_validate_path(state, new_path, new_len, 1,
                           &new_resolved_len) != 0) {
        state->last_errno = EACCES;
        return -1;
    }

    /* Now state->path_buf has new path, old_resolved has old path */
    if (rename(old_resolved, state->path_buf) != 0) {
        state->last_errno = errno;
        return -1;
    }

    return 0;
}

static int ansi_tmpnam_func(void *ctx, char *buf, size_t buf_size, int id)
{
    zbc_ansi_state_t *state = (zbc_ansi_state_t *)ctx;
    size_t needed;

    if (!state || !state->initialized) {
        return -1;
    }

    /* Format: {sandbox_dir}tmp{id:03d}.tmp */
    needed = state->sandbox_dir_len + 3 + 3 + 4 + 1;  /* tmp + NNN + .tmp + \0 */

    if (buf_size < needed) {
        state->last_errno = EINVAL;
        return -1;
    }

    memcpy(buf, state->sandbox_dir, state->sandbox_dir_len);
    sprintf(buf + state->sandbox_dir_len, "tmp%03d.tmp", id % 1000);

    return 0;
}

static void ansi_writec(void *ctx, char c)
{
    (void)ctx;
    putchar(c);
    fflush(stdout);
}

static void ansi_write0(void *ctx, const char *str)
{
    (void)ctx;
    fputs(str, stdout);
    fflush(stdout);
}

static int ansi_readc(void *ctx)
{
    int c;
    (void)ctx;
    c = getchar();
    if (c == EOF) {
        return -1;
    }
    return c;
}

static int ansi_iserror(void *ctx, int status)
{
    (void)ctx;
    return (status < 0) ? 1 : 0;
}

static int ansi_istty(void *ctx, int fd)
{
    (void)ctx;
    if (fd >= 0 && fd <= 2) {
        return 1;
    }
    return 0;
}

static int ansi_clock_func(void *ctx)
{
    zbc_ansi_state_t *state = (zbc_ansi_state_t *)ctx;
    clock_t now;
    clock_t elapsed;

    if (!state || !state->initialized) {
        return -1;
    }

    now = clock();
    elapsed = now - (clock_t)state->start_clock;

    /* Convert to centiseconds */
    return (int)((elapsed * 100) / CLOCKS_PER_SEC);
}

static int ansi_time_func(void *ctx)
{
    time_t t;

    (void)ctx;

    t = time(NULL);
    if (t == (time_t)-1) {
        return -1;
    }

    return (int)t;
}

static int ansi_elapsed(void *ctx, unsigned int *lo, unsigned int *hi)
{
    zbc_ansi_state_t *state = (zbc_ansi_state_t *)ctx;
    clock_t now;
    clock_t elapsed;

    if (!state || !state->initialized) {
        return -1;
    }

    now = clock();
    elapsed = now - (clock_t)state->start_clock;

    *lo = (unsigned int)elapsed;
    *hi = 0;

    return 0;
}

static int ansi_tickfreq(void *ctx)
{
    (void)ctx;
    return (int)CLOCKS_PER_SEC;
}

static int ansi_do_system(void *ctx, const char *cmd, size_t cmd_len)
{
    zbc_ansi_state_t *state = (zbc_ansi_state_t *)ctx;
    int result;

    if (!state || !state->initialized) {
        return -1;
    }

    /* Check if system() is allowed */
    if (!(state->flags & ZBC_ANSI_FLAG_ALLOW_SYSTEM)) {
        if (state->on_violation) {
            state->on_violation(state->callback_ctx,
                                ZBC_ANSI_VIOL_SYSTEM_BLOCKED, cmd);
        }
        return -1;
    }

    /* Check custom policy */
    if (state->policy && state->policy->validate_system) {
        if (state->policy->validate_system(state->policy_ctx,
                                           cmd, cmd_len) != 0) {
            if (state->on_violation) {
                state->on_violation(state->callback_ctx,
                                    ZBC_ANSI_VIOL_SYSTEM_BLOCKED, cmd);
            }
            return -1;
        }
    }

    /* Copy command to path_buf for null-termination */
    if (cmd_len >= sizeof(state->path_buf)) {
        state->last_errno = ENAMETOOLONG;
        return -1;
    }
    memcpy(state->path_buf, cmd, cmd_len);
    state->path_buf[cmd_len] = '\0';

    result = system(state->path_buf);
    return result;
}

static int ansi_get_cmdline(void *ctx, char *buf, size_t buf_size)
{
    (void)ctx;
    if (buf_size > 0) {
        buf[0] = '\0';
    }
    return 0;
}

static int ansi_heapinfo(void *ctx, unsigned int *heap_base,
                         unsigned int *heap_limit, unsigned int *stack_base,
                         unsigned int *stack_limit)
{
    (void)ctx;
    *heap_base = 0;
    *heap_limit = 0;
    *stack_base = 0;
    *stack_limit = 0;
    return 0;
}

static void ansi_do_exit(void *ctx, unsigned int reason, unsigned int subcode)
{
    zbc_ansi_state_t *state = (zbc_ansi_state_t *)ctx;

    if (!state) {
        return;
    }

    /* Check if exit() is allowed */
    if (!(state->flags & ZBC_ANSI_FLAG_ALLOW_EXIT)) {
        /* Check custom policy */
        if (state->policy && state->policy->handle_exit) {
            if (state->policy->handle_exit(state->policy_ctx,
                                           reason, subcode) != 0) {
                /* Policy blocked exit */
                if (state->on_violation) {
                    state->on_violation(state->callback_ctx,
                                        ZBC_ANSI_VIOL_EXIT_BLOCKED, NULL);
                }
                if (state->on_exit) {
                    state->on_exit(state->callback_ctx, reason, subcode);
                }
                return;
            }
        } else {
            /* No policy, exit blocked by flag */
            if (state->on_violation) {
                state->on_violation(state->callback_ctx,
                                    ZBC_ANSI_VIOL_EXIT_BLOCKED, NULL);
            }
            if (state->on_exit) {
                state->on_exit(state->callback_ctx, reason, subcode);
            }
            return;
        }
    }

    /* Clean up and exit */
    zbc_ansi_cleanup(state);
    exit((int)(reason & 0xFF));
}

static int ansi_get_errno(void *ctx)
{
    zbc_ansi_state_t *state = (zbc_ansi_state_t *)ctx;
    if (!state) {
        return 0;
    }
    return state->last_errno;
}

/*========================================================================
 * SECURE BACKEND - Vtable and Public API
 *========================================================================*/

static const zbc_backend_t ansi_secure_backend = {
    ansi_open,
    ansi_close,
    ansi_read,
    ansi_write,
    ansi_seek,
    ansi_flen,
    ansi_remove_file,
    ansi_rename_file,
    ansi_tmpnam_func,
    ansi_writec,
    ansi_write0,
    ansi_readc,
    ansi_iserror,
    ansi_istty,
    ansi_clock_func,
    ansi_time_func,
    ansi_elapsed,
    ansi_tickfreq,
    ansi_do_system,
    ansi_get_cmdline,
    ansi_heapinfo,
    ansi_do_exit,
    ansi_get_errno
};

const zbc_backend_t *zbc_backend_ansi(void)
{
    return &ansi_secure_backend;
}

void zbc_ansi_init(zbc_ansi_state_t *state, const char *sandbox_dir)
{
    size_t len;
    int i;

    if (!state || !sandbox_dir) {
        return;
    }

    memset(state, 0, sizeof(*state));

    /* Copy sandbox directory */
    len = strlen(sandbox_dir);
    if (len >= ZBC_ANSI_SANDBOX_DIR_MAX - 1) {
        len = ZBC_ANSI_SANDBOX_DIR_MAX - 2;
    }
    memcpy(state->sandbox_dir, sandbox_dir, len);

    /* Ensure trailing slash */
    if (len > 0 && state->sandbox_dir[len - 1] != '/') {
        state->sandbox_dir[len] = '/';
        len++;
    }
    state->sandbox_dir[len] = '\0';
    state->sandbox_dir_len = len;

    /* Initialize FD tracking */
    state->next_fd = ZBC_ANSI_FIRST_FD;
    state->free_fd_list = NULL;
    for (i = 0; i < ZBC_ANSI_MAX_FILES; i++) {
        state->files[i] = NULL;
        state->fd_pool[i].fd = 0;
        state->fd_pool[i].next = NULL;
    }

    state->start_clock = (uint32_t)clock();
    state->initialized = 1;
}

int zbc_ansi_add_path(zbc_ansi_state_t *state, const char *prefix,
                      int allow_write)
{
    zbc_ansi_path_rule_t *rule;

    if (!state || !prefix) {
        return -1;
    }

    if (state->path_rule_count >= ZBC_ANSI_MAX_PATH_RULES) {
        return -1;
    }

    rule = &state->path_rules[state->path_rule_count];
    rule->prefix = prefix;
    rule->prefix_len = strlen(prefix);
    rule->allow_write = allow_write;

    state->path_rule_count++;
    return 0;
}

void zbc_ansi_set_policy(zbc_ansi_state_t *state,
                         const zbc_ansi_policy_t *policy, void *ctx)
{
    if (!state) {
        return;
    }
    state->policy = policy;
    state->policy_ctx = ctx;
}

void zbc_ansi_set_callbacks(zbc_ansi_state_t *state,
                            void (*on_violation)(void *ctx, int type,
                                                 const char *detail),
                            void (*on_exit)(void *ctx, unsigned int reason,
                                            unsigned int subcode),
                            void *ctx)
{
    if (!state) {
        return;
    }
    state->on_violation = on_violation;
    state->on_exit = on_exit;
    state->callback_ctx = ctx;
}

void zbc_ansi_cleanup(zbc_ansi_state_t *state)
{
    int i;

    if (!state || !state->initialized) {
        return;
    }

    /* Close all open files */
    for (i = 0; i < ZBC_ANSI_MAX_FILES; i++) {
        if (state->files[i] != NULL) {
            fclose((FILE *)state->files[i]);
            state->files[i] = NULL;
        }
    }

    state->initialized = 0;
}

/*========================================================================
 * INSECURE BACKEND - File Descriptor Management
 *========================================================================*/

static int ansi_insecure_alloc_fd(zbc_ansi_insecure_state_t *state, FILE *fp)
{
    int fd;
    int idx;
    zbc_ansi_fd_node_t *node;

    if (state->free_fd_list != NULL) {
        node = state->free_fd_list;
        fd = node->fd;
        state->free_fd_list = node->next;
        node->fd = 0;
        node->next = NULL;
    } else {
        fd = state->next_fd++;
    }

    idx = fd - ZBC_ANSI_FIRST_FD;
    if (idx < 0 || idx >= ZBC_ANSI_MAX_FILES) {
        return -1;
    }

    state->files[idx] = fp;
    return fd;
}

static void ansi_insecure_free_fd(zbc_ansi_insecure_state_t *state, int fd)
{
    int idx;
    int pool_idx;

    idx = fd - ZBC_ANSI_FIRST_FD;
    if (idx < 0 || idx >= ZBC_ANSI_MAX_FILES) {
        return;
    }

    state->files[idx] = NULL;

    for (pool_idx = 0; pool_idx < ZBC_ANSI_MAX_FILES; pool_idx++) {
        if (state->fd_pool[pool_idx].fd == 0 ||
            state->fd_pool[pool_idx].fd == fd) {
            state->fd_pool[pool_idx].fd = fd;
            state->fd_pool[pool_idx].next = state->free_fd_list;
            state->free_fd_list = &state->fd_pool[pool_idx];
            break;
        }
    }
}

static FILE *ansi_insecure_get_file(zbc_ansi_insecure_state_t *state, int fd)
{
    int idx;

    if (fd == 0) return stdin;
    if (fd == 1) return stdout;
    if (fd == 2) return stderr;

    idx = fd - ZBC_ANSI_FIRST_FD;
    if (idx < 0 || idx >= ZBC_ANSI_MAX_FILES) {
        return NULL;
    }

    return (FILE *)state->files[idx];
}

/*========================================================================
 * INSECURE BACKEND - Implementation
 *========================================================================*/

static int ansi_insecure_open(void *ctx, const char *path, size_t path_len,
                              int mode)
{
    zbc_ansi_insecure_state_t *state = (zbc_ansi_insecure_state_t *)ctx;
    const char *mode_str;
    FILE *fp;
    int fd;

    if (!state || !state->initialized) {
        return -1;
    }

    mode_str = ansi_mode_string(mode);
    if (mode_str == NULL) {
        state->last_errno = EINVAL;
        return -1;
    }

    /* Path may not be null-terminated - use state buffer */
    if (path_len >= sizeof(state->path_buf)) {
        state->last_errno = ENAMETOOLONG;
        return -1;
    }
    memcpy(state->path_buf, path, path_len);
    state->path_buf[path_len] = '\0';

    fp = fopen(state->path_buf, mode_str);
    if (fp == NULL) {
        state->last_errno = errno;
        return -1;
    }

    fd = ansi_insecure_alloc_fd(state, fp);
    if (fd < 0) {
        fclose(fp);
        state->last_errno = EMFILE;
        return -1;
    }

    return fd;
}

static int ansi_insecure_close(void *ctx, int fd)
{
    zbc_ansi_insecure_state_t *state = (zbc_ansi_insecure_state_t *)ctx;
    FILE *fp;

    if (!state || !state->initialized) {
        return -1;
    }

    if (fd < ZBC_ANSI_FIRST_FD) {
        return 0;
    }

    fp = ansi_insecure_get_file(state, fd);
    if (fp == NULL) {
        state->last_errno = EBADF;
        return -1;
    }

    if (fclose(fp) != 0) {
        state->last_errno = errno;
        return -1;
    }

    ansi_insecure_free_fd(state, fd);
    return 0;
}

static int ansi_insecure_read(void *ctx, int fd, void *buf, size_t count)
{
    zbc_ansi_insecure_state_t *state = (zbc_ansi_insecure_state_t *)ctx;
    FILE *fp;
    size_t nread;

    if (!state || !state->initialized) {
        return -1;
    }

    fp = ansi_insecure_get_file(state, fd);
    if (fp == NULL) {
        state->last_errno = EBADF;
        return -1;
    }

    nread = fread(buf, 1, count, fp);
    if (nread < count && ferror(fp)) {
        state->last_errno = errno;
        return -1;
    }

    return (int)(count - nread);
}

static int ansi_insecure_write(void *ctx, int fd, const void *buf, size_t count)
{
    zbc_ansi_insecure_state_t *state = (zbc_ansi_insecure_state_t *)ctx;
    FILE *fp;
    size_t nwritten;

    if (!state || !state->initialized) {
        return -1;
    }

    fp = ansi_insecure_get_file(state, fd);
    if (fp == NULL) {
        state->last_errno = EBADF;
        return -1;
    }

    nwritten = fwrite(buf, 1, count, fp);
    if (nwritten < count) {
        state->last_errno = errno;
    }

    if (fd == 1 || fd == 2) {
        fflush(fp);
    }

    return (int)(count - nwritten);
}

static int ansi_insecure_seek(void *ctx, int fd, int pos)
{
    zbc_ansi_insecure_state_t *state = (zbc_ansi_insecure_state_t *)ctx;
    FILE *fp;

    if (!state || !state->initialized) {
        return -1;
    }

    fp = ansi_insecure_get_file(state, fd);
    if (fp == NULL) {
        state->last_errno = EBADF;
        return -1;
    }

    if (fseek(fp, pos, SEEK_SET) != 0) {
        state->last_errno = errno;
        return -1;
    }

    return 0;
}

static int ansi_insecure_flen(void *ctx, int fd)
{
    zbc_ansi_insecure_state_t *state = (zbc_ansi_insecure_state_t *)ctx;
    FILE *fp;
    long cur_pos;
    long end_pos;

    if (!state || !state->initialized) {
        return -1;
    }

    fp = ansi_insecure_get_file(state, fd);
    if (fp == NULL) {
        state->last_errno = EBADF;
        return -1;
    }

    cur_pos = ftell(fp);
    if (cur_pos < 0) {
        state->last_errno = errno;
        return -1;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        state->last_errno = errno;
        return -1;
    }

    end_pos = ftell(fp);
    if (end_pos < 0) {
        state->last_errno = errno;
        fseek(fp, cur_pos, SEEK_SET);
        return -1;
    }

    if (fseek(fp, cur_pos, SEEK_SET) != 0) {
        state->last_errno = errno;
        return -1;
    }

    return (int)end_pos;
}

static int ansi_insecure_remove_file(void *ctx, const char *path,
                                     size_t path_len)
{
    zbc_ansi_insecure_state_t *state = (zbc_ansi_insecure_state_t *)ctx;

    if (!state || !state->initialized) {
        return -1;
    }

    if (path_len >= sizeof(state->path_buf)) {
        state->last_errno = ENAMETOOLONG;
        return -1;
    }
    memcpy(state->path_buf, path, path_len);
    state->path_buf[path_len] = '\0';

    if (remove(state->path_buf) != 0) {
        state->last_errno = errno;
        return -1;
    }

    return 0;
}

static int ansi_insecure_rename_file(void *ctx, const char *old_path,
                                     size_t old_len, const char *new_path,
                                     size_t new_len)
{
    zbc_ansi_insecure_state_t *state = (zbc_ansi_insecure_state_t *)ctx;
    char old_copy[ZBC_ANSI_PATH_BUF_MAX];

    if (!state || !state->initialized) {
        return -1;
    }

    if (old_len >= sizeof(old_copy) || new_len >= sizeof(state->path_buf)) {
        state->last_errno = ENAMETOOLONG;
        return -1;
    }

    memcpy(old_copy, old_path, old_len);
    old_copy[old_len] = '\0';

    memcpy(state->path_buf, new_path, new_len);
    state->path_buf[new_len] = '\0';

    if (rename(old_copy, state->path_buf) != 0) {
        state->last_errno = errno;
        return -1;
    }

    return 0;
}

static int ansi_insecure_tmpnam_func(void *ctx, char *buf, size_t buf_size,
                                     int id)
{
    (void)ctx;

    if (buf_size < 12) {
        return -1;
    }

    sprintf(buf, "tmp%03d.tmp", id % 1000);
    return 0;
}

static int ansi_insecure_clock_func(void *ctx)
{
    zbc_ansi_insecure_state_t *state = (zbc_ansi_insecure_state_t *)ctx;
    clock_t now;
    clock_t elapsed;

    if (!state || !state->initialized) {
        return -1;
    }

    now = clock();
    elapsed = now - (clock_t)state->start_clock;

    return (int)((elapsed * 100) / CLOCKS_PER_SEC);
}

static int ansi_insecure_elapsed(void *ctx, unsigned int *lo, unsigned int *hi)
{
    zbc_ansi_insecure_state_t *state = (zbc_ansi_insecure_state_t *)ctx;
    clock_t now;
    clock_t elapsed;

    if (!state || !state->initialized) {
        return -1;
    }

    now = clock();
    elapsed = now - (clock_t)state->start_clock;

    *lo = (unsigned int)elapsed;
    *hi = 0;

    return 0;
}

static int ansi_insecure_do_system(void *ctx, const char *cmd, size_t cmd_len)
{
    zbc_ansi_insecure_state_t *state = (zbc_ansi_insecure_state_t *)ctx;

    if (!state || !state->initialized) {
        return -1;
    }

    if (cmd_len >= sizeof(state->path_buf)) {
        state->last_errno = ENAMETOOLONG;
        return -1;
    }
    memcpy(state->path_buf, cmd, cmd_len);
    state->path_buf[cmd_len] = '\0';

    return system(state->path_buf);
}

static void ansi_insecure_do_exit(void *ctx, unsigned int reason,
                                  unsigned int subcode)
{
    zbc_ansi_insecure_state_t *state = (zbc_ansi_insecure_state_t *)ctx;
    (void)subcode;

    if (state) {
        zbc_ansi_insecure_cleanup(state);
    }

    exit((int)(reason & 0xFF));
}

static int ansi_insecure_get_errno(void *ctx)
{
    zbc_ansi_insecure_state_t *state = (zbc_ansi_insecure_state_t *)ctx;
    if (!state) {
        return 0;
    }
    return state->last_errno;
}

/*========================================================================
 * INSECURE BACKEND - Vtable and Public API
 *========================================================================*/

static const zbc_backend_t ansi_insecure_backend = {
    ansi_insecure_open,
    ansi_insecure_close,
    ansi_insecure_read,
    ansi_insecure_write,
    ansi_insecure_seek,
    ansi_insecure_flen,
    ansi_insecure_remove_file,
    ansi_insecure_rename_file,
    ansi_insecure_tmpnam_func,
    ansi_writec,
    ansi_write0,
    ansi_readc,
    ansi_iserror,
    ansi_istty,
    ansi_insecure_clock_func,
    ansi_time_func,
    ansi_insecure_elapsed,
    ansi_tickfreq,
    ansi_insecure_do_system,
    ansi_get_cmdline,
    ansi_heapinfo,
    ansi_insecure_do_exit,
    ansi_insecure_get_errno
};

const zbc_backend_t *zbc_backend_ansi_insecure(void)
{
    return &ansi_insecure_backend;
}

void zbc_ansi_insecure_init(zbc_ansi_insecure_state_t *state)
{
    int i;

    if (!state) {
        return;
    }

    memset(state, 0, sizeof(*state));

    state->next_fd = ZBC_ANSI_FIRST_FD;
    state->free_fd_list = NULL;
    for (i = 0; i < ZBC_ANSI_MAX_FILES; i++) {
        state->files[i] = NULL;
        state->fd_pool[i].fd = 0;
        state->fd_pool[i].next = NULL;
    }

    state->start_clock = (uint32_t)clock();
    state->initialized = 1;
}

void zbc_ansi_insecure_cleanup(zbc_ansi_insecure_state_t *state)
{
    int i;

    if (!state || !state->initialized) {
        return;
    }

    for (i = 0; i < ZBC_ANSI_MAX_FILES; i++) {
        if (state->files[i] != NULL) {
            fclose((FILE *)state->files[i]);
            state->files[i] = NULL;
        }
    }

    state->initialized = 0;
}

/*========================================================================
 * Legacy API - Removed
 *
 * The old zbc_backend_ansi_cleanup() global function is no longer needed
 * since state is now caller-provided. Use zbc_ansi_cleanup() or
 * zbc_ansi_insecure_cleanup() instead.
 *========================================================================*/
