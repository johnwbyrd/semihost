/*
 * ZBC Semihosting ANSI C Backend
 *
 * Implements semihosting using only standard C library functions.
 * Portable across any hosted environment with ANSI C support.
 */

#include "zbc_semihost.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

/*------------------------------------------------------------------------
 * File descriptor table
 *
 * fd 0-2 are reserved for stdin/stdout/stderr.
 * fd 3+ are dynamically allocated FILE* handles.
 *------------------------------------------------------------------------*/

#define ANSI_FIRST_FD 3
#define ANSI_INITIAL_CAPACITY 64 /* Use a reasonable initial capacity */

static FILE **ansi_files_ptr = NULL;
static int ansi_files_capacity = 0;
static int ansi_next_fd = ANSI_FIRST_FD; /* Smallest FD is 3 */
static int ansi_last_errno = 0;
static clock_t ansi_start_clock;

typedef struct FreeFDNode {
    int fd;
    struct FreeFDNode *next;
} FreeFDNode;

static FreeFDNode *ansi_free_fd_list = NULL;
static int ansi_initialized = 0;

static void ansi_init(void)
{
    if (ansi_initialized) {
        return;
    }

    ansi_files_ptr = (FILE**)calloc(ANSI_INITIAL_CAPACITY, sizeof(FILE*));
    if (ansi_files_ptr == NULL) {
        /* Handle allocation error, though in semihosting this might be tricky */
        exit(1); /* or some other error handling */
    }
    ansi_files_capacity = ANSI_INITIAL_CAPACITY;
    ansi_next_fd = ANSI_FIRST_FD;
    ansi_free_fd_list = NULL;

    ansi_start_clock = clock();
    ansi_initialized = 1;
}

/* Helper to resize the ansi_files_ptr array */
static int ansi_resize_fd_array(int required_capacity)
{
    int new_capacity;
    FILE **new_ptr;
    int i;

    new_capacity = ansi_files_capacity;
    if (new_capacity == 0) {
        new_capacity = ANSI_INITIAL_CAPACITY; /* Start with initial capacity if currently 0 */
    }
    while (new_capacity < required_capacity) {
        new_capacity *= 2;
    }

    new_ptr = (FILE**)realloc(ansi_files_ptr, new_capacity * sizeof(FILE*));
    if (new_ptr == NULL) {
        return -1; /* realloc failed */
    }

    /* Initialize newly allocated memory to NULL */
    for (i = ansi_files_capacity; i < new_capacity; i++) {
        new_ptr[i] = NULL;
    }

    ansi_files_ptr = new_ptr;
    ansi_files_capacity = new_capacity;
    return 0;
}

static int ansi_alloc_fd(FILE *fp)
{
    int new_fd;
    int idx;
    FreeFDNode *node_to_reuse;

    if (ansi_free_fd_list != NULL) {
        /* Reuse an FD from the free list */
        node_to_reuse = ansi_free_fd_list;
        new_fd = node_to_reuse->fd;
        ansi_free_fd_list = node_to_reuse->next;
        free(node_to_reuse);
    } else {
        /* Allocate a new FD */
        new_fd = ansi_next_fd++;
    }

    idx = new_fd - ANSI_FIRST_FD;

    /* Ensure array has capacity for this FD */
    if (idx >= ansi_files_capacity) {
        if (ansi_resize_fd_array(idx + 1) != 0) {
            return -1; /* Failed to resize */
        }
    }

    ansi_files_ptr[idx] = fp;
    return new_fd;
}

static FILE *ansi_get_file(int fd)
{
    /* Handle stdin/stdout/stderr */
    if (fd == 0) return stdin;
    if (fd == 1) return stdout;
    if (fd == 2) return stderr;

    /* Validate FD and array bounds */
    if (fd < ANSI_FIRST_FD || (fd - ANSI_FIRST_FD >= ansi_files_capacity)) {
        return NULL; /* Invalid FD or out of bounds */
    }

    return ansi_files_ptr[fd - ANSI_FIRST_FD];
}
/*------------------------------------------------------------------------
 * ARM semihosting open mode to fopen mode string mapping
 *------------------------------------------------------------------------*/

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

/*------------------------------------------------------------------------
 * Backend implementation
 *------------------------------------------------------------------------*/

static int ansi_open(void *ctx, const char *path, size_t path_len, int mode)
{
    const char *mode_str;
    FILE *fp;
    int fd;
    char *path_copy;

    (void)ctx;
    ansi_init();

    mode_str = ansi_mode_string(mode);
    if (mode_str == NULL) {
        ansi_last_errno = EINVAL;
        return -1;
    }

    /* Path may not be null-terminated, so make a copy */
    path_copy = (char *)malloc(path_len + 1);
    if (path_copy == NULL) {
        ansi_last_errno = ENOMEM;
        return -1;
    }
    memcpy(path_copy, path, path_len);
    path_copy[path_len] = '\0';

    fp = fopen(path_copy, mode_str);
    free(path_copy);

    if (fp == NULL) {
        ansi_last_errno = errno;
        return -1;
    }

    fd = ansi_alloc_fd(fp);
    if (fd < 0) {
        fclose(fp);
        ansi_last_errno = EMFILE;
        return -1;
    }

    return fd;
}

static int ansi_close(void *ctx, int fd)
{
    FILE *fp;
    FreeFDNode *new_node;

    (void)ctx;
    ansi_init();

    /* Cannot close stdin/stdout/stderr */
    if (fd < ANSI_FIRST_FD) {
        return 0; /* Silently succeed */
    }

    /* Validate FD and array bounds */
    if (fd - ANSI_FIRST_FD >= ansi_files_capacity || ansi_files_ptr[fd - ANSI_FIRST_FD] == NULL) {
         ansi_last_errno = EBADF;
        return -1;
    }

    fp = ansi_files_ptr[fd - ANSI_FIRST_FD];

    if (fclose(fp) != 0) {
        ansi_last_errno = errno;
        return -1;
    }

    /* Mark the slot as free */
    ansi_files_ptr[fd - ANSI_FIRST_FD] = NULL;

    /* Add FD to free list */
    new_node = (FreeFDNode *)malloc(sizeof(FreeFDNode));
    if (new_node == NULL) {
        /* This is a critical error, but we've already closed the file.
         * Cannot add to free list, so this FD won't be reused.
         * Consider logging or handling more robustly if needed. */
        return 0; /* Still succeed for the close operation itself */
    }
    new_node->fd = fd;
    new_node->next = ansi_free_fd_list;
    ansi_free_fd_list = new_node;

    return 0;
}

static int ansi_read(void *ctx, int fd, void *buf, size_t count)
{
    FILE *fp;
    size_t nread;

    (void)ctx;
    ansi_init();

    fp = ansi_get_file(fd);
    if (fp == NULL) {
        ansi_last_errno = EBADF;
        return -1;
    }

    nread = fread(buf, 1, count, fp);
    if (nread < count && ferror(fp)) {
        ansi_last_errno = errno;
        return -1;
    }

    /* Return bytes NOT read */
    return (int)(count - nread);
}

static int ansi_write(void *ctx, int fd, const void *buf, size_t count)
{
    FILE *fp;
    size_t nwritten;

    (void)ctx;
    ansi_init();

    fp = ansi_get_file(fd);
    if (fp == NULL) {
        ansi_last_errno = EBADF;
        return -1;
    }

    nwritten = fwrite(buf, 1, count, fp);
    if (nwritten < count) {
        ansi_last_errno = errno;
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
    FILE *fp;

    (void)ctx;
    ansi_init();

    fp = ansi_get_file(fd);
    if (fp == NULL) {
        ansi_last_errno = EBADF;
        return -1;
    }

    if (fseek(fp, pos, SEEK_SET) != 0) {
        ansi_last_errno = errno;
        return -1;
    }

    return 0;
}

static int ansi_flen(void *ctx, int fd)
{
    FILE *fp;
    long cur_pos;
    long end_pos;

    (void)ctx;
    ansi_init();

    fp = ansi_get_file(fd);
    if (fp == NULL) {
        ansi_last_errno = EBADF;
        return -1;
    }

    cur_pos = ftell(fp);
    if (cur_pos < 0) {
        ansi_last_errno = errno;
        return -1;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        ansi_last_errno = errno;
        return -1;
    }

    end_pos = ftell(fp);
    if (end_pos < 0) {
        ansi_last_errno = errno;
        fseek(fp, cur_pos, SEEK_SET);
        return -1;
    }

    if (fseek(fp, cur_pos, SEEK_SET) != 0) {
        ansi_last_errno = errno;
        return -1;
    }

    return (int)end_pos;
}

static int ansi_remove_file(void *ctx, const char *path, size_t path_len)
{
    char *path_copy;
    int result;

    (void)ctx;
    ansi_init();

    path_copy = (char *)malloc(path_len + 1);
    if (path_copy == NULL) {
        ansi_last_errno = ENOMEM;
        return -1;
    }
    memcpy(path_copy, path, path_len);
    path_copy[path_len] = '\0';

    result = remove(path_copy);
    free(path_copy);

    if (result != 0) {
        ansi_last_errno = errno;
        return -1;
    }

    return 0;
}

static int ansi_rename_file(void *ctx,
                            const char *old_path, size_t old_len,
                            const char *new_path, size_t new_len)
{
    char *old_copy;
    char *new_copy;
    int result;

    (void)ctx;
    ansi_init();

    old_copy = (char *)malloc(old_len + 1);
    if (old_copy == NULL) {
        ansi_last_errno = ENOMEM;
        return -1;
    }
    memcpy(old_copy, old_path, old_len);
    old_copy[old_len] = '\0';

    new_copy = (char *)malloc(new_len + 1);
    if (new_copy == NULL) {
        free(old_copy);
        ansi_last_errno = ENOMEM;
        return -1;
    }
    memcpy(new_copy, new_path, new_len);
    new_copy[new_len] = '\0';

    result = rename(old_copy, new_copy);
    free(old_copy);
    free(new_copy);

    if (result != 0) {
        ansi_last_errno = errno;
        return -1;
    }

    return 0;
}

static int ansi_tmpnam_func(void *ctx, char *buf, size_t buf_size, int id)
{
    (void)ctx;
    ansi_init();

    /* Generate a simple temp name: tmp<id>.tmp */
    if (buf_size < 12) {
        ansi_last_errno = EINVAL;
        return -1;
    }

    sprintf(buf, "tmp%03d.tmp", id % 1000);
    return 0;
}

static void ansi_writec(void *ctx, char c)
{
    (void)ctx;
    ansi_init();
    putchar(c);
    fflush(stdout);
}

static void ansi_write0(void *ctx, const char *str)
{
    (void)ctx;
    ansi_init();
    fputs(str, stdout);
    fflush(stdout);
}

static int ansi_readc(void *ctx)
{
    int c;
    (void)ctx;
    ansi_init();
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
    /* In pure ANSI C, we can't determine if fd is a TTY.
     * Assume stdin/stdout/stderr are TTYs. */
    if (fd >= 0 && fd <= 2) {
        return 1;
    }
    return 0;
}

static int ansi_clock_func(void *ctx)
{
    clock_t now;
    clock_t elapsed;

    (void)ctx;
    ansi_init();

    now = clock();
    elapsed = now - ansi_start_clock;

    /* Convert to centiseconds */
    return (int)((elapsed * 100) / CLOCKS_PER_SEC);
}

static int ansi_time_func(void *ctx)
{
    time_t t;

    (void)ctx;
    ansi_init();

    t = time(NULL);
    if (t == (time_t)-1) {
        ansi_last_errno = errno;
        return -1;
    }

    return (int)t;
}

static int ansi_elapsed(void *ctx, unsigned int *lo, unsigned int *hi)
{
    clock_t now;
    clock_t elapsed;

    (void)ctx;
    ansi_init();

    now = clock();
    elapsed = now - ansi_start_clock;

    /* Return as ticks (same as clock value) */
    *lo = (unsigned int)elapsed;
    *hi = 0; /* Assume clock_t fits in 32 bits for simplicity */

    return 0;
}

static int ansi_tickfreq(void *ctx)
{
    (void)ctx;
    return (int)CLOCKS_PER_SEC;
}

static int ansi_do_system(void *ctx, const char *cmd, size_t cmd_len)
{
    char *cmd_copy;
    int result;

    (void)ctx;
    ansi_init();

    cmd_copy = (char *)malloc(cmd_len + 1);
    if (cmd_copy == NULL) {
        ansi_last_errno = ENOMEM;
        return -1;
    }
    memcpy(cmd_copy, cmd, cmd_len);
    cmd_copy[cmd_len] = '\0';

    result = system(cmd_copy);
    free(cmd_copy);

    return result;
}

static int ansi_get_cmdline(void *ctx, char *buf, size_t buf_size)
{
    (void)ctx;
    ansi_init();

    /* ANSI C has no standard way to get command line.
     * Return empty string. */
    if (buf_size > 0) {
        buf[0] = '\0';
    }
    return 0;
}

static int ansi_heapinfo(void *ctx,
                         unsigned int *heap_base, unsigned int *heap_limit,
                         unsigned int *stack_base, unsigned int *stack_limit)
{
    (void)ctx;
    ansi_init();

    /* ANSI C has no standard way to query heap/stack layout.
     * Return zeros to indicate unknown. */
    *heap_base = 0;
    *heap_limit = 0;
    *stack_base = 0;
    *stack_limit = 0;

    return 0;
}

static void ansi_do_exit(void *ctx, unsigned int reason, unsigned int subcode)
{
    (void)ctx;
    (void)subcode;

    /* Clean up before exit */
    zbc_backend_ansi_cleanup();

    /* Use reason as exit code, clamped to valid range */
    exit((int)(reason & 0xFF));
}

static int ansi_get_errno(void *ctx)
{
    (void)ctx;
    return ansi_last_errno;
}

/*------------------------------------------------------------------------
 * Backend vtable
 *------------------------------------------------------------------------*/

static const zbc_backend_t ansi_backend = {
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
    return &ansi_backend;
}

void zbc_backend_ansi_cleanup(void)
{
    int i;
    FreeFDNode *current;
    FreeFDNode *next;

    if (!ansi_initialized) {
        return;
    }

    /* Close all open files */
    if (ansi_files_ptr != NULL) {
        for (i = 0; i < ansi_files_capacity; i++) {
            if (ansi_files_ptr[i] != NULL) {
                fclose(ansi_files_ptr[i]);
                ansi_files_ptr[i] = NULL;
            }
        }
        free(ansi_files_ptr);
        ansi_files_ptr = NULL;
    }
    ansi_files_capacity = 0;

    /* Free the free FD list */
    current = ansi_free_fd_list;
    while (current != NULL) {
        next = current->next;
        free(current);
        current = next;
    }
    ansi_free_fd_list = NULL;

    ansi_initialized = 0;
}
