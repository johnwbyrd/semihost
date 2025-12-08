/*
 * Client API Tests
 *
 * Tests for the zbc_semihost() low-level entry point.
 * The old builder API has been replaced with table-driven zbc_call().
 */

#include "mock_device.h"
#include "test_harness.h"
#include <string.h>

/*------------------------------------------------------------------------
 * Helper: Create a fake client state pointing to mock device
 *------------------------------------------------------------------------*/

static void setup_client_state(zbc_client_state_t *state, mock_device_t *dev) {
    zbc_client_init(state, dev->regs);
}

/*------------------------------------------------------------------------
 * Doorbell callback for testing
 *------------------------------------------------------------------------*/

static void doorbell_hook(void *ctx) {
    mock_device_doorbell((mock_device_t *)ctx);
}

static void setup_roundtrip(zbc_client_state_t *client, mock_device_t *dev)
{
    mock_device_init(dev);
    zbc_client_init(client, dev->regs);
    client->doorbell_callback = doorbell_hook;
    client->doorbell_ctx = dev;
}

/*------------------------------------------------------------------------
 * Basic Client Tests
 *------------------------------------------------------------------------*/

static void test_client_init(void) {
    zbc_client_state_t state;
    mock_device_t dev;

    mock_device_init(&dev);
    setup_client_state(&state, &dev);

    TEST_ASSERT_EQ(state.cnfg_sent, 0);
    TEST_ASSERT_EQ(state.int_size, (int)sizeof(int));
    TEST_ASSERT_EQ(state.ptr_size, (int)sizeof(void *));
}

static void test_client_check_signature(void) {
    zbc_client_state_t state;
    mock_device_t dev;
    int result;

    mock_device_init(&dev);
    setup_client_state(&state, &dev);

    result = zbc_client_check_signature(&state);
    TEST_ASSERT_EQ(result, ZBC_OK);
}

static void test_client_device_present(void) {
    zbc_client_state_t state;
    mock_device_t dev;
    int result;

    mock_device_init(&dev);
    setup_client_state(&state, &dev);

    result = zbc_client_device_present(&state);
    TEST_ASSERT_EQ(result, ZBC_OK);
}

/*------------------------------------------------------------------------
 * zbc_semihost() Tests
 *------------------------------------------------------------------------*/

static void test_semihost_close(void)
{
    GUARDED_BUF(buf, 256);
    zbc_client_state_t client;
    mock_device_t dev;
    uintptr_t args[1];
    uintptr_t result;

    GUARDED_INIT(buf);
    setup_roundtrip(&client, &dev);

    /* Set up ARM-style param block: {fd} */
    args[0] = 5; /* fd = 5 */

    /* Call zbc_semihost() */
    result = zbc_semihost(&client, buf, buf_size, SH_SYS_CLOSE, (uintptr_t)args);

    /* Dummy backend returns 0 for close */
    TEST_ASSERT_EQ((int)result, 0);

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

static void test_semihost_time(void)
{
    GUARDED_BUF(buf, 256);
    zbc_client_state_t client;
    mock_device_t dev;
    uintptr_t result;

    GUARDED_INIT(buf);
    setup_roundtrip(&client, &dev);

    /* SYS_TIME has no parameters */
    result = zbc_semihost(&client, buf, buf_size, SH_SYS_TIME, 0);

    /* Dummy backend returns 0 for time */
    TEST_ASSERT_EQ((int)result, 0);

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

static void test_semihost_errno(void)
{
    GUARDED_BUF(buf, 256);
    zbc_client_state_t client;
    mock_device_t dev;
    uintptr_t result;

    GUARDED_INIT(buf);
    setup_roundtrip(&client, &dev);

    /* SYS_ERRNO has no parameters */
    result = zbc_semihost(&client, buf, buf_size, SH_SYS_ERRNO, 0);

    /* Dummy backend returns 0 for errno */
    TEST_ASSERT_EQ((int)result, 0);

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

static void test_semihost_clock(void)
{
    GUARDED_BUF(buf, 256);
    zbc_client_state_t client;
    mock_device_t dev;
    uintptr_t result;

    GUARDED_INIT(buf);
    setup_roundtrip(&client, &dev);

    result = zbc_semihost(&client, buf, buf_size, SH_SYS_CLOCK, 0);

    /* Dummy backend returns 0 for clock */
    TEST_ASSERT_EQ((int)result, 0);

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

static void test_semihost_tickfreq(void)
{
    GUARDED_BUF(buf, 256);
    zbc_client_state_t client;
    mock_device_t dev;
    uintptr_t result;

    GUARDED_INIT(buf);
    setup_roundtrip(&client, &dev);

    result = zbc_semihost(&client, buf, buf_size, SH_SYS_TICKFREQ, 0);

    /* Dummy backend returns 100 for tickfreq */
    TEST_ASSERT_EQ((int)result, 100);

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Buffer boundary tests
 *------------------------------------------------------------------------*/

static void test_buffer_too_small(void)
{
    GUARDED_BUF(buf, 16); /* Too small for any request */
    zbc_client_state_t client;
    mock_device_t dev;
    uintptr_t result;

    GUARDED_INIT(buf);
    setup_roundtrip(&client, &dev);

    result = zbc_semihost(&client, buf, buf_size, SH_SYS_TIME, 0);

    /* Should return error indicator */
    TEST_ASSERT_EQ((int)result, -1);

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Run all client tests
 *------------------------------------------------------------------------*/

void run_client_builder_tests(void) {
    BEGIN_SUITE("Client API");

    RUN_TEST(client_init);
    RUN_TEST(client_check_signature);
    RUN_TEST(client_device_present);

    END_SUITE();

    BEGIN_SUITE("zbc_semihost() Entry Point");

    RUN_TEST(semihost_close);
    RUN_TEST(semihost_time);
    RUN_TEST(semihost_errno);
    RUN_TEST(semihost_clock);
    RUN_TEST(semihost_tickfreq);
    RUN_TEST(buffer_too_small);

    END_SUITE();
}
