/*
 * ZBC High-Level API Tests
 *
 * Tests zbc_api_* wrapper functions with REAL file I/O using the ANSI backend.
 * This verifies the full round-trip: client builds request -> host processes
 * with real backend -> client parses response.
 */

#include "mock_device.h"
#include "mock_memory.h"
#include "test_harness.h"
#include "zbc_api.h"
#include <stdlib.h>
#include <string.h>

/*------------------------------------------------------------------------
 * Temp directory handling (same pattern as test_ansi_common.h)
 *------------------------------------------------------------------------*/

#define MAX_TEMP_DIR_LEN 256

static char g_temp_dir[MAX_TEMP_DIR_LEN];

static void init_temp_dir(void)
{
    const char *tmp;
    size_t len;

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
    size_t total = dir_len + 1 + file_len + 1;

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

/*------------------------------------------------------------------------
 * Doorbell callback for testing
 *------------------------------------------------------------------------*/

static void doorbell_hook(void *ctx) {
    mock_device_doorbell((mock_device_t *)ctx);
}

/*------------------------------------------------------------------------
 * Backend state (shared across tests)
 *------------------------------------------------------------------------*/

static zbc_ansi_insecure_state_t g_ansi_state;

/*------------------------------------------------------------------------
 * Helper: Set up API with ANSI backend for real I/O
 *------------------------------------------------------------------------*/

static void setup_api_ansi(zbc_api_t *api, zbc_client_state_t *client,
                           mock_device_t *dev, void *buf, size_t buf_size)
{
    mock_device_init_ansi(dev, &g_ansi_state);
    zbc_client_init(client, dev->regs);
    client->doorbell_callback = doorbell_hook;
    client->doorbell_ctx = dev;
    zbc_api_init(api, client, buf, buf_size);
}

/*------------------------------------------------------------------------
 * Initialization & Utility Tests
 *------------------------------------------------------------------------*/

static void test_api_init(void)
{
    zbc_client_state_t client;
    zbc_api_t api;
    uint8_t buf[256];

    zbc_api_init(&api, &client, buf, sizeof(buf));

    TEST_ASSERT_EQ((uintptr_t)api.client, (uintptr_t)&client);
    TEST_ASSERT_EQ((uintptr_t)api.buf, (uintptr_t)buf);
    TEST_ASSERT_EQ((int)api.buf_size, (int)sizeof(buf));
    TEST_ASSERT_EQ(api.last_errno, 0);
}

static void test_api_errno(void)
{
    zbc_api_t api;

    api.last_errno = 42;
    TEST_ASSERT_EQ(zbc_api_errno(&api), 42);

    api.last_errno = 0;
    TEST_ASSERT_EQ(zbc_api_errno(&api), 0);
}

static void test_api_iserror(void)
{
    TEST_ASSERT_EQ(zbc_api_iserror(-1), 1);
    TEST_ASSERT_EQ(zbc_api_iserror(-100), 1);
    TEST_ASSERT_EQ(zbc_api_iserror(0), 0);
    TEST_ASSERT_EQ(zbc_api_iserror(1), 0);
    TEST_ASSERT_EQ(zbc_api_iserror(100), 0);
}

/*------------------------------------------------------------------------
 * File Operation Tests - Real I/O
 *------------------------------------------------------------------------*/

static void test_api_write_read_file(void)
{
    GUARDED_BUF(buf, 512);
    zbc_client_state_t client;
    mock_device_t dev;
    zbc_api_t api;
    char filepath[512];
    const char *test_data = "Hello from zbc_api test!";
    size_t data_len = strlen(test_data);
    char read_buf[64];
    int fd;
    int result;

    GUARDED_INIT(buf);
    setup_api_ansi(&api, &client, &dev, buf, buf_size);

    make_temp_path(filepath, sizeof(filepath), "zbc_api_test.txt");

    /* Open for writing */
    fd = zbc_api_open(&api, filepath, SH_OPEN_W);
    TEST_ASSERT(fd >= 0);

    /* Write data */
    result = zbc_api_write(&api, fd, test_data, data_len);
    TEST_ASSERT_EQ(result, 0);  /* 0 bytes NOT written = success */

    /* Close */
    result = zbc_api_close(&api, fd);
    TEST_ASSERT_EQ(result, 0);

    /* Open for reading */
    fd = zbc_api_open(&api, filepath, SH_OPEN_R);
    TEST_ASSERT(fd >= 0);

    /* Read data back */
    memset(read_buf, 0, sizeof(read_buf));
    result = zbc_api_read(&api, fd, read_buf, data_len);
    TEST_ASSERT_EQ(result, 0);  /* 0 bytes NOT read = success */

    /* Verify data matches */
    TEST_ASSERT_MEM_EQ(read_buf, test_data, data_len);

    /* Close and cleanup */
    zbc_api_close(&api, fd);
    zbc_api_remove(&api, filepath);

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

static void test_api_flen(void)
{
    GUARDED_BUF(buf, 512);
    zbc_client_state_t client;
    mock_device_t dev;
    zbc_api_t api;
    char filepath[512];
    const char *test_data = "1234567890";  /* 10 bytes */
    int fd;
    intmax_t len;

    GUARDED_INIT(buf);
    setup_api_ansi(&api, &client, &dev, buf, buf_size);

    make_temp_path(filepath, sizeof(filepath), "zbc_api_flen.txt");

    /* Create file with known content */
    fd = zbc_api_open(&api, filepath, SH_OPEN_W);
    TEST_ASSERT(fd >= 0);

    zbc_api_write(&api, fd, test_data, 10);

    /* Get file length */
    len = zbc_api_flen(&api, fd);
    TEST_ASSERT_EQ((int)len, 10);

    zbc_api_close(&api, fd);
    zbc_api_remove(&api, filepath);

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

static void test_api_seek(void)
{
    GUARDED_BUF(buf, 512);
    zbc_client_state_t client;
    mock_device_t dev;
    zbc_api_t api;
    char filepath[512];
    const char *test_data = "ABCDEFGHIJ";  /* 10 bytes */
    char read_buf[4];
    int fd;
    int result;

    GUARDED_INIT(buf);
    setup_api_ansi(&api, &client, &dev, buf, buf_size);

    make_temp_path(filepath, sizeof(filepath), "zbc_api_seek.txt");

    /* Create file */
    fd = zbc_api_open(&api, filepath, SH_OPEN_W);
    TEST_ASSERT(fd >= 0);
    zbc_api_write(&api, fd, test_data, 10);
    zbc_api_close(&api, fd);

    /* Reopen for reading */
    fd = zbc_api_open(&api, filepath, SH_OPEN_R);
    TEST_ASSERT(fd >= 0);

    /* Seek to position 5 and read */
    result = zbc_api_seek(&api, fd, 5);
    TEST_ASSERT_EQ(result, 0);

    memset(read_buf, 0, sizeof(read_buf));
    result = zbc_api_read(&api, fd, read_buf, 3);
    TEST_ASSERT_EQ(result, 0);
    TEST_ASSERT_MEM_EQ(read_buf, "FGH", 3);

    zbc_api_close(&api, fd);
    zbc_api_remove(&api, filepath);

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

static void test_api_istty(void)
{
    GUARDED_BUF(buf, 512);
    zbc_client_state_t client;
    mock_device_t dev;
    zbc_api_t api;
    char filepath[512];
    int fd;
    int result;

    GUARDED_INIT(buf);
    setup_api_ansi(&api, &client, &dev, buf, buf_size);

    /* stdin/stdout/stderr should be TTYs */
    result = zbc_api_istty(&api, 0);
    TEST_ASSERT_EQ(result, 1);

    result = zbc_api_istty(&api, 1);
    TEST_ASSERT_EQ(result, 1);

    result = zbc_api_istty(&api, 2);
    TEST_ASSERT_EQ(result, 1);

    /* File should not be TTY */
    make_temp_path(filepath, sizeof(filepath), "zbc_api_istty.txt");
    fd = zbc_api_open(&api, filepath, SH_OPEN_W);
    if (fd >= 0) {
        result = zbc_api_istty(&api, fd);
        TEST_ASSERT_EQ(result, 0);
        zbc_api_close(&api, fd);
        zbc_api_remove(&api, filepath);
    }

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

static void test_api_rename(void)
{
    GUARDED_BUF(buf, 512);
    zbc_client_state_t client;
    mock_device_t dev;
    zbc_api_t api;
    char old_path[512];
    char new_path[512];
    const char *test_data = "rename test";
    char read_buf[32];
    int fd;
    int result;

    GUARDED_INIT(buf);
    setup_api_ansi(&api, &client, &dev, buf, buf_size);

    make_temp_path(old_path, sizeof(old_path), "zbc_api_rename_old.txt");
    make_temp_path(new_path, sizeof(new_path), "zbc_api_rename_new.txt");

    /* Create file with old name */
    fd = zbc_api_open(&api, old_path, SH_OPEN_W);
    TEST_ASSERT(fd >= 0);
    zbc_api_write(&api, fd, test_data, strlen(test_data));
    zbc_api_close(&api, fd);

    /* Rename */
    result = zbc_api_rename(&api, old_path, new_path);
    TEST_ASSERT_EQ(result, 0);

    /* Verify old name doesn't exist */
    fd = zbc_api_open(&api, old_path, SH_OPEN_R);
    TEST_ASSERT(fd < 0);

    /* Verify new name exists with correct content */
    fd = zbc_api_open(&api, new_path, SH_OPEN_R);
    TEST_ASSERT(fd >= 0);

    memset(read_buf, 0, sizeof(read_buf));
    zbc_api_read(&api, fd, read_buf, strlen(test_data));
    TEST_ASSERT_MEM_EQ(read_buf, test_data, strlen(test_data));

    zbc_api_close(&api, fd);
    zbc_api_remove(&api, new_path);

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

static void test_api_tmpnam(void)
{
    GUARDED_BUF(buf, 512);
    zbc_client_state_t client;
    mock_device_t dev;
    zbc_api_t api;
    char tmpname[64];
    int result;

    GUARDED_INIT(buf);
    setup_api_ansi(&api, &client, &dev, buf, buf_size);

    memset(tmpname, 0, sizeof(tmpname));
    result = zbc_api_tmpnam(&api, tmpname, sizeof(tmpname), 42);
    TEST_ASSERT_EQ(result, 0);
    TEST_ASSERT(strlen(tmpname) > 0);

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

static void test_api_partial_read(void)
{
    GUARDED_BUF(buf, 512);
    zbc_client_state_t client;
    mock_device_t dev;
    zbc_api_t api;
    char filepath[512];
    const char *test_data = "SHORT";  /* 5 bytes */
    char read_buf[100];
    int fd;
    int result;

    GUARDED_INIT(buf);
    setup_api_ansi(&api, &client, &dev, buf, buf_size);

    make_temp_path(filepath, sizeof(filepath), "zbc_api_partial.txt");

    /* Create file with 5 bytes */
    fd = zbc_api_open(&api, filepath, SH_OPEN_W);
    TEST_ASSERT(fd >= 0);
    zbc_api_write(&api, fd, test_data, 5);
    zbc_api_close(&api, fd);

    /* Read requesting 100 bytes */
    fd = zbc_api_open(&api, filepath, SH_OPEN_R);
    TEST_ASSERT(fd >= 0);

    memset(read_buf, 0, sizeof(read_buf));
    result = zbc_api_read(&api, fd, read_buf, 100);

    /* Result should be 95 (100 - 5 = bytes NOT read) */
    TEST_ASSERT_EQ(result, 95);
    TEST_ASSERT_MEM_EQ(read_buf, test_data, 5);

    zbc_api_close(&api, fd);
    zbc_api_remove(&api, filepath);

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Console Operation Tests
 *------------------------------------------------------------------------*/

static void test_api_console(void)
{
    GUARDED_BUF(buf, 256);
    zbc_client_state_t client;
    mock_device_t dev;
    zbc_api_t api;

    GUARDED_INIT(buf);
    setup_api_ansi(&api, &client, &dev, buf, buf_size);

    /* These just shouldn't crash - output goes to stdout */
    zbc_api_writec(&api, 'X');
    zbc_api_write0(&api, "[test_api_console OK]\n");

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Time Operation Tests
 *------------------------------------------------------------------------*/

static void test_api_time_functions(void)
{
    GUARDED_BUF(buf, 256);
    zbc_client_state_t client;
    mock_device_t dev;
    zbc_api_t api;
    int clock_val;
    int time_val;
    int tickfreq;

    GUARDED_INIT(buf);
    setup_api_ansi(&api, &client, &dev, buf, buf_size);

    clock_val = zbc_api_clock(&api);
    TEST_ASSERT(clock_val >= 0);

    time_val = zbc_api_time(&api);
    TEST_ASSERT(time_val > 0);

    tickfreq = zbc_api_tickfreq(&api);
    TEST_ASSERT(tickfreq > 0);

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

static void test_api_elapsed(void)
{
    GUARDED_BUF(buf, 256);
    zbc_client_state_t client;
    mock_device_t dev;
    zbc_api_t api;
    uint64_t ticks;
    int result;

    GUARDED_INIT(buf);
    setup_api_ansi(&api, &client, &dev, buf, buf_size);

    result = zbc_api_elapsed(&api, &ticks);
    TEST_ASSERT_EQ(result, 0);
    /* ticks value is implementation-dependent, just check call succeeded */

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

static void test_api_timer_config(void)
{
    GUARDED_BUF(buf, 256);
    zbc_client_state_t client;
    mock_device_t dev;
    zbc_api_t api;
    int result;

    GUARDED_INIT(buf);
    setup_api_ansi(&api, &client, &dev, buf, buf_size);

    /* Configure 1000 Hz timer */
    result = zbc_api_timer_config(&api, 1000);
    TEST_ASSERT_EQ(result, 0);

    /* Disable timer */
    result = zbc_api_timer_config(&api, 0);
    TEST_ASSERT_EQ(result, 0);

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * System Operation Tests
 *------------------------------------------------------------------------*/

static void test_api_get_errno(void)
{
    GUARDED_BUF(buf, 512);
    zbc_client_state_t client;
    mock_device_t dev;
    zbc_api_t api;
    char filepath[512];
    int fd;
    int err;

    GUARDED_INIT(buf);
    setup_api_ansi(&api, &client, &dev, buf, buf_size);

    make_temp_path(filepath, sizeof(filepath), "zbc_api_nonexistent_12345.txt");

    /* Try to open nonexistent file */
    fd = zbc_api_open(&api, filepath, SH_OPEN_R);
    TEST_ASSERT(fd < 0);

    /* Check that errno was set */
    err = zbc_api_get_errno(&api);
    TEST_ASSERT(err != 0);

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

static void test_api_get_cmdline(void)
{
    GUARDED_BUF(buf, 512);
    zbc_client_state_t client;
    mock_device_t dev;
    zbc_api_t api;
    char cmdline[128];
    int result;

    GUARDED_INIT(buf);
    setup_api_ansi(&api, &client, &dev, buf, buf_size);

    memset(cmdline, 0, sizeof(cmdline));
    result = zbc_api_get_cmdline(&api, cmdline, sizeof(cmdline));
    TEST_ASSERT_EQ(result, 0);
    /* cmdline content is implementation-dependent */

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

static void test_api_heapinfo(void)
{
    GUARDED_BUF(buf, 512);
    zbc_client_state_t client;
    mock_device_t dev;
    zbc_api_t api;
    uintptr_t heap_base, heap_limit, stack_base, stack_limit;
    int result;

    GUARDED_INIT(buf);
    setup_api_ansi(&api, &client, &dev, buf, buf_size);

    result = zbc_api_heapinfo(&api, &heap_base, &heap_limit,
                              &stack_base, &stack_limit);
    TEST_ASSERT_EQ(result, 0);
    /* Values are implementation-dependent */

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Run all API tests
 *------------------------------------------------------------------------*/

void run_api_tests(void)
{
    BEGIN_SUITE("High-Level API (ANSI Backend)");

    init_temp_dir();
    zbc_ansi_insecure_init(&g_ansi_state);

    /* Initialization & Utility */
    RUN_TEST(api_init);
    RUN_TEST(api_errno);
    RUN_TEST(api_iserror);

    /* File Operations - Real I/O */
    RUN_TEST(api_write_read_file);
    RUN_TEST(api_flen);
    RUN_TEST(api_seek);
    RUN_TEST(api_istty);
    RUN_TEST(api_rename);
    RUN_TEST(api_tmpnam);
    RUN_TEST(api_partial_read);

    /* Console Operations */
    RUN_TEST(api_console);

    /* Time Operations */
    RUN_TEST(api_time_functions);
    RUN_TEST(api_elapsed);
    RUN_TEST(api_timer_config);

    /* System Operations */
    RUN_TEST(api_get_errno);
    RUN_TEST(api_get_cmdline);
    RUN_TEST(api_heapinfo);

    zbc_ansi_insecure_cleanup(&g_ansi_state);

    END_SUITE();
}
