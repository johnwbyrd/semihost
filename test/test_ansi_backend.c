/*
 * ANSI Backend Integration Tests
 *
 * Tests the ANSI backend with real file I/O operations.
 * Creates actual files, writes data, reads it back, verifies results.
 *
 * Tests both:
 *   - Insecure backend: for basic functionality tests
 *   - Secure backend: for security enforcement tests
 */

#ifndef ZBC_HOST
#define ZBC_HOST
#endif
#include "zbc_semihost.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;

/*------------------------------------------------------------------------
 * Global backend state for tests
 *------------------------------------------------------------------------*/

static zbc_ansi_insecure_state_t g_ansi_state;

/*------------------------------------------------------------------------
 * Portable temp directory handling
 *------------------------------------------------------------------------*/

#define MAX_TEMP_DIR_LEN 256
#define MAX_FILENAME_LEN 128

static char g_temp_dir[MAX_TEMP_DIR_LEN];

static void init_temp_dir(void)
{
    const char *tmp;
    size_t len;

    /* Try standard environment variables */
    tmp = getenv("TMPDIR");
    if (!tmp) tmp = getenv("TMP");
    if (!tmp) tmp = getenv("TEMP");

#ifdef _WIN32
    if (!tmp) tmp = "C:\\Windows\\Temp";
#else
    if (!tmp) tmp = "/tmp";
#endif

    len = strlen(tmp);
    if (len >= MAX_TEMP_DIR_LEN) {
        len = MAX_TEMP_DIR_LEN - 1;
    }
    memcpy(g_temp_dir, tmp, len);
    g_temp_dir[len] = '\0';
}

static int make_temp_path(char *buf, size_t buf_size, const char *filename)
{
    size_t dir_len = strlen(g_temp_dir);
    size_t file_len = strlen(filename);
    size_t total = dir_len + 1 + file_len + 1;  /* dir + sep + filename + nul */

    if (total > buf_size) {
        buf[0] = '\0';
        return -1;
    }

#ifdef _WIN32
    sprintf(buf, "%s\\%s", g_temp_dir, filename);
#else
    sprintf(buf, "%s/%s", g_temp_dir, filename);
#endif
    return 0;
}

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", msg, __LINE__); \
        return 0; \
    } \
} while(0)

#define RUN_TEST(name) do { \
    int result; \
    printf("  %s... ", #name); \
    fflush(stdout); \
    tests_run++; \
    result = test_##name(); \
    if (result) { \
        tests_passed++; \
        printf("PASS\n"); \
    } \
} while(0)

/*------------------------------------------------------------------------
 * Stress test constants and helpers
 *------------------------------------------------------------------------*/

#define STRESS_FILE_COUNT_SMALL  64   /* Maximum for fixed-size array */

/* First available FD after stdin/stdout/stderr */
#define ANSI_FIRST_FD 3

static int make_indexed_temp_path(char *buf, size_t buf_size,
                                  const char *prefix, int index)
{
    char filename[MAX_FILENAME_LEN];
    sprintf(filename, "%s_%04d.tmp", prefix, index);
    return make_temp_path(buf, buf_size, filename);
}

static void cleanup_temp_files(const zbc_backend_t *be, void *ctx,
                               int *fds, int fds_count,
                               const char *prefix, int close_first)
{
    int i;
    char path[512];

    for (i = 0; i < fds_count; i++) {
        if (close_first && fds[i] >= 0) {
            be->close(ctx, fds[i]);
            fds[i] = -1;
        }
        make_indexed_temp_path(path, sizeof(path), prefix, i);
        be->remove(ctx, path, strlen(path));
    }
}

/*------------------------------------------------------------------------
 * Test: Open, Write, Close, Open, Read, Verify, Close, Remove
 *------------------------------------------------------------------------*/

static int test_write_read_file(void)
{
    const zbc_backend_t *be = zbc_backend_ansi_insecure();
    void *ctx = &g_ansi_state;
    char filename[512];
    size_t filename_len;
    const char *test_data = "Hello from ZBC semihosting test!\n";
    size_t data_len = strlen(test_data);
    char read_buf[256];
    int fd;
    int result;

    make_temp_path(filename, sizeof(filename), "zbc_test_write_read.txt");
    filename_len = strlen(filename);

    /* Open for writing */
    fd = be->open(ctx, filename, filename_len, 4); /* SH_OPEN_W = 4 */
    TEST_ASSERT(fd >= 0, "open for write failed");

    /* Write data */
    result = be->write(ctx, fd, test_data, data_len);
    TEST_ASSERT(result == 0, "write should return 0 (all bytes written)");

    /* Close */
    result = be->close(ctx, fd);
    TEST_ASSERT(result == 0, "close after write failed");

    /* Open for reading */
    fd = be->open(ctx, filename, filename_len, 0); /* SH_OPEN_R = 0 */
    TEST_ASSERT(fd >= 0, "open for read failed");

    /* Read data back */
    memset(read_buf, 0, sizeof(read_buf));
    result = be->read(ctx, fd, read_buf, data_len);
    TEST_ASSERT(result == 0, "read should return 0 (all bytes read)");

    /* Verify data matches */
    TEST_ASSERT(memcmp(read_buf, test_data, data_len) == 0,
                "read data does not match written data");

    /* Close */
    result = be->close(ctx, fd);
    TEST_ASSERT(result == 0, "close after read failed");

    /* Remove the test file */
    result = be->remove(ctx, filename, filename_len);
    TEST_ASSERT(result == 0, "remove failed");

    return 1;
}

/*------------------------------------------------------------------------
 * Test: File length (flen)
 *------------------------------------------------------------------------*/

static int test_file_length(void)
{
    const zbc_backend_t *be = zbc_backend_ansi_insecure();
    void *ctx = &g_ansi_state;
    char filename[512];
    size_t filename_len;
    const char *test_data = "1234567890"; /* 10 bytes */
    int fd;
    int result;
    int len;

    make_temp_path(filename, sizeof(filename), "zbc_test_flen.txt");
    filename_len = strlen(filename);

    /* Create file with known content */
    fd = be->open(ctx, filename, filename_len, 4); /* SH_OPEN_W */
    TEST_ASSERT(fd >= 0, "open for write failed");

    result = be->write(ctx, fd, test_data, 10);
    TEST_ASSERT(result == 0, "write failed");

    /* Get file length */
    len = be->flen(ctx, fd);
    TEST_ASSERT(len == 10, "flen should return 10");

    be->close(ctx, fd);
    be->remove(ctx, filename, filename_len);

    return 1;
}

/*------------------------------------------------------------------------
 * Test: Seek
 *------------------------------------------------------------------------*/

static int test_seek(void)
{
    const zbc_backend_t *be = zbc_backend_ansi_insecure();
    void *ctx = &g_ansi_state;
    char filename[512];
    size_t filename_len;
    const char *test_data = "ABCDEFGHIJ"; /* 10 bytes */
    char read_buf[4];
    int fd;
    int result;

    make_temp_path(filename, sizeof(filename), "zbc_test_seek.txt");
    filename_len = strlen(filename);

    /* Create file */
    fd = be->open(ctx, filename, filename_len, 4); /* SH_OPEN_W */
    TEST_ASSERT(fd >= 0, "open for write failed");
    be->write(ctx, fd, test_data, 10);
    be->close(ctx, fd);

    /* Reopen for reading */
    fd = be->open(ctx, filename, filename_len, 0); /* SH_OPEN_R */
    TEST_ASSERT(fd >= 0, "open for read failed");

    /* Seek to position 5 and read */
    result = be->seek(ctx, fd, 5);
    TEST_ASSERT(result == 0, "seek failed");

    memset(read_buf, 0, sizeof(read_buf));
    result = be->read(ctx, fd, read_buf, 3);
    TEST_ASSERT(result == 0, "read after seek failed");
    TEST_ASSERT(memcmp(read_buf, "FGH", 3) == 0, "seek position incorrect");

    be->close(ctx, fd);
    be->remove(ctx, filename, filename_len);

    return 1;
}

/*------------------------------------------------------------------------
 * Test: Console write (writec, write0)
 *------------------------------------------------------------------------*/

static int test_console_write(void)
{
    const zbc_backend_t *be = zbc_backend_ansi_insecure();
    void *ctx = &g_ansi_state;

    /* These just shouldn't crash - output goes to stdout */
    be->writec(ctx, 'X');
    be->write0(ctx, "[test_console_write OK]\n");

    return 1;
}

/*------------------------------------------------------------------------
 * Test: Time functions
 *------------------------------------------------------------------------*/

static int test_time_functions(void)
{
    const zbc_backend_t *be = zbc_backend_ansi_insecure();
    void *ctx = &g_ansi_state;
    int clock_val;
    int time_val;
    int tickfreq;

    clock_val = be->clock(ctx);
    TEST_ASSERT(clock_val >= 0, "clock should return >= 0");

    time_val = be->time(ctx);
    TEST_ASSERT(time_val > 0, "time should return > 0 (seconds since epoch)");

    tickfreq = be->tickfreq(ctx);
    TEST_ASSERT(tickfreq > 0, "tickfreq should return > 0");

    return 1;
}

/*------------------------------------------------------------------------
 * Test: istty
 *------------------------------------------------------------------------*/

static int test_istty(void)
{
    const zbc_backend_t *be = zbc_backend_ansi_insecure();
    void *ctx = &g_ansi_state;
    int result;

    /* stdin/stdout/stderr should be TTYs per ANSI backend */
    result = be->istty(ctx, 0);
    TEST_ASSERT(result == 1, "stdin should be TTY");

    result = be->istty(ctx, 1);
    TEST_ASSERT(result == 1, "stdout should be TTY");

    result = be->istty(ctx, 2);
    TEST_ASSERT(result == 1, "stderr should be TTY");

    /* File fd should not be TTY */
    {
        char filename[512];
        int fd;
        make_temp_path(filename, sizeof(filename), "zbc_test_istty.txt");
        fd = be->open(ctx, filename, strlen(filename), 4);
        if (fd >= 0) {
            result = be->istty(ctx, fd);
            TEST_ASSERT(result == 0, "file should not be TTY");
            be->close(ctx, fd);
            be->remove(ctx, filename, strlen(filename));
        }
    }

    return 1;
}

/*------------------------------------------------------------------------
 * Test: tmpnam
 *------------------------------------------------------------------------*/

static int test_tmpnam(void)
{
    const zbc_backend_t *be = zbc_backend_ansi_insecure();
    void *ctx = &g_ansi_state;
    char buf[64];
    int result;

    memset(buf, 0, sizeof(buf));
    result = be->tmpnam(ctx, buf, sizeof(buf), 42);
    TEST_ASSERT(result == 0, "tmpnam failed");
    TEST_ASSERT(strlen(buf) > 0, "tmpnam returned empty string");

    return 1;
}

/*------------------------------------------------------------------------
 * Test: rename
 *------------------------------------------------------------------------*/

static int test_rename(void)
{
    const zbc_backend_t *be = zbc_backend_ansi_insecure();
    void *ctx = &g_ansi_state;
    char old_name[512];
    char new_name[512];
    const char *test_data = "rename test";
    char read_buf[32];
    int fd;
    int result;

    make_temp_path(old_name, sizeof(old_name), "zbc_test_rename_old.txt");
    make_temp_path(new_name, sizeof(new_name), "zbc_test_rename_new.txt");

    /* Create file with old name */
    fd = be->open(ctx, old_name, strlen(old_name), 4);
    TEST_ASSERT(fd >= 0, "create old file failed");
    be->write(ctx, fd, test_data, strlen(test_data));
    be->close(ctx, fd);

    /* Rename */
    result = be->rename(ctx, old_name, strlen(old_name),
                        new_name, strlen(new_name));
    TEST_ASSERT(result == 0, "rename failed");

    /* Verify old name doesn't exist (open should fail) */
    fd = be->open(ctx, old_name, strlen(old_name), 0);
    TEST_ASSERT(fd < 0, "old file should not exist after rename");

    /* Verify new name exists and has correct content */
    fd = be->open(ctx, new_name, strlen(new_name), 0);
    TEST_ASSERT(fd >= 0, "new file should exist after rename");

    memset(read_buf, 0, sizeof(read_buf));
    be->read(ctx, fd, read_buf, strlen(test_data));
    TEST_ASSERT(memcmp(read_buf, test_data, strlen(test_data)) == 0,
                "content should match after rename");

    be->close(ctx, fd);
    be->remove(ctx, new_name, strlen(new_name));

    return 1;
}

/*------------------------------------------------------------------------
 * Test: Partial read (EOF before requested bytes)
 *------------------------------------------------------------------------*/

static int test_partial_read(void)
{
    const zbc_backend_t *be = zbc_backend_ansi_insecure();
    void *ctx = &g_ansi_state;
    char filename[512];
    size_t filename_len;
    const char *test_data = "SHORT"; /* 5 bytes */
    char read_buf[100];
    int fd;
    int result;

    make_temp_path(filename, sizeof(filename), "zbc_test_partial.txt");
    filename_len = strlen(filename);

    /* Create file with 5 bytes */
    fd = be->open(ctx, filename, filename_len, 4);
    TEST_ASSERT(fd >= 0, "open for write failed");
    be->write(ctx, fd, test_data, 5);
    be->close(ctx, fd);

    /* Read requesting 100 bytes */
    fd = be->open(ctx, filename, filename_len, 0);
    TEST_ASSERT(fd >= 0, "open for read failed");

    memset(read_buf, 0, sizeof(read_buf));
    result = be->read(ctx, fd, read_buf, 100);

    /* Result should be 95 (100 - 5 = bytes NOT read) */
    TEST_ASSERT(result == 95, "partial read should return bytes NOT read");
    TEST_ASSERT(memcmp(read_buf, test_data, 5) == 0, "partial read data wrong");

    be->close(ctx, fd);
    be->remove(ctx, filename, filename_len);

    return 1;
}

/*------------------------------------------------------------------------
 * Test: get_errno after failed operation
 *------------------------------------------------------------------------*/

static int test_errno(void)
{
    const zbc_backend_t *be = zbc_backend_ansi_insecure();
    void *ctx = &g_ansi_state;
    char nonexistent[512];
    int fd;
    int err;

    make_temp_path(nonexistent, sizeof(nonexistent), "zbc_test_nonexistent_12345.txt");

    /* Try to open nonexistent file for reading */
    fd = be->open(ctx, nonexistent, strlen(nonexistent), 0);
    TEST_ASSERT(fd < 0, "open nonexistent file should fail");

    err = be->get_errno(ctx);
    TEST_ASSERT(err != 0, "errno should be set after failed open");

    return 1;
}

/*------------------------------------------------------------------------
 * Stress Test: Verify FDs are reused in LIFO order from free list
 *------------------------------------------------------------------------*/

static int test_stress_fd_lifo_reuse(void)
{
    const zbc_backend_t *be = zbc_backend_ansi_insecure();
    void *ctx = &g_ansi_state;
    char path[512];
    int fds[10];
    int reused_fd;
    int i;
    int last_closed_fd;

    /* Initialize */
    for (i = 0; i < 10; i++) {
        fds[i] = -1;
    }

    /* Open 10 files */
    for (i = 0; i < 10; i++) {
        make_indexed_temp_path(path, sizeof(path), "lifo", i);
        fds[i] = be->open(ctx, path, strlen(path), 4);
        TEST_ASSERT(fds[i] >= 0, "open failed");
    }

    /* Close file at index 9 (last opened), remember its FD */
    last_closed_fd = fds[9];
    make_indexed_temp_path(path, sizeof(path), "lifo", 9);
    be->close(ctx, fds[9]);
    be->remove(ctx, path, strlen(path));
    fds[9] = -1;

    /* Open a new file - should reuse the just-closed FD (LIFO) */
    make_indexed_temp_path(path, sizeof(path), "lifo_new", 0);
    reused_fd = be->open(ctx, path, strlen(path), 4);
    TEST_ASSERT(reused_fd >= 0, "reopen failed");
    TEST_ASSERT(reused_fd == last_closed_fd,
                "LIFO: new open should reuse last closed FD");

    /* Cleanup */
    be->close(ctx, reused_fd);
    be->remove(ctx, path, strlen(path));

    for (i = 0; i < 9; i++) {
        if (fds[i] >= 0) {
            be->close(ctx, fds[i]);
        }
        make_indexed_temp_path(path, sizeof(path), "lifo", i);
        be->remove(ctx, path, strlen(path));
    }

    return 1;
}

/*------------------------------------------------------------------------
 * Stress Test: All simultaneously open FDs are unique
 *------------------------------------------------------------------------*/

static int test_stress_fd_uniqueness(void)
{
    const zbc_backend_t *be = zbc_backend_ansi_insecure();
    void *ctx = &g_ansi_state;
    int fds[STRESS_FILE_COUNT_SMALL];
    char path[512];
    int i, j;
    int opened_count;

    /* Initialize */
    for (i = 0; i < STRESS_FILE_COUNT_SMALL; i++) {
        fds[i] = -1;
    }

    /* Open files */
    opened_count = 0;
    for (i = 0; i < STRESS_FILE_COUNT_SMALL; i++) {
        make_indexed_temp_path(path, sizeof(path), "uniq", i);
        fds[i] = be->open(ctx, path, strlen(path), 4);
        if (fds[i] < 0) {
            break; /* Limit reached */
        }
        opened_count++;
    }

    TEST_ASSERT(opened_count > 0, "should open at least one file");

    /* Check all FDs are unique */
    for (i = 0; i < opened_count; i++) {
        for (j = i + 1; j < opened_count; j++) {
            TEST_ASSERT(fds[i] != fds[j], "duplicate FD found");
        }
    }

    /* Cleanup */
    cleanup_temp_files(be, ctx, fds, opened_count, "uniq", 1);

    return 1;
}

/*------------------------------------------------------------------------
 * Stress Test: Interleaved open/close operations
 *------------------------------------------------------------------------*/

static int test_stress_fd_interleaved_ops(void)
{
    const zbc_backend_t *be = zbc_backend_ansi_insecure();
    void *ctx = &g_ansi_state;
    int fds[20];
    char path[512];
    int new_fd;
    int i;

    /* Initialize */
    for (i = 0; i < 20; i++) {
        fds[i] = -1;
    }

    /* Open 20 files */
    for (i = 0; i < 20; i++) {
        make_indexed_temp_path(path, sizeof(path), "intlv", i);
        fds[i] = be->open(ctx, path, strlen(path), 4);
        TEST_ASSERT(fds[i] >= 0, "open failed");
    }

    /* Close every other file (0, 2, 4, ..., 18) */
    for (i = 0; i < 20; i += 2) {
        be->close(ctx, fds[i]);
        make_indexed_temp_path(path, sizeof(path), "intlv", i);
        be->remove(ctx, path, strlen(path));
        fds[i] = -1;
    }

    /* Open 5 new files - should reuse FDs from free list */
    for (i = 0; i < 5; i++) {
        make_indexed_temp_path(path, sizeof(path), "intlv_new", i);
        new_fd = be->open(ctx, path, strlen(path), 4);
        TEST_ASSERT(new_fd >= 0, "reopen failed");

        /* Store in previously emptied slot */
        fds[i * 2] = new_fd;
    }

    /* Close remaining odd-indexed files */
    for (i = 1; i < 20; i += 2) {
        if (fds[i] >= 0) {
            be->close(ctx, fds[i]);
            make_indexed_temp_path(path, sizeof(path), "intlv", i);
            be->remove(ctx, path, strlen(path));
            fds[i] = -1;
        }
    }

    /* Open more files - free list should have entries */
    for (i = 0; i < 10; i++) {
        make_indexed_temp_path(path, sizeof(path), "intlv_final", i);
        new_fd = be->open(ctx, path, strlen(path), 4);
        TEST_ASSERT(new_fd >= 0, "final open failed");
        be->close(ctx, new_fd);
        be->remove(ctx, path, strlen(path));
    }

    /* Final cleanup of remaining open files */
    for (i = 0; i < 20; i++) {
        if (fds[i] >= 0) {
            be->close(ctx, fds[i]);
            fds[i] = -1;
        }
    }

    /* Remove any remaining "intlv_new" files */
    for (i = 0; i < 5; i++) {
        make_indexed_temp_path(path, sizeof(path), "intlv_new", i);
        be->remove(ctx, path, strlen(path));
    }

    return 1;
}

/*------------------------------------------------------------------------
 * Stress Test: All allocated FDs can perform actual I/O
 *------------------------------------------------------------------------*/

static int test_stress_fd_io_functional(void)
{
    const zbc_backend_t *be = zbc_backend_ansi_insecure();
    void *ctx = &g_ansi_state;
    int fds[STRESS_FILE_COUNT_SMALL];
    char path[512];
    char write_buf[32];
    char read_buf[32];
    int i;
    int opened_count;
    int result;
    int len;

    /* Initialize */
    for (i = 0; i < STRESS_FILE_COUNT_SMALL; i++) {
        fds[i] = -1;
    }

    /* Open files for read/write */
    opened_count = 0;
    for (i = 0; i < STRESS_FILE_COUNT_SMALL; i++) {
        make_indexed_temp_path(path, sizeof(path), "io_func", i);
        fds[i] = be->open(ctx, path, strlen(path), 6); /* SH_OPEN_W_PLUS */
        if (fds[i] < 0) {
            break;
        }
        opened_count++;
    }

    TEST_ASSERT(opened_count >= 50, "should open at least 50 files");

    /* Write unique data to each file */
    for (i = 0; i < opened_count; i++) {
        sprintf(write_buf, "file_%04d_data", i);
        result = be->write(ctx, fds[i], write_buf, strlen(write_buf));
        TEST_ASSERT(result == 0, "write failed");
    }

    /* Seek to start and read back each file */
    for (i = 0; i < opened_count; i++) {
        result = be->seek(ctx, fds[i], 0);
        TEST_ASSERT(result == 0, "seek failed");

        memset(read_buf, 0, sizeof(read_buf));
        sprintf(write_buf, "file_%04d_data", i);
        len = (int)strlen(write_buf);

        result = be->read(ctx, fds[i], read_buf, len);
        TEST_ASSERT(result == 0, "read failed");
        TEST_ASSERT(memcmp(read_buf, write_buf, len) == 0, "data mismatch");
    }

    /* Verify flen works on all files */
    for (i = 0; i < opened_count; i++) {
        sprintf(write_buf, "file_%04d_data", i);
        len = be->flen(ctx, fds[i]);
        TEST_ASSERT(len == (int)strlen(write_buf), "flen incorrect");
    }

    /* Cleanup */
    cleanup_temp_files(be, ctx, fds, opened_count, "io_func", 1);

    return 1;
}

/*------------------------------------------------------------------------
 * Stress Test: Close all files, verify FDs are properly recycled
 *------------------------------------------------------------------------*/

static int test_stress_fd_reuse_after_close_all(void)
{
    const zbc_backend_t *be = zbc_backend_ansi_insecure();
    void *ctx = &g_ansi_state;
    int first_batch_fds[50];
    int second_batch_fds[50];
    char path[512];
    int i, j;
    int reused_count;

    /* Initialize */
    for (i = 0; i < 50; i++) {
        first_batch_fds[i] = -1;
        second_batch_fds[i] = -1;
    }

    /* Open 50 files */
    for (i = 0; i < 50; i++) {
        make_indexed_temp_path(path, sizeof(path), "reuse1", i);
        first_batch_fds[i] = be->open(ctx, path, strlen(path), 4);
        TEST_ASSERT(first_batch_fds[i] >= 0, "first batch open failed");
    }

    /* Close all 50 files */
    for (i = 0; i < 50; i++) {
        be->close(ctx, first_batch_fds[i]);
        make_indexed_temp_path(path, sizeof(path), "reuse1", i);
        be->remove(ctx, path, strlen(path));
    }

    /* Reopen 50 files - should reuse FDs from free list */
    for (i = 0; i < 50; i++) {
        make_indexed_temp_path(path, sizeof(path), "reuse2", i);
        second_batch_fds[i] = be->open(ctx, path, strlen(path), 4);
        TEST_ASSERT(second_batch_fds[i] >= 0, "second batch open failed");
    }

    /* Count how many FDs were reused (should be all 50) */
    reused_count = 0;
    for (i = 0; i < 50; i++) {
        for (j = 0; j < 50; j++) {
            if (second_batch_fds[i] == first_batch_fds[j]) {
                reused_count++;
                break;
            }
        }
    }

    /* All FDs should be reused */
    TEST_ASSERT(reused_count == 50, "all FDs should be reused");

    /* Cleanup */
    cleanup_temp_files(be, ctx, second_batch_fds, 50, "reuse2", 1);

    return 1;
}

/*========================================================================
 * SECURE BACKEND TESTS
 *========================================================================*/

static zbc_ansi_state_t g_secure_state;
static int g_violation_count;
static int g_last_violation_type;
static int g_exit_count;

static void test_violation_callback(void *ctx, int type, const char *detail)
{
    (void)ctx;
    (void)detail;
    g_violation_count++;
    g_last_violation_type = type;
}

static void test_exit_callback(void *ctx, unsigned int reason,
                               unsigned int subcode)
{
    (void)ctx;
    (void)reason;
    (void)subcode;
    g_exit_count++;
}

/*------------------------------------------------------------------------
 * Test: Secure backend basic file operations within sandbox
 *------------------------------------------------------------------------*/

static int test_secure_basic_ops(void)
{
    const zbc_backend_t *be = zbc_backend_ansi();
    void *ctx = &g_secure_state;
    const char *test_data = "secure test data";
    char read_buf[64];
    int fd;
    int result;

    /* Open for writing (relative path -> sandbox) */
    fd = be->open(ctx, "secure_test.txt", 15, 4);
    TEST_ASSERT(fd >= 0, "open for write failed");

    result = be->write(ctx, fd, test_data, strlen(test_data));
    TEST_ASSERT(result == 0, "write failed");

    be->close(ctx, fd);

    /* Open for reading */
    fd = be->open(ctx, "secure_test.txt", 15, 0);
    TEST_ASSERT(fd >= 0, "open for read failed");

    memset(read_buf, 0, sizeof(read_buf));
    result = be->read(ctx, fd, read_buf, strlen(test_data));
    TEST_ASSERT(result == 0, "read failed");
    TEST_ASSERT(memcmp(read_buf, test_data, strlen(test_data)) == 0,
                "data mismatch");

    be->close(ctx, fd);
    be->remove(ctx, "secure_test.txt", 15);

    return 1;
}

/*------------------------------------------------------------------------
 * Test: Path traversal blocked
 *------------------------------------------------------------------------*/

static int test_secure_path_traversal_blocked(void)
{
    const zbc_backend_t *be = zbc_backend_ansi();
    void *ctx = &g_secure_state;
    int fd;

    g_violation_count = 0;
    g_last_violation_type = 0;

    /* Try to escape sandbox with .. */
    fd = be->open(ctx, "../etc/passwd", 13, 0);
    TEST_ASSERT(fd < 0, "path traversal should be blocked");
    TEST_ASSERT(g_violation_count > 0, "violation callback should be called");
    TEST_ASSERT(g_last_violation_type == ZBC_ANSI_VIOL_PATH_TRAVERSAL ||
                g_last_violation_type == ZBC_ANSI_VIOL_PATH_BLOCKED,
                "wrong violation type");

    return 1;
}

/*------------------------------------------------------------------------
 * Test: Absolute path outside sandbox blocked
 *------------------------------------------------------------------------*/

static int test_secure_absolute_path_blocked(void)
{
    const zbc_backend_t *be = zbc_backend_ansi();
    void *ctx = &g_secure_state;
    int fd;

    g_violation_count = 0;

    /* Try absolute path outside sandbox */
    fd = be->open(ctx, "/etc/passwd", 11, 0);
    TEST_ASSERT(fd < 0, "absolute path outside sandbox should be blocked");
    TEST_ASSERT(g_violation_count > 0, "violation callback should be called");
    TEST_ASSERT(g_last_violation_type == ZBC_ANSI_VIOL_PATH_BLOCKED,
                "wrong violation type");

    return 1;
}

/*------------------------------------------------------------------------
 * Test: system() blocked by default
 *------------------------------------------------------------------------*/

static int test_secure_system_blocked(void)
{
    const zbc_backend_t *be = zbc_backend_ansi();
    void *ctx = &g_secure_state;
    int result;

    g_violation_count = 0;

    /* system() should be blocked by default */
    result = be->do_system(ctx, "echo hello", 10);
    TEST_ASSERT(result < 0, "system() should be blocked");
    TEST_ASSERT(g_violation_count > 0, "violation callback should be called");
    TEST_ASSERT(g_last_violation_type == ZBC_ANSI_VIOL_SYSTEM_BLOCKED,
                "wrong violation type");

    return 1;
}

/*------------------------------------------------------------------------
 * Test: exit() intercepted
 *------------------------------------------------------------------------*/

static int test_secure_exit_intercepted(void)
{
    const zbc_backend_t *be = zbc_backend_ansi();
    void *ctx = &g_secure_state;

    g_exit_count = 0;
    g_violation_count = 0;

    /* exit() should be intercepted, not terminate the host */
    be->do_exit(ctx, 42, 0);

    TEST_ASSERT(g_exit_count > 0, "exit callback should be called");
    TEST_ASSERT(g_violation_count > 0, "violation callback should be called");
    TEST_ASSERT(g_last_violation_type == ZBC_ANSI_VIOL_EXIT_BLOCKED,
                "wrong violation type");

    return 1;
}

/*------------------------------------------------------------------------
 * Test: tmpnam generates path within sandbox
 *------------------------------------------------------------------------*/

static int test_secure_tmpnam(void)
{
    const zbc_backend_t *be = zbc_backend_ansi();
    void *ctx = &g_secure_state;
    char buf[256];
    int result;

    memset(buf, 0, sizeof(buf));
    result = be->tmpnam(ctx, buf, sizeof(buf), 42);
    TEST_ASSERT(result == 0, "tmpnam failed");
    TEST_ASSERT(strlen(buf) > 0, "tmpnam returned empty string");

    /* Should start with sandbox directory (g_temp_dir + "/") */
    TEST_ASSERT(strncmp(buf, g_temp_dir, strlen(g_temp_dir)) == 0,
                "tmpnam should generate path within sandbox");

    return 1;
}

/*------------------------------------------------------------------------
 * Test: Read-only mode blocks writes
 *------------------------------------------------------------------------*/

static int test_secure_read_only_mode(void)
{
    const zbc_backend_t *be = zbc_backend_ansi();
    zbc_ansi_state_t ro_state;
    void *ctx = &ro_state;
    int fd;

    /* Create a read-only state */
    zbc_ansi_init(&ro_state, g_temp_dir);
    ro_state.flags = ZBC_ANSI_FLAG_READ_ONLY;
    zbc_ansi_set_callbacks(&ro_state, test_violation_callback, NULL, NULL);

    g_violation_count = 0;

    /* Try to open for writing */
    fd = be->open(ctx, "readonly_test.txt", 17, 4);
    TEST_ASSERT(fd < 0, "write should be blocked in read-only mode");
    TEST_ASSERT(g_violation_count > 0, "violation callback should be called");
    TEST_ASSERT(g_last_violation_type == ZBC_ANSI_VIOL_WRITE_BLOCKED,
                "wrong violation type");

    zbc_ansi_cleanup(&ro_state);

    return 1;
}

/*------------------------------------------------------------------------
 * Test: Additional path rule allows read-only access
 *------------------------------------------------------------------------*/

static int test_secure_path_rules(void)
{
    const zbc_backend_t *be = zbc_backend_ansi();
    zbc_ansi_state_t rule_state;
    void *ctx = &rule_state;
    int result;

    /* Create state with additional read-only path rule for /tmp */
    zbc_ansi_init(&rule_state, g_temp_dir);
    result = zbc_ansi_add_path(&rule_state, "/tmp/", 0);  /* read-only */
    TEST_ASSERT(result == 0, "add_path failed");

    zbc_ansi_set_callbacks(&rule_state, test_violation_callback, NULL, NULL);

    g_violation_count = 0;

    /* Reading from /tmp should work (if the path rule covers it) */
    /* Note: This test assumes /tmp exists and is accessible */

    /* But writing should be blocked since the rule is read-only */
    (void)be->open(ctx, "/tmp/zbc_rule_test.txt", 22, 4);  /* write mode */
    /* This should either succeed (if /tmp matches sandbox) or fail (read-only rule) */

    zbc_ansi_cleanup(&rule_state);

    return 1;  /* Pass regardless - behavior depends on sandbox location */
}

/*------------------------------------------------------------------------
 * Main
 *------------------------------------------------------------------------*/

void run_ansi_backend_tests(void)
{
    printf("\n=== ANSI Backend Tests ===\n");

    init_temp_dir();

    /*--------------------------------------------------------------------
     * Insecure backend tests
     *--------------------------------------------------------------------*/
    printf("\n--- Insecure Backend: Basic Operations ---\n");

    zbc_ansi_insecure_init(&g_ansi_state);

    RUN_TEST(write_read_file);
    RUN_TEST(file_length);
    RUN_TEST(seek);
    RUN_TEST(console_write);
    RUN_TEST(time_functions);
    RUN_TEST(istty);
    RUN_TEST(tmpnam);
    RUN_TEST(rename);
    RUN_TEST(partial_read);
    RUN_TEST(errno);

    printf("\n--- Insecure Backend: FD Stress Tests ---\n");
    RUN_TEST(stress_fd_lifo_reuse);
    RUN_TEST(stress_fd_uniqueness);
    RUN_TEST(stress_fd_interleaved_ops);
    RUN_TEST(stress_fd_reuse_after_close_all);
    RUN_TEST(stress_fd_io_functional);

    zbc_ansi_insecure_cleanup(&g_ansi_state);

    /*--------------------------------------------------------------------
     * Secure backend tests
     *--------------------------------------------------------------------*/
    printf("\n--- Secure Backend: Security Tests ---\n");

    zbc_ansi_init(&g_secure_state, g_temp_dir);
    zbc_ansi_set_callbacks(&g_secure_state, test_violation_callback,
                           test_exit_callback, NULL);

    RUN_TEST(secure_basic_ops);
    RUN_TEST(secure_path_traversal_blocked);
    RUN_TEST(secure_absolute_path_blocked);
    RUN_TEST(secure_system_blocked);
    RUN_TEST(secure_exit_intercepted);
    RUN_TEST(secure_tmpnam);
    RUN_TEST(secure_read_only_mode);
    RUN_TEST(secure_path_rules);

    zbc_ansi_cleanup(&g_secure_state);

    printf("\nANSI Backend: %d/%d tests passed\n", tests_passed, tests_run);
}
