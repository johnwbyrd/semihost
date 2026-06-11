/*
 * Transport Seam Tests
 *
 * Verifies that zbc_call() dispatches through the transport vtable:
 * 1. zbc_client_init() selects the RIFF transport by default
 * 2. A custom transport assigned to the state receives every call
 * 3. The null transport fails every operation with -1 / ENOSYS,
 *    deterministically and without touching the device
 * 4. A state with no transport fails cleanly (never dereferences NULL)
 */

#include "mock_device.h"
#include "test_harness.h"
#include "zbc_api.h"
#include <string.h>

/*------------------------------------------------------------------------
 * Recording transport: captures what zbc_call() dispatches
 *------------------------------------------------------------------------*/

typedef struct {
  int call_count;
  int last_opcode;
  uintptr_t *last_args;
  int canned_result;
  int canned_errno;
} recording_ctx_t;

static int recording_call(zbc_response_t *response, zbc_client_state_t *state,
                          void *buf, size_t buf_size, int opcode,
                          uintptr_t *args) {
  recording_ctx_t *ctx = (recording_ctx_t *)state->transport_ctx;

  (void)buf;
  (void)buf_size;

  ctx->call_count++;
  ctx->last_opcode = opcode;
  ctx->last_args = args;

  response->result = ctx->canned_result;
  response->error_code = ctx->canned_errno;
  response->data = (const uint8_t *)0;
  response->data_size = 0;
  response->is_error = 0;
  response->proto_error = 0;

  return ZBC_OK;
}

static const zbc_transport_t recording_transport = {recording_call};

/*------------------------------------------------------------------------
 * Test: default transport after init is the RIFF transport
 *------------------------------------------------------------------------*/

static void test_transport_default_is_riff(void) {
  zbc_client_state_t client;
  mock_device_t dev;

  mock_device_init(&dev);
  zbc_client_init(&client, dev.regs);

  TEST_ASSERT(client.transport == zbc_transport_riff());
  TEST_ASSERT(client.transport_ctx == (void *)0);
}

/*------------------------------------------------------------------------
 * Test: custom transport receives dispatch (the override mechanism)
 *------------------------------------------------------------------------*/

static void test_transport_custom_dispatch(void) {
  GUARDED_BUF(buf, 256);
  zbc_client_state_t client;
  mock_device_t dev;
  recording_ctx_t ctx;
  zbc_response_t response;
  uintptr_t args[1];
  int rc;

  GUARDED_INIT(buf);
  mock_device_init(&dev);
  zbc_client_init(&client, dev.regs);

  memset(&ctx, 0, sizeof(ctx));
  ctx.canned_result = 42;
  ctx.canned_errno = 0;
  client.transport = &recording_transport;
  client.transport_ctx = &ctx;

  args[0] = 7;
  rc = zbc_call(&response, &client, buf, buf_size, SH_SYS_CLOSE, args);

  TEST_ASSERT_EQ(rc, ZBC_OK);
  TEST_ASSERT_EQ(ctx.call_count, 1);
  TEST_ASSERT_EQ(ctx.last_opcode, SH_SYS_CLOSE);
  TEST_ASSERT(ctx.last_args == args);
  TEST_ASSERT_EQ(response.result, 42);
  /* The device was never touched */
  TEST_ASSERT_EQ(dev.doorbell_count, 0);
  TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Test: zbc_semihost() flows through the custom transport too
 *------------------------------------------------------------------------*/

static void test_transport_semihost_dispatch(void) {
  GUARDED_BUF(buf, 256);
  zbc_client_state_t client;
  mock_device_t dev;
  recording_ctx_t ctx;
  uintptr_t args[1];
  uintptr_t result;

  GUARDED_INIT(buf);
  mock_device_init(&dev);
  zbc_client_init(&client, dev.regs);

  memset(&ctx, 0, sizeof(ctx));
  ctx.canned_result = 5;
  client.transport = &recording_transport;
  client.transport_ctx = &ctx;

  args[0] = 3;
  result = zbc_semihost(&client, buf, buf_size, SH_SYS_CLOSE, (uintptr_t)args);

  TEST_ASSERT_EQ((int)result, 5);
  TEST_ASSERT_EQ(ctx.call_count, 1);
  TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Test: null transport fails every operation with -1 / ENOSYS
 *------------------------------------------------------------------------*/

static void test_transport_null_fails_enosys(void) {
  GUARDED_BUF(buf, 256);
  zbc_client_state_t client;
  mock_device_t dev;
  zbc_response_t response;
  uintptr_t args[3];
  int rc;

  GUARDED_INIT(buf);
  mock_device_init(&dev);
  zbc_client_init(&client, dev.regs);
  client.transport = zbc_transport_null();

  args[0] = (uintptr_t) "/tmp/nope";
  args[1] = SH_OPEN_R;
  args[2] = 9;
  rc = zbc_call(&response, &client, buf, buf_size, SH_SYS_OPEN, args);

  TEST_ASSERT_EQ(rc, ZBC_OK);
  TEST_ASSERT_EQ(response.result, -1);
  TEST_ASSERT_EQ(response.error_code, ZBC_ERRNO_ENOSYS);
  TEST_ASSERT_EQ(response.is_error, 0);
  /* The device was never touched */
  TEST_ASSERT_EQ(dev.doorbell_count, 0);
  TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Test: null transport through the high-level API
 *------------------------------------------------------------------------*/

static void test_transport_null_api(void) {
  GUARDED_BUF(buf, 256);
  zbc_client_state_t client;
  zbc_api_t api;
  mock_device_t dev;
  int fd;

  GUARDED_INIT(buf);
  mock_device_init(&dev);
  zbc_client_init(&client, dev.regs);
  client.transport = zbc_transport_null();
  zbc_api_init(&api, &client, buf, buf_size);

  fd = zbc_api_open(&api, "/tmp/nope", SH_OPEN_R);

  TEST_ASSERT_EQ(fd, -1);
  TEST_ASSERT_EQ(zbc_api_errno(&api), ZBC_ERRNO_ENOSYS);
  TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Test: missing transport fails cleanly (uninitialized state guard)
 *------------------------------------------------------------------------*/

static void test_transport_missing_fails(void) {
  GUARDED_BUF(buf, 256);
  zbc_client_state_t client;
  mock_device_t dev;
  zbc_response_t response;
  int rc;

  GUARDED_INIT(buf);
  mock_device_init(&dev);
  zbc_client_init(&client, dev.regs);
  client.transport = (const zbc_transport_t *)0;

  rc =
      zbc_call(&response, &client, buf, buf_size, SH_SYS_ERRNO, (uintptr_t *)0);

  TEST_ASSERT_EQ(rc, ZBC_ERR_NOT_INITIALIZED);
  TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Test: RIFF transport via explicit assignment behaves like the default
 *
 * Round-trips SYS_CLOSE against the mock device twice -- once with the
 * default transport, once with zbc_transport_riff() assigned explicitly
 * -- and verifies identical results and identical request bytes.
 *------------------------------------------------------------------------*/

static void doorbell_hook(void *ctx) {
  mock_device_doorbell((mock_device_t *)ctx);
}

static void test_transport_riff_explicit_matches_default(void) {
  GUARDED_BUF(buf_a, 256);
  GUARDED_BUF(buf_b, 256);
  zbc_client_state_t client;
  mock_device_t dev;
  uintptr_t args[1];
  uintptr_t result_a, result_b;

  GUARDED_INIT(buf_a);
  GUARDED_INIT(buf_b);
  /* Zero the usable areas so unwritten tail bytes compare equal */
  memset(buf_a, 0, buf_a_size);
  memset(buf_b, 0, buf_b_size);

  /* Default transport */
  mock_device_init(&dev);
  zbc_client_init(&client, dev.regs);
  client.doorbell_callback = doorbell_hook;
  client.doorbell_ctx = &dev;
  args[0] = 5;
  result_a =
      zbc_semihost(&client, buf_a, buf_a_size, SH_SYS_CLOSE, (uintptr_t)args);

  /* Explicitly assigned RIFF transport, fresh state */
  mock_device_init(&dev);
  zbc_client_init(&client, dev.regs);
  client.doorbell_callback = doorbell_hook;
  client.doorbell_ctx = &dev;
  client.transport = zbc_transport_riff();
  args[0] = 5;
  result_b =
      zbc_semihost(&client, buf_b, buf_b_size, SH_SYS_CLOSE, (uintptr_t)args);

  TEST_ASSERT_EQ((int)result_a, 0);
  TEST_ASSERT_EQ((int)result_b, 0);
  /* Identical wire bytes: the response-laden buffers must match exactly */
  TEST_ASSERT_MEM_EQ(buf_a, buf_b, buf_a_size);
  TEST_ASSERT_EQ(GUARDED_CHECK(buf_a), 0);
  TEST_ASSERT_EQ(GUARDED_CHECK(buf_b), 0);
}

/*------------------------------------------------------------------------
 * Suite runner
 *------------------------------------------------------------------------*/

void run_transport_tests(void) {
  BEGIN_SUITE("Transport Seam");

  RUN_TEST(transport_default_is_riff);
  RUN_TEST(transport_custom_dispatch);
  RUN_TEST(transport_semihost_dispatch);
  RUN_TEST(transport_null_fails_enosys);
  RUN_TEST(transport_null_api);
  RUN_TEST(transport_missing_fails);
  RUN_TEST(transport_riff_explicit_matches_default);

  END_SUITE();
}
