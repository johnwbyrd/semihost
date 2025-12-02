/*
 * Round-Trip Integration Tests
 *
 * Tests client and host libraries together, verifying that:
 * 1. Client builds valid RIFF request
 * 2. Host parses and dispatches correctly via backend
 * 3. Host builds valid RIFF response
 * 4. Client parses response correctly
 *
 * Uses the dummy backend which returns success for all operations.
 */

#include "mock_device.h"
#include "mock_memory.h"
#include "test_harness.h"
#include "zbc_semi_client.h"
#include "zbc_semi_common.h"
#include "zbc_semi_host.h"
#include <string.h>

/*------------------------------------------------------------------------
 * Helper: Set up client pointing to mock device
 *------------------------------------------------------------------------*/

static void setup_roundtrip(zbc_client_state_t *client, mock_device_t *dev)
{
    mock_device_init(dev);
    zbc_client_init(client, dev->regs);
}

/*------------------------------------------------------------------------
 * Helper: Simulate full request/response cycle
 *
 * This mimics what happens when client submits a request:
 * 1. Client writes RIFF_PTR
 * 2. Client writes DOORBELL
 * 3. Host processes and writes response
 * 4. Client reads STATUS (RESPONSE_READY)
 *------------------------------------------------------------------------*/

static int do_roundtrip(zbc_client_state_t *client, mock_device_t *dev,
                        uint8_t *buf, size_t riff_size)
{
    size_t i;
    uintptr_t addr = (uintptr_t)buf;

    /* Write buffer address to RIFF_PTR register */
    for (i = 0; i < sizeof(uintptr_t) && i < 16; i++) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        dev->regs[ZBC_REG_RIFF_PTR + sizeof(uintptr_t) - 1 - i] =
            (uint8_t)(addr & 0xFF);
#else
        dev->regs[ZBC_REG_RIFF_PTR + i] = (uint8_t)(addr & 0xFF);
#endif
        addr >>= 8;
    }

    /* Clear remaining bytes */
    for (; i < 16; i++) {
        dev->regs[ZBC_REG_RIFF_PTR + i] = 0;
    }

    /* Trigger doorbell (this processes the request) */
    mock_device_doorbell(dev);

    /* Check response ready */
    (void)client;
    (void)riff_size;

    return (dev->regs[ZBC_REG_STATUS] & ZBC_STATUS_RESPONSE_READY)
               ? ZBC_OK
               : ZBC_ERR_TIMEOUT;
}

/*------------------------------------------------------------------------
 * Test: SYS_CLOSE - simple syscall with PARM
 *
 * Dummy backend returns 0 (success) for close.
 *------------------------------------------------------------------------*/

static void test_roundtrip_close(void)
{
    GUARDED_BUF(buf, 256);
    zbc_client_state_t client;
    mock_device_t dev;
    zbc_builder_t builder;
    zbc_response_t response;
    size_t riff_size;
    int rc;

    GUARDED_INIT(buf);
    setup_roundtrip(&client, &dev);

    /* Build request */
    rc = zbc_builder_start(&builder, buf, buf_size, &client);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    rc = zbc_builder_begin_call(&builder, SH_SYS_CLOSE);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    rc = zbc_builder_add_parm_int(&builder, 5); /* fd = 5 */
    TEST_ASSERT_EQ(rc, ZBC_OK);

    rc = zbc_builder_finish(&builder, &riff_size);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    /* Execute round-trip */
    rc = do_roundtrip(&client, &dev, buf, riff_size);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    /* Parse response */
    rc = zbc_parse_response(&response, buf, buf_size, &client);
    TEST_ASSERT_EQ(rc, ZBC_OK);
    TEST_ASSERT_EQ(response.is_error, 0);
    TEST_ASSERT_EQ(response.result, 0); /* Dummy backend returns 0 */

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Test: SYS_ERRNO - syscall that returns errno
 *
 * Dummy backend returns 0 for get_errno.
 *------------------------------------------------------------------------*/

static void test_roundtrip_errno(void)
{
    GUARDED_BUF(buf, 256);
    zbc_client_state_t client;
    mock_device_t dev;
    zbc_builder_t builder;
    zbc_response_t response;
    size_t riff_size;
    int rc;

    GUARDED_INIT(buf);
    setup_roundtrip(&client, &dev);

    /* Build request */
    rc = zbc_builder_start(&builder, buf, buf_size, &client);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    rc = zbc_builder_begin_call(&builder, SH_SYS_ERRNO);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    rc = zbc_builder_finish(&builder, &riff_size);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    /* Execute round-trip */
    rc = do_roundtrip(&client, &dev, buf, riff_size);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    /* Parse response */
    rc = zbc_parse_response(&response, buf, buf_size, &client);
    TEST_ASSERT_EQ(rc, ZBC_OK);
    TEST_ASSERT_EQ(response.is_error, 0);
    TEST_ASSERT_EQ(response.result, 0); /* Dummy backend returns 0 */

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Test: SYS_WRITE - syscall with DATA chunk
 *
 * Dummy backend returns 0 (all bytes written).
 *------------------------------------------------------------------------*/

static void test_roundtrip_write(void)
{
    GUARDED_BUF(buf, 512);
    zbc_client_state_t client;
    mock_device_t dev;
    zbc_builder_t builder;
    zbc_response_t response;
    size_t riff_size;
    int rc;
    uint8_t test_data[] = "Hello, World!";

    GUARDED_INIT(buf);
    setup_roundtrip(&client, &dev);

    /* Build request with DATA */
    rc = zbc_builder_start(&builder, buf, buf_size, &client);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    rc = zbc_builder_begin_call(&builder, SH_SYS_WRITE);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    rc = zbc_builder_add_parm_int(&builder, 1); /* fd = 1 (stdout) */
    TEST_ASSERT_EQ(rc, ZBC_OK);

    rc = zbc_builder_add_data_binary(&builder, test_data, sizeof(test_data));
    TEST_ASSERT_EQ(rc, ZBC_OK);

    rc = zbc_builder_add_parm_uint(&builder, sizeof(test_data));
    TEST_ASSERT_EQ(rc, ZBC_OK);

    rc = zbc_builder_finish(&builder, &riff_size);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    /* Execute round-trip */
    rc = do_roundtrip(&client, &dev, buf, riff_size);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    /* Parse response */
    rc = zbc_parse_response(&response, buf, buf_size, &client);
    TEST_ASSERT_EQ(rc, ZBC_OK);
    TEST_ASSERT_EQ(response.is_error, 0);
    TEST_ASSERT_EQ(response.result, 0); /* Dummy: 0 bytes NOT written */

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Test: SYS_HEAPINFO - syscall that returns multiple values
 *
 * Dummy backend returns zeros for all heap/stack info.
 *------------------------------------------------------------------------*/

static void test_roundtrip_heapinfo(void)
{
    GUARDED_BUF(buf, 512);
    zbc_client_state_t client;
    mock_device_t dev;
    zbc_builder_t builder;
    zbc_response_t response;
    size_t riff_size;
    int rc;

    GUARDED_INIT(buf);
    setup_roundtrip(&client, &dev);

    /* Build request */
    rc = zbc_builder_start(&builder, buf, buf_size, &client);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    rc = zbc_builder_begin_call(&builder, SH_SYS_HEAPINFO);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    rc = zbc_builder_finish(&builder, &riff_size);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    /* Execute round-trip */
    rc = do_roundtrip(&client, &dev, buf, riff_size);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    /* Parse response */
    rc = zbc_parse_response(&response, buf, buf_size, &client);
    TEST_ASSERT_EQ(rc, ZBC_OK);
    TEST_ASSERT_EQ(response.is_error, 0);
    TEST_ASSERT_EQ(response.result, 0);

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Test: SYS_OPEN - syscall with string DATA
 *
 * Dummy backend returns fd = 3.
 *------------------------------------------------------------------------*/

static void test_roundtrip_open(void)
{
    GUARDED_BUF(buf, 512);
    zbc_client_state_t client;
    mock_device_t dev;
    zbc_builder_t builder;
    zbc_response_t response;
    size_t riff_size;
    int rc;
    const char *filename = "test.txt";

    GUARDED_INIT(buf);
    setup_roundtrip(&client, &dev);

    /* Build OPEN request */
    rc = zbc_builder_start(&builder, buf, buf_size, &client);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    rc = zbc_builder_begin_call(&builder, SH_SYS_OPEN);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    rc = zbc_builder_add_data_string(&builder, filename);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    rc = zbc_builder_add_parm_int(&builder, SH_OPEN_R);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    rc = zbc_builder_add_parm_uint(&builder, strlen(filename));
    TEST_ASSERT_EQ(rc, ZBC_OK);

    rc = zbc_builder_finish(&builder, &riff_size);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    /* Execute round-trip */
    rc = do_roundtrip(&client, &dev, buf, riff_size);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    /* Parse response */
    rc = zbc_parse_response(&response, buf, buf_size, &client);
    TEST_ASSERT_EQ(rc, ZBC_OK);
    TEST_ASSERT_EQ(response.is_error, 0);
    TEST_ASSERT_EQ(response.result, 3); /* Dummy backend returns fd = 3 */

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Test: SYS_TIME - syscall with no params
 *
 * Dummy backend returns 0.
 *------------------------------------------------------------------------*/

static void test_roundtrip_time(void)
{
    GUARDED_BUF(buf, 256);
    zbc_client_state_t client;
    mock_device_t dev;
    zbc_builder_t builder;
    zbc_response_t response;
    size_t riff_size;
    int rc;

    GUARDED_INIT(buf);
    setup_roundtrip(&client, &dev);

    /* Build request */
    rc = zbc_builder_start(&builder, buf, buf_size, &client);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    rc = zbc_builder_begin_call(&builder, SH_SYS_TIME);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    rc = zbc_builder_finish(&builder, &riff_size);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    /* Execute round-trip */
    rc = do_roundtrip(&client, &dev, buf, riff_size);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    /* Parse response */
    rc = zbc_parse_response(&response, buf, buf_size, &client);
    TEST_ASSERT_EQ(rc, ZBC_OK);
    TEST_ASSERT_EQ(response.is_error, 0);
    TEST_ASSERT_EQ(response.result, 0); /* Dummy backend returns 0 */

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
    RUN_TEST(roundtrip_heapinfo);
    RUN_TEST(roundtrip_open);
    RUN_TEST(roundtrip_time);

    END_SUITE();
}
