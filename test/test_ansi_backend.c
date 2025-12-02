/*
 * ANSI Backend Integration Tests
 *
 * Tests the ANSI backend with real file I/O operations.
 * Creates actual files, writes data, reads it back, verifies results.
 */

#include "zbc_semihost.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;

/*------------------------------------------------------------------------
 * Portable temp directory handling
 *------------------------------------------------------------------------*/

static char g_temp_dir[512];

static void init_temp_dir(void)
{
    const char *tmp;

    /* Try standard environment variables */
    tmp = getenv("TMPDIR");
    if (!tmp) tmp = getenv("TMP");
    if (!tmp) tmp = getenv("TEMP");

#ifdef _WIN32
    if (!tmp) tmp = "C:\\Windows\\Temp";
#else
    if (!tmp) tmp = "/tmp";
#endif

    strncpy(g_temp_dir, tmp, sizeof(g_temp_dir) - 1);
    g_temp_dir[sizeof(g_temp_dir) - 1] = '\0';
}

static void make_temp_path(char *buf, size_t buf_size, const char *filename)
{
    size_t dir_len = strlen(g_temp_dir);

#ifdef _WIN32
    snprintf(buf, buf_size, "%s\\%s", g_temp_dir, filename);
#else
    snprintf(buf, buf_size, "%s/%s", g_temp_dir, filename);
#endif
    (void)dir_len;
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
 * Test: Open, Write, Close, Open, Read, Verify, Close, Remove
 *------------------------------------------------------------------------*/

static int test_write_read_file(void)
{
    const zbc_backend_t *be = zbc_backend_ansi();
    void *ctx = NULL;
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
    const zbc_backend_t *be = zbc_backend_ansi();
    void *ctx = NULL;
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
    const zbc_backend_t *be = zbc_backend_ansi();
    void *ctx = NULL;
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
    const zbc_backend_t *be = zbc_backend_ansi();
    void *ctx = NULL;

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
    const zbc_backend_t *be = zbc_backend_ansi();
    void *ctx = NULL;
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
    const zbc_backend_t *be = zbc_backend_ansi();
    void *ctx = NULL;
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
    const zbc_backend_t *be = zbc_backend_ansi();
    void *ctx = NULL;
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
    const zbc_backend_t *be = zbc_backend_ansi();
    void *ctx = NULL;
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
    const zbc_backend_t *be = zbc_backend_ansi();
    void *ctx = NULL;
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
    const zbc_backend_t *be = zbc_backend_ansi();
    void *ctx = NULL;
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
 * Main
 *------------------------------------------------------------------------*/

void run_ansi_backend_tests(void)
{
    printf("\n=== ANSI Backend Tests ===\n");

    init_temp_dir();

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

    printf("\nANSI Backend: %d/%d tests passed\n", tests_passed, tests_run);

    /* Cleanup any open files */
    zbc_backend_ansi_cleanup();
}
