/*
 * ZBC Semihosting MAME Integration Test
 *
 * This program tests the semihost device in MAME by:
 * 1. Detecting the device via signature check
 * 2. Printing messages via SYS_WRITE0
 * 3. File operations: open, write, read, seek, flen, close, remove
 * 4. Exiting with status code via SYS_EXIT
 *
 * Compile for any ZBC platform - the device address is calculated
 * using ZBC's memory layout formula.
 */

/* ZBC_CLIENT is defined via Makefile -DZBC_CLIENT */
#include "zbc_semihost.h"

/*
 * ZBC Memory Layout Formula:
 *   addr_bits = sizeof(void*) * 8
 *   reserved_start = (1 << addr_bits) - (1 << (addr_bits/2))
 *   vram_addr = reserved_start - 512
 *   semihost_addr = vram_addr - 32
 *
 * For 32-bit: reserved = 0xFFFF0000, semihost = 0xFFFEFDE0
 * For 64-bit: reserved = 0xFFFFFFFF00000000, semihost = 0xFFFFFFFEFFFFFDE0
 */
static inline volatile uint8_t *get_semihost_base(void)
{
    const unsigned int half_bits = sizeof(void*) * 4;
    const uintptr_t reserved_start = (uintptr_t)0 - ((uintptr_t)1 << half_bits);
    return (volatile uint8_t *)(reserved_start - 512 - 32);
}

#define SEMIHOST_BASE get_semihost_base()

static zbc_client_state_t client;
static uint8_t riff_buf[512];

/* Forward declarations */
static void print(const char *msg);
static void print_hex(uintptr_t val);
static void do_exit(int code);
static uintptr_t sh_open(const char *path, int mode);
static uintptr_t sh_close(uintptr_t fd);
static uintptr_t sh_write(uintptr_t fd, const void *buf, size_t len);
static uintptr_t sh_read(uintptr_t fd, void *buf, size_t len);
static uintptr_t sh_seek(uintptr_t fd, uintptr_t pos);
static uintptr_t sh_flen(uintptr_t fd);
static uintptr_t sh_remove(const char *path);

/* Test data */
static const char test_filename[] = "test_file.txt";
static const char test_data[] = "Hello, semihost file I/O!";
static char read_buf[64];

/* Simple string comparison */
static int str_equal(const char *a, const char *b, size_t len)
{
    size_t i;
    for (i = 0; i < len; i++) {
        if (a[i] != b[i]) return 0;
    }
    return 1;
}

__attribute__((section(".text.startup"))) void _start(void)
{
    uintptr_t fd;
    uintptr_t result;
    size_t test_len;
    size_t i;

    /* Initialize client */
    zbc_client_init(&client, SEMIHOST_BASE);

    /* Test 1: Check device signature */
    if (!zbc_client_check_signature(&client)) {
        print("FAIL: signature check\n");
        do_exit(1);
    }

    /* Test 2: Check device present bit */
    if (!zbc_client_device_present(&client)) {
        print("FAIL: device not present\n");
        do_exit(2);
    }

    print("Device detected.\n");

    /* Test 3: Open file for writing */
    print("Opening file for write... ");
    fd = sh_open(test_filename, SH_OPEN_W);
    print("fd=");
    print_hex(fd);
    print(" ");
    if (fd == (uintptr_t)-1) {
        print("FAIL\n");
        do_exit(3);
    }
    print("OK\n");

    /* Test 4: Write data to file */
    test_len = zbc_strlen(test_data);
    print("Writing data... ");
    result = sh_write(fd, test_data, test_len);
    if (result != 0) {
        print("FAIL (not all bytes written)\n");
        do_exit(4);
    }
    print("OK\n");

    /* Test 5: Get file length */
    print("Checking file length... ");
    result = sh_flen(fd);
    if (result != test_len) {
        print("FAIL (expected ");
        print_hex(test_len);
        print(", got ");
        print_hex(result);
        print(")\n");
        do_exit(5);
    }
    print("OK\n");

    /* Test 6: Close file */
    print("Closing file... ");
    result = sh_close(fd);
    if (result != 0) {
        print("FAIL\n");
        do_exit(6);
    }
    print("OK\n");

    /* Test 7: Reopen file for reading */
    print("Reopening for read... ");
    fd = sh_open(test_filename, SH_OPEN_R);
    if (fd == (uintptr_t)-1) {
        print("FAIL\n");
        do_exit(7);
    }
    print("OK\n");

    /* Test 8: Read data back */
    print("Reading data... ");
    /* Clear buffer first */
    for (i = 0; i < sizeof(read_buf); i++) read_buf[i] = 0;
    result = sh_read(fd, read_buf, test_len);
    if (result != 0) {
        print("FAIL (not all bytes read)\n");
        do_exit(8);
    }
    print("OK\n");

    /* Test 9: Verify data matches */
    print("Verifying data... ");
    if (!str_equal(read_buf, test_data, test_len)) {
        print("FAIL (data mismatch)\n");
        print("Expected: ");
        print(test_data);
        print("\nGot: ");
        print(read_buf);
        print("\n");
        do_exit(9);
    }
    print("OK\n");

    /* Test 10: Seek to beginning */
    print("Seeking to start... ");
    result = sh_seek(fd, 0);
    if (result != 0) {
        print("FAIL\n");
        do_exit(10);
    }
    print("OK\n");

    /* Test 11: Read first 5 bytes */
    print("Reading partial... ");
    for (i = 0; i < sizeof(read_buf); i++) read_buf[i] = 0;
    result = sh_read(fd, read_buf, 5);
    if (result != 0) {
        print("FAIL\n");
        do_exit(11);
    }
    if (!str_equal(read_buf, "Hello", 5)) {
        print("FAIL (got: ");
        print(read_buf);
        print(")\n");
        do_exit(11);
    }
    print("OK\n");

    /* Test 12: Close file */
    print("Closing file... ");
    result = sh_close(fd);
    if (result != 0) {
        print("FAIL\n");
        do_exit(12);
    }
    print("OK\n");

    /* Test 13: Remove file */
    print("Removing file... ");
    result = sh_remove(test_filename);
    if (result != 0) {
        print("FAIL\n");
        do_exit(13);
    }
    print("OK\n");

    /* Test 14: Verify file is gone (open should fail) */
    print("Verifying removal... ");
    fd = sh_open(test_filename, SH_OPEN_R);
    if (fd != (uintptr_t)-1) {
        print("FAIL (file still exists)\n");
        sh_close(fd);
        do_exit(14);
    }
    print("OK\n");

    print("\nAll file I/O tests passed!\n");

    /* Success - exit with code 0 */
    do_exit(0);
}

static void print(const char *msg)
{
    uintptr_t args[1];
    args[0] = (uintptr_t)msg;
    zbc_semihost(&client, riff_buf, sizeof(riff_buf),
                 SH_SYS_WRITE0, (uintptr_t)args);
}

static void print_hex(uintptr_t val)
{
    static const char hex[] = "0123456789abcdef";
    char buf[20];
    int i = 0;
    int started = 0;
    int shift;

    buf[i++] = '0';
    buf[i++] = 'x';

    for (shift = (sizeof(uintptr_t) * 8) - 4; shift >= 0; shift -= 4) {
        int digit = (val >> shift) & 0xf;
        if (digit != 0 || started || shift == 0) {
            buf[i++] = hex[digit];
            started = 1;
        }
    }
    buf[i] = '\0';
    print(buf);
}

static void do_exit(int code)
{
    uintptr_t args[2];
    args[0] = (uintptr_t)code;
    args[1] = 0;  /* subcode */
    zbc_semihost(&client, riff_buf, sizeof(riff_buf),
                 SH_SYS_EXIT, (uintptr_t)args);

    /* Should never reach here - MAME exits */
    for (;;) {}
}

/*
 * SH_SYS_OPEN: args[0] = path, args[1] = mode, args[2] = path_len
 * Returns file descriptor or -1 on error
 */
static uintptr_t sh_open(const char *path, int mode)
{
    uintptr_t args[3];
    uintptr_t result;

    print("  path=");
    print(path);
    print(" mode=");
    print_hex(mode);
    print(" len=");
    print_hex(zbc_strlen(path));
    print("\n");

    args[0] = (uintptr_t)path;
    args[1] = (uintptr_t)mode;
    args[2] = (uintptr_t)zbc_strlen(path);

    print("  args: ");
    print_hex(args[0]);
    print(" ");
    print_hex(args[1]);
    print(" ");
    print_hex(args[2]);
    print("\n");

    print("  riff_buf=");
    print_hex((uintptr_t)riff_buf);
    print(" size=");
    print_hex(sizeof(riff_buf));
    print(" cnfg_sent=");
    print_hex(client.cnfg_sent);
    print("\n");

    result = zbc_semihost(&client, riff_buf, sizeof(riff_buf),
                          SH_SYS_OPEN, (uintptr_t)args);

    print("  cnfg_sent after=");
    print_hex(client.cnfg_sent);
    print("\n");

    print("  result=");
    print_hex(result);
    print("\n");

    /* Dump first 48 bytes of response buffer */
    print("  resp: ");
    {
        int i;
        for (i = 0; i < 48; i++) {
            print_hex(riff_buf[i]);
            print(" ");
            if (i == 15 || i == 31) print("\n        ");
        }
        print("\n");
    }

    return result;
}

/*
 * SH_SYS_CLOSE: args[0] = fd
 * Returns 0 on success, -1 on error
 */
static uintptr_t sh_close(uintptr_t fd)
{
    uintptr_t args[1];
    args[0] = fd;
    return zbc_semihost(&client, riff_buf, sizeof(riff_buf),
                        SH_SYS_CLOSE, (uintptr_t)args);
}

/*
 * SH_SYS_WRITE: args[0] = fd, args[1] = buf, args[2] = len
 * Returns number of bytes NOT written (0 = success)
 */
static uintptr_t sh_write(uintptr_t fd, const void *buf, size_t len)
{
    uintptr_t args[3];
    args[0] = fd;
    args[1] = (uintptr_t)buf;
    args[2] = (uintptr_t)len;
    return zbc_semihost(&client, riff_buf, sizeof(riff_buf),
                        SH_SYS_WRITE, (uintptr_t)args);
}

/*
 * SH_SYS_READ: args[0] = fd, args[1] = buf, args[2] = len
 * Returns number of bytes NOT read (0 = success, len = EOF)
 */
static uintptr_t sh_read(uintptr_t fd, void *buf, size_t len)
{
    uintptr_t args[3];
    args[0] = fd;
    args[1] = (uintptr_t)buf;
    args[2] = (uintptr_t)len;
    return zbc_semihost(&client, riff_buf, sizeof(riff_buf),
                        SH_SYS_READ, (uintptr_t)args);
}

/*
 * SH_SYS_SEEK: args[0] = fd, args[1] = pos
 * Returns 0 on success, negative on error
 */
static uintptr_t sh_seek(uintptr_t fd, uintptr_t pos)
{
    uintptr_t args[2];
    args[0] = fd;
    args[1] = pos;
    return zbc_semihost(&client, riff_buf, sizeof(riff_buf),
                        SH_SYS_SEEK, (uintptr_t)args);
}

/*
 * SH_SYS_FLEN: args[0] = fd
 * Returns file length or -1 on error
 */
static uintptr_t sh_flen(uintptr_t fd)
{
    uintptr_t args[1];
    args[0] = fd;
    return zbc_semihost(&client, riff_buf, sizeof(riff_buf),
                        SH_SYS_FLEN, (uintptr_t)args);
}

/*
 * SH_SYS_REMOVE: args[0] = path, args[1] = path_len
 * Returns 0 on success, -1 on error
 */
static uintptr_t sh_remove(const char *path)
{
    uintptr_t args[2];
    args[0] = (uintptr_t)path;
    args[1] = (uintptr_t)zbc_strlen(path);
    return zbc_semihost(&client, riff_buf, sizeof(riff_buf),
                        SH_SYS_REMOVE, (uintptr_t)args);
}
