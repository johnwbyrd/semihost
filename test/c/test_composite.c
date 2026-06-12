/*
 * Composite Transport Tests
 *
 * Verify opcode-class routing:
 * 1. SYS_WRITEC, SYS_WRITE0, SYS_READC reach the console child
 * 2. SYS_OPEN, SYS_CLOSE, SYS_READ, SYS_WRITE, SYS_SEEK, SYS_FLEN,
 *    SYS_REMOVE, SYS_RENAME, SYS_TMPNAM, SYS_ISTTY reach the file child
 * 3. Opcodes outside both classes reach the fallback child
 * 4. With no fallback, off-class opcodes fail -1 / ENOSYS deterministically
 * 5. Each child sees only its own transport_ctx during dispatch and the
 *    caller's transport_ctx is restored after every call
 */

#include "mock_device.h"
#include "test_harness.h"
#include "zbc_composite.h"
#include <string.h>

/*------------------------------------------------------------------------
 * Tag transport: records every call and stamps a tag onto the response
 *------------------------------------------------------------------------*/

typedef struct {
  int call_count;
  int last_opcode;
  void *seen_ctx;     /* state->transport_ctx as the child observed it */
  int tag;            /* unique per-child marker stamped into response */
} tag_ctx_t;

static int tag_call(zbc_response_t *response, zbc_client_state_t *state,
                    void *buf, size_t buf_size, int opcode,
                    uintptr_t *args) {
  tag_ctx_t *ctx = (tag_ctx_t *)state->transport_ctx;

  (void)buf;
  (void)buf_size;
  (void)args;

  ctx->call_count++;
  ctx->last_opcode = opcode;
  ctx->seen_ctx = state->transport_ctx;

  response->result = ctx->tag;
  response->error_code = 0;
  response->data = (const uint8_t *)0;
  response->data_size = 0;
  response->is_error = 0;
  response->proto_error = 0;

  return ZBC_OK;
}

static const zbc_transport_t tag_transport = {tag_call};

#define TAG_CONSOLE 100
#define TAG_FILE 200
#define TAG_FALLBACK 300

/*------------------------------------------------------------------------
 * Helpers
 *------------------------------------------------------------------------*/

static void setup_composite(zbc_client_state_t *client, mock_device_t *dev,
                            zbc_composite_state_t *cc, tag_ctx_t *console,
                            tag_ctx_t *file, tag_ctx_t *fallback,
                            int wire_fallback) {
  mock_device_init(dev);
  zbc_client_init(client, dev->regs);

  memset(console, 0, sizeof(*console));
  console->tag = TAG_CONSOLE;
  memset(file, 0, sizeof(*file));
  file->tag = TAG_FILE;
  memset(fallback, 0, sizeof(*fallback));
  fallback->tag = TAG_FALLBACK;

  cc->console = &tag_transport;
  cc->console_ctx = console;
  cc->file = &tag_transport;
  cc->file_ctx = file;
  if (wire_fallback) {
    cc->fallback = &tag_transport;
    cc->fallback_ctx = fallback;
  } else {
    cc->fallback = (const zbc_transport_t *)0;
    cc->fallback_ctx = (void *)0;
  }

  client->transport = zbc_transport_composite();
  client->transport_ctx = cc;
}

static int call_one(zbc_client_state_t *client, void *buf, size_t buf_size,
                    int opcode, zbc_response_t *response) {
  uintptr_t args[1];
  args[0] = 0;
  return zbc_call(response, client, buf, buf_size, opcode, args);
}

/*------------------------------------------------------------------------
 * Test: console opcodes reach the console child
 *------------------------------------------------------------------------*/

static void test_composite_console_routing(void) {
  GUARDED_BUF(buf, 256);
  zbc_client_state_t client;
  mock_device_t dev;
  zbc_composite_state_t cc;
  tag_ctx_t console, file, fallback;
  zbc_response_t response;
  int rc;

  GUARDED_INIT(buf);
  setup_composite(&client, &dev, &cc, &console, &file, &fallback, 1);

  rc = call_one(&client, buf, buf_size, SH_SYS_WRITEC, &response);
  TEST_ASSERT_EQ(rc, ZBC_OK);
  TEST_ASSERT_EQ(response.result, TAG_CONSOLE);

  rc = call_one(&client, buf, buf_size, SH_SYS_WRITE0, &response);
  TEST_ASSERT_EQ(rc, ZBC_OK);
  TEST_ASSERT_EQ(response.result, TAG_CONSOLE);

  rc = call_one(&client, buf, buf_size, SH_SYS_READC, &response);
  TEST_ASSERT_EQ(rc, ZBC_OK);
  TEST_ASSERT_EQ(response.result, TAG_CONSOLE);

  TEST_ASSERT_EQ(console.call_count, 3);
  TEST_ASSERT_EQ(file.call_count, 0);
  TEST_ASSERT_EQ(fallback.call_count, 0);
  TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Test: file opcodes reach the file child
 *------------------------------------------------------------------------*/

static void test_composite_file_routing(void) {
  GUARDED_BUF(buf, 256);
  zbc_client_state_t client;
  mock_device_t dev;
  zbc_composite_state_t cc;
  tag_ctx_t console, file, fallback;
  zbc_response_t response;
  static const int file_opcodes[] = {
      SH_SYS_OPEN,   SH_SYS_CLOSE,  SH_SYS_READ,   SH_SYS_WRITE,
      SH_SYS_SEEK,   SH_SYS_FLEN,   SH_SYS_REMOVE, SH_SYS_RENAME,
      SH_SYS_TMPNAM, SH_SYS_ISTTY};
  size_t i;
  int rc;

  GUARDED_INIT(buf);
  setup_composite(&client, &dev, &cc, &console, &file, &fallback, 1);

  for (i = 0; i < sizeof(file_opcodes) / sizeof(file_opcodes[0]); i++) {
    rc = call_one(&client, buf, buf_size, file_opcodes[i], &response);
    TEST_ASSERT_EQ(rc, ZBC_OK);
    TEST_ASSERT_EQ(response.result, TAG_FILE);
  }

  TEST_ASSERT_EQ(console.call_count, 0);
  TEST_ASSERT_EQ(file.call_count,
                 (int)(sizeof(file_opcodes) / sizeof(file_opcodes[0])));
  TEST_ASSERT_EQ(fallback.call_count, 0);
  TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Test: off-class opcodes reach the fallback child
 *------------------------------------------------------------------------*/

static void test_composite_fallback_routing(void) {
  GUARDED_BUF(buf, 256);
  zbc_client_state_t client;
  mock_device_t dev;
  zbc_composite_state_t cc;
  tag_ctx_t console, file, fallback;
  zbc_response_t response;
  int rc;

  GUARDED_INIT(buf);
  setup_composite(&client, &dev, &cc, &console, &file, &fallback, 1);

  rc = call_one(&client, buf, buf_size, SH_SYS_CLOCK, &response);
  TEST_ASSERT_EQ(rc, ZBC_OK);
  TEST_ASSERT_EQ(response.result, TAG_FALLBACK);

  rc = call_one(&client, buf, buf_size, SH_SYS_TIME, &response);
  TEST_ASSERT_EQ(rc, ZBC_OK);
  TEST_ASSERT_EQ(response.result, TAG_FALLBACK);

  rc = call_one(&client, buf, buf_size, SH_SYS_EXIT, &response);
  TEST_ASSERT_EQ(rc, ZBC_OK);
  TEST_ASSERT_EQ(response.result, TAG_FALLBACK);

  TEST_ASSERT_EQ(console.call_count, 0);
  TEST_ASSERT_EQ(file.call_count, 0);
  TEST_ASSERT_EQ(fallback.call_count, 3);
  TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Test: with no fallback wired, off-class opcodes fail -1 / ENOSYS
 *------------------------------------------------------------------------*/

static void test_composite_no_fallback_enosys(void) {
  GUARDED_BUF(buf, 256);
  zbc_client_state_t client;
  mock_device_t dev;
  zbc_composite_state_t cc;
  tag_ctx_t console, file, fallback;
  zbc_response_t response;
  int rc;

  GUARDED_INIT(buf);
  setup_composite(&client, &dev, &cc, &console, &file, &fallback, 0);

  rc = call_one(&client, buf, buf_size, SH_SYS_CLOCK, &response);
  TEST_ASSERT_EQ(rc, ZBC_OK);
  TEST_ASSERT_EQ(response.result, -1);
  TEST_ASSERT_EQ(response.error_code, ZBC_ERRNO_ENOSYS);
  TEST_ASSERT_EQ(response.is_error, 0);

  TEST_ASSERT_EQ(console.call_count, 0);
  TEST_ASSERT_EQ(file.call_count, 0);
  /* The unwired fallback ctx was never visited */
  TEST_ASSERT_EQ(fallback.call_count, 0);
  TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Test: each child sees its own transport_ctx; caller's ctx is restored
 *------------------------------------------------------------------------*/

static void test_composite_ctx_swap_and_restore(void) {
  GUARDED_BUF(buf, 256);
  zbc_client_state_t client;
  mock_device_t dev;
  zbc_composite_state_t cc;
  tag_ctx_t console, file, fallback;
  zbc_response_t response;
  int rc;

  GUARDED_INIT(buf);
  setup_composite(&client, &dev, &cc, &console, &file, &fallback, 1);

  rc = call_one(&client, buf, buf_size, SH_SYS_WRITEC, &response);
  TEST_ASSERT_EQ(rc, ZBC_OK);
  TEST_ASSERT(console.seen_ctx == &console);
  TEST_ASSERT(client.transport_ctx == &cc);

  rc = call_one(&client, buf, buf_size, SH_SYS_OPEN, &response);
  TEST_ASSERT_EQ(rc, ZBC_OK);
  TEST_ASSERT(file.seen_ctx == &file);
  TEST_ASSERT(client.transport_ctx == &cc);

  rc = call_one(&client, buf, buf_size, SH_SYS_TIME, &response);
  TEST_ASSERT_EQ(rc, ZBC_OK);
  TEST_ASSERT(fallback.seen_ctx == &fallback);
  TEST_ASSERT(client.transport_ctx == &cc);

  TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Suite runner
 *------------------------------------------------------------------------*/

void run_composite_tests(void) {
  BEGIN_SUITE("Composite Transport");

  RUN_TEST(composite_console_routing);
  RUN_TEST(composite_file_routing);
  RUN_TEST(composite_fallback_routing);
  RUN_TEST(composite_no_fallback_enosys);
  RUN_TEST(composite_ctx_swap_and_restore);

  END_SUITE();
}
