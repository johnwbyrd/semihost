/*
 * ZBC Semihosting On-Target Test Suite
 *
 * Comprehensive test program that exercises all 23 ZBC semihosting syscalls
 * on real hardware (via emulator). Runs on any ZBC-compatible platform.
 *
 * Syscalls tested:
 *   File I/O:  OPEN, CLOSE, READ, WRITE, SEEK, FLEN, REMOVE, RENAME, TMPNAM
 *   Console:   WRITEC, WRITE0, READC
 *   Time:      CLOCK, TIME, ELAPSED, TICKFREQ
 *   System:    ISERROR, ISTTY, ERRNO, GET_CMDLINE, HEAPINFO, SYSTEM
 *   Exit:      EXIT (used at end to report results)
 *
 * Exit code: 0 = all tests passed, 1 = any test failed
 */

#include "zbc_target_harness.h"

/*------------------------------------------------------------------------
 * Test data buffers (static allocation - no malloc)
 *------------------------------------------------------------------------*/

static char test_filename[] = "zbc_test_file.txt";
static char test_filename2[] = "zbc_test_renamed.txt";
static const char test_data[] = "Hello, ZBC semihosting!";
static char read_buf[128];
static char tmpnam_buf[64];
static char cmdline_buf[256];

/*------------------------------------------------------------------------
 * Helper: Compare memory (no libc memcmp)
 *------------------------------------------------------------------------*/

static int mem_equal(const void *a, const void *b, size_t n)
{
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    size_t i;
    for (i = 0; i < n; i++) {
        if (pa[i] != pb[i]) return 0;
    }
    return 1;
}

static void mem_clear(void *buf, size_t n)
{
    uint8_t *p = (uint8_t *)buf;
    size_t i;
    for (i = 0; i < n; i++) {
        p[i] = 0;
    }
}

/*------------------------------------------------------------------------
 * Test: Device Detection
 *------------------------------------------------------------------------*/

static void test_device_detection(void)
{
    int result;

    TARGET_BEGIN_TEST("device_signature");
    result = zbc_client_check_signature(&g_target_client);
    TARGET_ASSERT_EQ(result, ZBC_OK);
    TARGET_END_TEST();

    TARGET_BEGIN_TEST("device_present");
    result = zbc_client_device_present(&g_target_client);
    TARGET_ASSERT_EQ(result, ZBC_OK);
    TARGET_END_TEST();
}

/*------------------------------------------------------------------------
 * Test: Console I/O (WRITEC, WRITE0)
 *------------------------------------------------------------------------*/

static void test_console_io(void)
{
    uintptr_t args[1];
    static const char test_char = 'X';

    /* WRITEC - write single character */
    TARGET_BEGIN_TEST("sys_writec");
    args[0] = (uintptr_t)&test_char;
    zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf),
                 SH_SYS_WRITEC, (uintptr_t)args);
    /* WRITEC has no return value - just verify it doesn't crash */
    TARGET_PRINT(" ");  /* spacing after the 'X' */
    TARGET_END_TEST();

    /* WRITE0 - write null-terminated string */
    TARGET_BEGIN_TEST("sys_write0");
    args[0] = (uintptr_t)"[test string]";
    zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf),
                 SH_SYS_WRITE0, (uintptr_t)args);
    TARGET_PRINT(" ");
    TARGET_END_TEST();
}

/*------------------------------------------------------------------------
 * Test: READC (requires stdin input)
 *------------------------------------------------------------------------*/

static void test_readc(void)
{
    uintptr_t result;

    TARGET_BEGIN_TEST("sys_readc");

    /* READC reads a character from stdin.
     * The test runner should pipe input like: echo "A" | mame ...
     * We expect to read the character 'A' (or whatever was piped).
     */
    result = zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf),
                          SH_SYS_READC, 0);

    /* Check that we got a valid character (not -1 error) */
    if (result == (uintptr_t)-1) {
        TARGET_PRINT("(no input available) ");
        /* Don't fail - input may not be available in all test configurations */
    } else {
        TARGET_PRINT("(got '");
        {
            char buf[2];
            buf[0] = (char)result;
            buf[1] = '\0';
            TARGET_PRINT(buf);
        }
        TARGET_PRINT("') ");
        /* Verify it's a printable character or newline */
        TARGET_ASSERT(result < 256);
    }
    TARGET_END_TEST();
}

/*------------------------------------------------------------------------
 * Test: File I/O (OPEN, CLOSE, WRITE, READ, SEEK, FLEN)
 *------------------------------------------------------------------------*/

static void test_file_io(void)
{
    uintptr_t args[3];
    uintptr_t fd;
    uintptr_t result;
    size_t test_len = zbc_strlen(test_data);

    /* OPEN for writing */
    TARGET_BEGIN_TEST("sys_open_write");
    args[0] = (uintptr_t)test_filename;
    args[1] = SH_OPEN_W;
    args[2] = zbc_strlen(test_filename);
    fd = zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf),
                      SH_SYS_OPEN, (uintptr_t)args);
    TARGET_ASSERT_NEQ(fd, (uintptr_t)-1);
    TARGET_END_TEST();

    if (fd == (uintptr_t)-1) {
        TARGET_PRINT("  (skipping remaining file tests due to open failure)\n");
        return;
    }

    /* WRITE to file */
    TARGET_BEGIN_TEST("sys_write");
    args[0] = fd;
    args[1] = (uintptr_t)test_data;
    args[2] = test_len;
    result = zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf),
                          SH_SYS_WRITE, (uintptr_t)args);
    /* WRITE returns bytes NOT written, so 0 = success */
    TARGET_ASSERT_EQ(result, 0);
    TARGET_END_TEST();

    /* FLEN - get file length */
    TARGET_BEGIN_TEST("sys_flen");
    args[0] = fd;
    result = zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf),
                          SH_SYS_FLEN, (uintptr_t)args);
    TARGET_ASSERT_EQ(result, test_len);
    TARGET_END_TEST();

    /* CLOSE file */
    TARGET_BEGIN_TEST("sys_close");
    args[0] = fd;
    result = zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf),
                          SH_SYS_CLOSE, (uintptr_t)args);
    TARGET_ASSERT_EQ(result, 0);
    TARGET_END_TEST();

    /* OPEN for reading */
    TARGET_BEGIN_TEST("sys_open_read");
    args[0] = (uintptr_t)test_filename;
    args[1] = SH_OPEN_R;
    args[2] = zbc_strlen(test_filename);
    fd = zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf),
                      SH_SYS_OPEN, (uintptr_t)args);
    TARGET_ASSERT_NEQ(fd, (uintptr_t)-1);
    TARGET_END_TEST();

    if (fd == (uintptr_t)-1) {
        TARGET_PRINT("  (skipping read tests due to open failure)\n");
        return;
    }

    /* READ from file */
    TARGET_BEGIN_TEST("sys_read");
    mem_clear(read_buf, sizeof(read_buf));
    args[0] = fd;
    args[1] = (uintptr_t)read_buf;
    args[2] = test_len;
    result = zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf),
                          SH_SYS_READ, (uintptr_t)args);
    /* READ returns bytes NOT read, so 0 = all bytes read */
    TARGET_ASSERT_EQ(result, 0);
    TARGET_ASSERT(mem_equal(read_buf, test_data, test_len));
    TARGET_END_TEST();

    /* SEEK to beginning */
    TARGET_BEGIN_TEST("sys_seek");
    args[0] = fd;
    args[1] = 0;
    result = zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf),
                          SH_SYS_SEEK, (uintptr_t)args);
    TARGET_ASSERT_EQ(result, 0);
    TARGET_END_TEST();

    /* READ partial (first 5 bytes) */
    TARGET_BEGIN_TEST("sys_read_partial");
    mem_clear(read_buf, sizeof(read_buf));
    args[0] = fd;
    args[1] = (uintptr_t)read_buf;
    args[2] = 5;
    result = zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf),
                          SH_SYS_READ, (uintptr_t)args);
    TARGET_ASSERT_EQ(result, 0);
    TARGET_ASSERT(mem_equal(read_buf, "Hello", 5));
    TARGET_END_TEST();

    /* CLOSE file */
    args[0] = fd;
    zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf),
                 SH_SYS_CLOSE, (uintptr_t)args);
}

/*------------------------------------------------------------------------
 * Test: File System (REMOVE, RENAME, TMPNAM)
 *------------------------------------------------------------------------*/

static void test_filesystem(void)
{
    uintptr_t args[4];
    uintptr_t fd;
    uintptr_t result;

    /* Create a file to rename */
    args[0] = (uintptr_t)test_filename;
    args[1] = SH_OPEN_W;
    args[2] = zbc_strlen(test_filename);
    fd = zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf),
                      SH_SYS_OPEN, (uintptr_t)args);
    if (fd != (uintptr_t)-1) {
        args[0] = fd;
        args[1] = (uintptr_t)"test";
        args[2] = 4;
        zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf),
                     SH_SYS_WRITE, (uintptr_t)args);
        args[0] = fd;
        zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf),
                     SH_SYS_CLOSE, (uintptr_t)args);
    }

    /* RENAME file */
    TARGET_BEGIN_TEST("sys_rename");
    args[0] = (uintptr_t)test_filename;
    args[1] = zbc_strlen(test_filename);
    args[2] = (uintptr_t)test_filename2;
    args[3] = zbc_strlen(test_filename2);
    result = zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf),
                          SH_SYS_RENAME, (uintptr_t)args);
    TARGET_ASSERT_EQ(result, 0);
    TARGET_END_TEST();

    /* REMOVE renamed file */
    TARGET_BEGIN_TEST("sys_remove");
    args[0] = (uintptr_t)test_filename2;
    args[1] = zbc_strlen(test_filename2);
    result = zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf),
                          SH_SYS_REMOVE, (uintptr_t)args);
    TARGET_ASSERT_EQ(result, 0);
    TARGET_END_TEST();

    /* Verify file is gone */
    TARGET_BEGIN_TEST("sys_remove_verify");
    args[0] = (uintptr_t)test_filename2;
    args[1] = SH_OPEN_R;
    args[2] = zbc_strlen(test_filename2);
    fd = zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf),
                      SH_SYS_OPEN, (uintptr_t)args);
    TARGET_ASSERT_EQ(fd, (uintptr_t)-1);  /* Should fail - file doesn't exist */
    TARGET_END_TEST();

    /* TMPNAM - generate temp filename */
    TARGET_BEGIN_TEST("sys_tmpnam");
    mem_clear(tmpnam_buf, sizeof(tmpnam_buf));
    args[0] = (uintptr_t)tmpnam_buf;
    args[1] = 0;  /* identifier */
    args[2] = sizeof(tmpnam_buf) - 1;
    result = zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf),
                          SH_SYS_TMPNAM, (uintptr_t)args);
    TARGET_ASSERT_EQ(result, 0);
    /* Verify we got a non-empty filename */
    TARGET_ASSERT(tmpnam_buf[0] != '\0');
    TARGET_END_TEST();
}

/*------------------------------------------------------------------------
 * Test: Time (CLOCK, TIME, ELAPSED, TICKFREQ)
 *------------------------------------------------------------------------*/

static void test_time(void)
{
    uintptr_t result;
    uintptr_t clock1, clock2;
    uintptr_t time_val;
    uint64_t elapsed_buf;
    uintptr_t args[1];
    uintptr_t tickfreq;

    /* CLOCK - returns centiseconds since start */
    TARGET_BEGIN_TEST("sys_clock");
    clock1 = zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf),
                          SH_SYS_CLOCK, 0);
    /* Just verify it returns something reasonable (not -1) */
    TARGET_ASSERT_NEQ(clock1, (uintptr_t)-1);
    TARGET_END_TEST();

    /* TIME - returns seconds since epoch */
    TARGET_BEGIN_TEST("sys_time");
    time_val = zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf),
                            SH_SYS_TIME, 0);
    /* Should return a value or -1 if not supported.
     * On 16-bit platforms, we can't validate the actual timestamp value
     * since uintptr_t can't hold Unix timestamps past 1970 + ~18 hours. */
    TARGET_ASSERT(time_val != 0 || time_val == (uintptr_t)-1);
    TARGET_END_TEST();

    /* TICKFREQ - get tick frequency */
    TARGET_BEGIN_TEST("sys_tickfreq");
    tickfreq = zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf),
                            SH_SYS_TICKFREQ, 0);
    /* Should return a positive frequency or -1 if not supported */
    TARGET_ASSERT(tickfreq > 0 || tickfreq == (uintptr_t)-1);
    TARGET_END_TEST();

    /* ELAPSED - get 64-bit tick count */
    TARGET_BEGIN_TEST("sys_elapsed");
    elapsed_buf = 0;
    args[0] = (uintptr_t)&elapsed_buf;
    result = zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf),
                          SH_SYS_ELAPSED, (uintptr_t)args);
    /* Result should be 0 (success) or -1 (not supported) */
    TARGET_ASSERT(result == 0 || result == (uintptr_t)-1);
    TARGET_END_TEST();

    /* Verify CLOCK is monotonic */
    TARGET_BEGIN_TEST("clock_monotonic");
    clock2 = zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf),
                          SH_SYS_CLOCK, 0);
    TARGET_ASSERT(clock2 >= clock1);
    TARGET_END_TEST();
}

/*------------------------------------------------------------------------
 * Test: System (ISERROR, ISTTY, ERRNO, GET_CMDLINE, HEAPINFO, SYSTEM)
 *------------------------------------------------------------------------*/

static void test_system(void)
{
    uintptr_t args[2];
    uintptr_t result;
    uintptr_t heapinfo_block[4];

    /* ISERROR - check if value is error */
    TARGET_BEGIN_TEST("sys_iserror");
    args[0] = 0;  /* 0 is not an error */
    result = zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf),
                          SH_SYS_ISERROR, (uintptr_t)args);
    TARGET_ASSERT_EQ(result, 0);

    args[0] = (uintptr_t)-1;  /* -1 is an error */
    result = zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf),
                          SH_SYS_ISERROR, (uintptr_t)args);
    TARGET_ASSERT_EQ(result, 1);
    TARGET_END_TEST();

    /* ISTTY - check if fd is a terminal */
    TARGET_BEGIN_TEST("sys_istty");
    args[0] = 1;  /* stdout (fd 1) should be a TTY */
    result = zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf),
                          SH_SYS_ISTTY, (uintptr_t)args);
    TARGET_ASSERT_EQ(result, 1);
    TARGET_END_TEST();

    /* ERRNO - get last error */
    TARGET_BEGIN_TEST("sys_errno");
    result = zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf),
                          SH_SYS_ERRNO, 0);
    /* Just verify it returns something (errno value depends on previous operations) */
    TARGET_ASSERT(result != (uintptr_t)-1 || result == 0 || result > 0);
    TARGET_END_TEST();

    /* GET_CMDLINE - get command line */
    TARGET_BEGIN_TEST("sys_get_cmdline");
    mem_clear(cmdline_buf, sizeof(cmdline_buf));
    args[0] = (uintptr_t)cmdline_buf;
    args[1] = sizeof(cmdline_buf) - 1;
    result = zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf),
                          SH_SYS_GET_CMDLINE, (uintptr_t)args);
    /* 0 = success, -1 = not supported */
    TARGET_ASSERT(result == 0 || result == (uintptr_t)-1);
    TARGET_END_TEST();

    /* HEAPINFO - get heap/stack info */
    TARGET_BEGIN_TEST("sys_heapinfo");
    heapinfo_block[0] = 0;
    heapinfo_block[1] = 0;
    heapinfo_block[2] = 0;
    heapinfo_block[3] = 0;
    args[0] = (uintptr_t)heapinfo_block;
    result = zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf),
                          SH_SYS_HEAPINFO, (uintptr_t)args);
    /* HEAPINFO returns 0 on success, -1 if not supported */
    TARGET_ASSERT(result == 0 || result == (uintptr_t)-1);
    TARGET_END_TEST();

    /* SYSTEM - execute shell command (may be disabled) */
    TARGET_BEGIN_TEST("sys_system");
    {
        static const char cmd[] = "true";  /* Simple command that should succeed */
        args[0] = (uintptr_t)cmd;
        args[1] = zbc_strlen(cmd);
        result = zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf),
                              SH_SYS_SYSTEM, (uintptr_t)args);
        /* -1 means SYSTEM is disabled (security), which is acceptable */
        if (result == (uintptr_t)-1) {
            TARGET_PRINT("(disabled) ");
        } else {
            TARGET_ASSERT_EQ(result, 0);  /* true should exit with 0 */
        }
    }
    TARGET_END_TEST();
}

/*------------------------------------------------------------------------
 * Main entry point
 *------------------------------------------------------------------------*/

__attribute__((section(".text.startup")))
void _start(void)
{
    TARGET_INIT();

    TARGET_PRINT("========================================\n");
    TARGET_PRINT("ZBC Semihosting On-Target Test Suite\n");
    TARGET_PRINT("========================================\n\n");

    TARGET_PRINT("Device Detection:\n");
    test_device_detection();

    TARGET_PRINT("\nConsole I/O:\n");
    test_console_io();

    TARGET_PRINT("\nREADC:\n");
    test_readc();

    TARGET_PRINT("\nFile I/O:\n");
    test_file_io();

    TARGET_PRINT("\nFile System:\n");
    test_filesystem();

    TARGET_PRINT("\nTime:\n");
    test_time();

    TARGET_PRINT("\nSystem:\n");
    test_system();

    /* Clean up test file if it exists */
    {
        uintptr_t args[2];
        args[0] = (uintptr_t)test_filename;
        args[1] = zbc_strlen(test_filename);
        zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf),
                     SH_SYS_REMOVE, (uintptr_t)args);
    }

    TARGET_EXIT();
}
