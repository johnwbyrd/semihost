/*
 * Round-Trip Integration Tests
 *
 * Tests client and host libraries together, verifying that:
 * 1. Client builds valid RIFF request via zbc_semihost()
 * 2. Host parses and dispatches correctly via backend
 * 3. Host builds valid RIFF response
 * 4. Client parses response correctly
 *
 * Uses the dummy backend which returns success for all operations.
 */

#include "mock_device.h"
#include "mock_memory.h"
#include "test_harness.h"
#include <string.h>

/*------------------------------------------------------------------------
 * Doorbell callback for testing
 *------------------------------------------------------------------------*/

static void doorbell_hook(void *ctx) {
    mock_device_doorbell((mock_device_t *)ctx);
}

/*------------------------------------------------------------------------
 * Helper: Set up client pointing to mock device
 *------------------------------------------------------------------------*/

static void setup_roundtrip(zbc_client_state_t *client, mock_device_t *dev)
{
    mock_device_init(dev);
    zbc_client_init(client, dev->regs);
    client->doorbell_callback = doorbell_hook;
    client->doorbell_ctx = dev;
}

/*------------------------------------------------------------------------
 * Test: SYS_CLOSE - simple syscall with one PARM
 *------------------------------------------------------------------------*/

static void test_roundtrip_close(void)
{
    GUARDED_BUF(buf, 256);
    zbc_client_state_t client;
    mock_device_t dev;
    uintptr_t args[1];
    uintptr_t result;

    GUARDED_INIT(buf);
    setup_roundtrip(&client, &dev);

    args[0] = 5; /* fd = 5 */
    result = zbc_semihost(&client, buf, buf_size, SH_SYS_CLOSE, (uintptr_t)args);

    TEST_ASSERT_EQ((int)result, 0);
    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Test: SYS_ERRNO - syscall with no params
 *------------------------------------------------------------------------*/

static void test_roundtrip_errno(void)
{
    GUARDED_BUF(buf, 256);
    zbc_client_state_t client;
    mock_device_t dev;
    uintptr_t result;

    GUARDED_INIT(buf);
    setup_roundtrip(&client, &dev);

    result = zbc_semihost(&client, buf, buf_size, SH_SYS_ERRNO, 0);

    TEST_ASSERT_EQ((int)result, 0);
    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Test: SYS_WRITE - syscall with DATA chunk
 *------------------------------------------------------------------------*/

static void test_roundtrip_write(void)
{
    GUARDED_BUF(buf, 512);
    zbc_client_state_t client;
    mock_device_t dev;
    const char *data = "Hello, World!";
    uintptr_t args[3];
    uintptr_t result;

    GUARDED_INIT(buf);
    setup_roundtrip(&client, &dev);

    args[0] = 1; /* fd = stdout */
    args[1] = (uintptr_t)data;
    args[2] = strlen(data);

    result = zbc_semihost(&client, buf, buf_size, SH_SYS_WRITE, (uintptr_t)args);

    /* Dummy backend returns 0 (all bytes written) */
    TEST_ASSERT_EQ((int)result, 0);
    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Test: SYS_OPEN - syscall with string DATA
 *------------------------------------------------------------------------*/

static void test_roundtrip_open(void)
{
    GUARDED_BUF(buf, 512);
    zbc_client_state_t client;
    mock_device_t dev;
    const char *filename = "test.txt";
    uintptr_t args[3];
    uintptr_t result;

    GUARDED_INIT(buf);
    setup_roundtrip(&client, &dev);

    args[0] = (uintptr_t)filename;
    args[1] = SH_OPEN_R;
    args[2] = strlen(filename);

    result = zbc_semihost(&client, buf, buf_size, SH_SYS_OPEN, (uintptr_t)args);

    /* Dummy backend returns fd = 3 */
    TEST_ASSERT_EQ((int)result, 3);
    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Test: SYS_TIME - syscall with no params, returns value
 *------------------------------------------------------------------------*/

static void test_roundtrip_time(void)
{
    GUARDED_BUF(buf, 256);
    zbc_client_state_t client;
    mock_device_t dev;
    uintptr_t result;

    GUARDED_INIT(buf);
    setup_roundtrip(&client, &dev);

    result = zbc_semihost(&client, buf, buf_size, SH_SYS_TIME, 0);

    /* Dummy backend returns 0 */
    TEST_ASSERT_EQ((int)result, 0);
    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Test: SYS_CLOCK - syscall returning centiseconds
 *------------------------------------------------------------------------*/

static void test_roundtrip_clock(void)
{
    GUARDED_BUF(buf, 256);
    zbc_client_state_t client;
    mock_device_t dev;
    uintptr_t result;

    GUARDED_INIT(buf);
    setup_roundtrip(&client, &dev);

    result = zbc_semihost(&client, buf, buf_size, SH_SYS_CLOCK, 0);

    /* Dummy backend returns 0 */
    TEST_ASSERT_EQ((int)result, 0);
    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Test: SYS_TICKFREQ - syscall returning tick frequency
 *------------------------------------------------------------------------*/

static void test_roundtrip_tickfreq(void)
{
    GUARDED_BUF(buf, 256);
    zbc_client_state_t client;
    mock_device_t dev;
    uintptr_t result;

    GUARDED_INIT(buf);
    setup_roundtrip(&client, &dev);

    result = zbc_semihost(&client, buf, buf_size, SH_SYS_TICKFREQ, 0);

    /* Dummy backend returns 100 Hz */
    TEST_ASSERT_EQ((int)result, 100);
    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Test: SYS_FLEN - file length query
 *------------------------------------------------------------------------*/

static void test_roundtrip_flen(void)
{
    GUARDED_BUF(buf, 256);
    zbc_client_state_t client;
    mock_device_t dev;
    uintptr_t args[1];
    uintptr_t result;

    GUARDED_INIT(buf);
    setup_roundtrip(&client, &dev);

    args[0] = 3; /* fd */

    result = zbc_semihost(&client, buf, buf_size, SH_SYS_FLEN, (uintptr_t)args);

    /* Dummy backend returns 0 */
    TEST_ASSERT_EQ((int)result, 0);
    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Test: SYS_SEEK - file seek
 *------------------------------------------------------------------------*/

static void test_roundtrip_seek(void)
{
    GUARDED_BUF(buf, 256);
    zbc_client_state_t client;
    mock_device_t dev;
    uintptr_t args[2];
    uintptr_t result;

    GUARDED_INIT(buf);
    setup_roundtrip(&client, &dev);

    args[0] = 3;   /* fd */
    args[1] = 100; /* position */

    result = zbc_semihost(&client, buf, buf_size, SH_SYS_SEEK, (uintptr_t)args);

    /* Dummy backend returns 0 (success) */
    TEST_ASSERT_EQ((int)result, 0);
    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Test: SYS_ISTTY - check if fd is terminal
 *------------------------------------------------------------------------*/

static void test_roundtrip_istty(void)
{
    GUARDED_BUF(buf, 256);
    zbc_client_state_t client;
    mock_device_t dev;
    uintptr_t args[1];
    uintptr_t result;

    GUARDED_INIT(buf);
    setup_roundtrip(&client, &dev);

    args[0] = 1; /* stdout */

    result = zbc_semihost(&client, buf, buf_size, SH_SYS_ISTTY, (uintptr_t)args);

    /* Dummy backend returns 0 (not a TTY) */
    TEST_ASSERT_EQ((int)result, 0);
    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Test: SYS_TIMER_CONFIG - configure periodic timer
 *------------------------------------------------------------------------*/

static void test_roundtrip_timer_config(void)
{
    GUARDED_BUF(buf, 256);
    zbc_client_state_t client;
    mock_device_t dev;
    uintptr_t args[1];
    uintptr_t result;

    GUARDED_INIT(buf);
    setup_roundtrip(&client, &dev);

    args[0] = 1000; /* 1000 Hz */
    result = zbc_semihost(&client, buf, buf_size, SH_SYS_TIMER_CONFIG, (uintptr_t)args);

    /* Dummy backend returns 0 for timer_config */
    TEST_ASSERT_EQ((int)result, 0);
    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Test: malformed RIFF reported via register channel, guest RAM untouched
 *
 * Per spec v0.2.0, when the container is unparseable the host must not
 * write to guest memory at all; it reports through STATUS.PROTO_ERROR and
 * the ERROR_CODE register instead.
 *------------------------------------------------------------------------*/

static void test_roundtrip_malformed_riff(void)
{
    GUARDED_BUF(buf, 256);
    uint8_t snapshot[256];
    zbc_client_state_t client;
    mock_device_t dev;
    size_t i;

    GUARDED_INIT(buf);
    setup_roundtrip(&client, &dev);

    /* Garbage buffer: no RIFF magic */
    for (i = 0; i < buf_size; i++)
        buf[i] = 0xAA;
    memcpy(snapshot, buf, buf_size);

    zbc_client_submit(&client, buf, buf_size);

    /* Error reported through registers */
    TEST_ASSERT_EQ(dev.proto_error_count, 1);
    TEST_ASSERT_EQ(dev.last_proto_error, ZBC_PROTO_ERR_MALFORMED_RIFF);
    TEST_ASSERT_EQ(dev.regs[ZBC_REG_STATUS] & ZBC_STATUS_PROTO_ERROR,
                   ZBC_STATUS_PROTO_ERROR);
    TEST_ASSERT_EQ(dev.regs[ZBC_REG_ERROR_CODE], ZBC_PROTO_ERR_MALFORMED_RIFF);
    TEST_ASSERT_EQ(dev.regs[ZBC_REG_ERROR_CODE + 1], 0);

    /* Guest memory untouched */
    TEST_ASSERT_EQ(memcmp(buf, snapshot, buf_size), 0);
    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Test: platform-provided config defaults make CNFG optional
 *------------------------------------------------------------------------*/

static void test_roundtrip_platform_config(void)
{
    GUARDED_BUF(buf, 256);
    zbc_client_state_t client;
    mock_device_t dev;
    uintptr_t args[1];
    uintptr_t result;

    GUARDED_INIT(buf);
    setup_roundtrip(&client, &dev);

    /* Host knows the guest configuration from the platform */
    zbc_host_set_platform_config(&dev.host_state, client.int_size,
                                 client.ptr_size, client.endianness);

    /* Suppress the client's CNFG chunk to simulate a guest that omits it */
    client.cnfg_sent = 1;

    args[0] = 5; /* fd = 5 */
    result = zbc_semihost(&client, buf, buf_size, SH_SYS_CLOSE, (uintptr_t)args);

    TEST_ASSERT_EQ((int)result, 0);
    TEST_ASSERT_EQ(dev.proto_error_count, 0);
    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Test: missing CNFG without platform defaults reports MISSING_CNFG
 *
 * The client pre-allocates ERRO, so the error arrives in the ERRO chunk
 * (richest channel), not through the registers.
 *------------------------------------------------------------------------*/

static void test_roundtrip_missing_cnfg(void)
{
    GUARDED_BUF(buf, 256);
    zbc_response_t response;
    zbc_client_state_t client;
    mock_device_t dev;
    uintptr_t args[1];
    int rc;

    GUARDED_INIT(buf);
    setup_roundtrip(&client, &dev);

    /* No platform defaults; guest omits CNFG */
    client.cnfg_sent = 1;

    args[0] = 5; /* fd = 5 */
    rc = zbc_call(&response, &client, buf, buf_size, SH_SYS_CLOSE, args);

    TEST_ASSERT_EQ(rc, ZBC_ERR_DEVICE_ERROR);
    TEST_ASSERT_EQ(response.is_error, 1);
    TEST_ASSERT_EQ(response.proto_error, ZBC_PROTO_ERR_MISSING_CNFG);
    /* ERRO chunk was available, so the register channel stays clean */
    TEST_ASSERT_EQ(dev.proto_error_count, 0);
    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Run all roundtrip tests
 *------------------------------------------------------------------------*/

void run_roundtrip_tests(void)
{
    BEGIN_SUITE("Round-Trip Integration");

    RUN_TEST(roundtrip_close);
    RUN_TEST(roundtrip_errno);
    RUN_TEST(roundtrip_write);
    RUN_TEST(roundtrip_open);
    RUN_TEST(roundtrip_time);
    RUN_TEST(roundtrip_clock);
    RUN_TEST(roundtrip_tickfreq);
    RUN_TEST(roundtrip_flen);
    RUN_TEST(roundtrip_seek);
    RUN_TEST(roundtrip_istty);
    RUN_TEST(roundtrip_timer_config);
    RUN_TEST(roundtrip_malformed_riff);
    RUN_TEST(roundtrip_platform_config);
    RUN_TEST(roundtrip_missing_cnfg);

    END_SUITE();
}
