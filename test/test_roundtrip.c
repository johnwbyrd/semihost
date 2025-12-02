/*
 * Round-Trip Integration Tests
 *
 * Tests client and host libraries together, verifying that:
 * 1. Client builds valid RIFF request
 * 2. Host parses and dispatches correctly
 * 3. Host builds valid RIFF response
 * 4. Client parses response correctly
 */

#include "mock_device.h"
#include "mock_memory.h"
#include "test_harness.h"
#include "zbc_semi_client.h"
#include "zbc_semi_host.h"

/*------------------------------------------------------------------------
 * Helper: Set up client pointing to mock device
 *------------------------------------------------------------------------*/

static void setup_roundtrip(zbc_client_state_t *client, mock_device_t *dev) {
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
                        uint8_t *buf, size_t riff_size) {
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
  return (dev->regs[ZBC_REG_STATUS] & ZBC_STATUS_RESPONSE_READY)
             ? ZBC_OK
             : ZBC_ERR_TIMEOUT;

  (void)client;
  (void)riff_size;
}

/*------------------------------------------------------------------------
 * Simple Syscalls
 *------------------------------------------------------------------------*/

static void test_roundtrip_close(void) {
  GUARDED_BUF(buf, 256);
  zbc_client_state_t client;
  mock_device_t dev;
  zbc_builder_t builder;
  zbc_response_t response;
  size_t riff_size;
  int rc;

  GUARDED_INIT(buf);
  setup_roundtrip(&client, &dev);

  /* Set handler that returns 0 on success */
  mock_device_set_handler(&dev, SH_SYS_CLOSE, mock_handler_return_42);

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
  TEST_ASSERT_EQ(response.result, 42);

  TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

static void test_roundtrip_errno(void) {
  GUARDED_BUF(buf, 256);
  zbc_client_state_t client;
  mock_device_t dev;
  zbc_builder_t builder;
  zbc_response_t response;
  size_t riff_size;
  int rc;

  GUARDED_INIT(buf);
  setup_roundtrip(&client, &dev);

  /* Set handler that returns an error */
  mock_device_set_handler(&dev, SH_SYS_ERRNO, mock_handler_error);

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
  TEST_ASSERT_EQ(response.is_error, 0); /* RETN, not ERRO */
  TEST_ASSERT_EQ(response.result, -1);
  TEST_ASSERT_EQ(response.error_code, 5); /* EIO */

  TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Data-returning syscalls
 *------------------------------------------------------------------------*/

static uint8_t g_echo_data[64];
static size_t g_echo_size;

static int echo_handler(zbc_syscall_ctx_t *ctx, zbc_syscall_result_t *result) {
  /* Copy received data to echo buffer and return it */
  result->result = 0;
  result->error = 0;
  result->parm_count = 0;

  if (ctx->data_count > 0 && ctx->data[0].size > 0) {
    size_t i;
    g_echo_size = ctx->data[0].size;
    if (g_echo_size > sizeof(g_echo_data)) {
      g_echo_size = sizeof(g_echo_data);
    }
    for (i = 0; i < g_echo_size; i++) {
      g_echo_data[i] = ctx->data[0].data[i];
    }
    result->data = g_echo_data;
    result->data_size = g_echo_size;
    result->result = (int64_t)g_echo_size;
  } else {
    result->data = NULL;
    result->data_size = 0;
  }

  return 0;
}

static void test_roundtrip_write_read_data(void) {
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

  /* Use echo handler */
  mock_device_set_handler(&dev, SH_SYS_WRITE, echo_handler);

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
  TEST_ASSERT_EQ(response.result, sizeof(test_data));

  /* Verify echoed data */
  TEST_ASSERT(response.data != NULL);
  TEST_ASSERT_EQ(response.data_size, sizeof(test_data));
  TEST_ASSERT_MEM_EQ(response.data, test_data, sizeof(test_data));

  TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * HEAPINFO (multi-PARM return)
 *------------------------------------------------------------------------*/

static void test_roundtrip_heapinfo(void) {
  GUARDED_BUF(buf, 512);
  zbc_client_state_t client;
  mock_device_t dev;
  int rc;

  GUARDED_INIT(buf);
  setup_roundtrip(&client, &dev);

  /* Set heapinfo handler */
  mock_device_set_handler(&dev, SH_SYS_HEAPINFO, mock_handler_heapinfo);

  /* Build request manually */
  {
    zbc_builder_t builder;
    zbc_response_t response;
    size_t riff_size;

    rc = zbc_builder_start(&builder, buf, buf_size, &client);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    rc = zbc_builder_begin_call(&builder, SH_SYS_HEAPINFO);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    rc = zbc_builder_finish(&builder, &riff_size);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    /* Execute round-trip */
    rc = do_roundtrip(&client, &dev, buf, riff_size);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    /* Parse basic response */
    rc = zbc_parse_response(&response, buf, buf_size, &client);
    TEST_ASSERT_EQ(rc, ZBC_OK);
    TEST_ASSERT_EQ(response.is_error, 0);
    TEST_ASSERT_EQ(response.result, 0);

    /* The PARM chunks should be in the buffer - verify manually */
    /* After RIFF header + CNFG + RETN header + result + errno, we should find
     * PARMs */
  }

  TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * String syscalls
 *------------------------------------------------------------------------*/

static char g_received_path[256];

static int open_capture_handler(zbc_syscall_ctx_t *ctx,
                                zbc_syscall_result_t *result) {
  /* Capture the filename from DATA chunk */
  result->result = 3; /* Return fd = 3 */
  result->error = 0;
  result->data = NULL;
  result->data_size = 0;
  result->parm_count = 0;

  if (ctx->data_count > 0 && ctx->data[0].size > 0) {
    size_t len = ctx->data[0].size;
    if (len > sizeof(g_received_path) - 1) {
      len = sizeof(g_received_path) - 1;
    }
    memcpy(g_received_path, ctx->data[0].data, len);
    g_received_path[len] = '\0';
  }

  return 0;
}

static void test_roundtrip_open(void) {
  GUARDED_BUF(buf, 512);
  zbc_client_state_t client;
  mock_device_t dev;
  zbc_builder_t builder;
  zbc_response_t response;
  size_t riff_size;
  int rc;
  const char *filename = "/tmp/test.txt";

  GUARDED_INIT(buf);
  setup_roundtrip(&client, &dev);
  memset(g_received_path, 0, sizeof(g_received_path));

  mock_device_set_handler(&dev, SH_SYS_OPEN, open_capture_handler);

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
  TEST_ASSERT_EQ(response.result, 3); /* fd = 3 */

  /* Verify filename was received */
  TEST_ASSERT_EQ(strcmp(g_received_path, filename), 0);

  TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Run all roundtrip tests
 *------------------------------------------------------------------------*/

void run_roundtrip_tests(void) {
  BEGIN_SUITE("Round-Trip Integration");

  RUN_TEST(roundtrip_close);
  RUN_TEST(roundtrip_errno);
  RUN_TEST(roundtrip_write_read_data);
  RUN_TEST(roundtrip_heapinfo);
  RUN_TEST(roundtrip_open);

  END_SUITE();
}
