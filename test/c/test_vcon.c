/*
 * virtio-console Transport Tests
 *
 * Drives the vcon transport through zbc_call()/zbc_api_* against the
 * mock virtio device with a console personality: transmitted bytes are
 * captured to a log, received bytes come from a preloaded script.
 */

#include "mock_virtio.h"
#include "test_harness.h"
#include "zbc_api.h"
#include "zbc_vcon.h"
#include <string.h>

/*------------------------------------------------------------------------
 * Console personality for the mock device
 *------------------------------------------------------------------------*/

typedef struct {
  uint8_t tx_log[256];
  size_t tx_len;
  const char *rx_script;
  size_t rx_pos;
} console_ctx_t;

static int console_service(void *vctx, int queue_index, const uint8_t *out,
                           size_t out_len, uint8_t *in, size_t in_len) {
  console_ctx_t *ctx = (console_ctx_t *)vctx;
  size_t i;

  if (queue_index == ZBC_VCON_QUEUE_TX) {
    for (i = 0; i < out_len && ctx->tx_len < sizeof(ctx->tx_log); i++) {
      ctx->tx_log[ctx->tx_len++] = out[i];
    }
    return 0;
  }

  /* Receive queue: deliver whatever the script still has (may be less
   * than the posted buffer, like a real tty). */
  {
    size_t avail = strlen(ctx->rx_script) - ctx->rx_pos;
    size_t n = avail < in_len ? avail : in_len;

    for (i = 0; i < n; i++) {
      in[i] = (uint8_t)ctx->rx_script[ctx->rx_pos++];
    }
    return (int)n;
  }
}

/*------------------------------------------------------------------------
 * Fixture: client wired to a mock console through the vcon transport
 *------------------------------------------------------------------------*/

typedef struct {
  mock_virtio_t dev;
  mock_virtio_hook_t rx_hook;
  mock_virtio_hook_t tx_hook;
  console_ctx_t console;
  zbc_vcon_state_t vcon;
  zbc_client_state_t client;
  uint8_t arena[ZBC_VCON_ARENA_SIZE];
} vcon_fixture_t;

static int setup_vcon(vcon_fixture_t *fx, const char *rx_script) {
  int rc;

  memset(&fx->console, 0, sizeof(fx->console));
  fx->console.rx_script = rx_script;

  mock_virtio_init(&fx->dev, ZBC_VIRTIO_ID_CONSOLE, 8);
  fx->dev.service = console_service;
  fx->dev.service_ctx = &fx->console;

  rc = zbc_vcon_init(&fx->vcon, fx->dev.window.regs, fx->arena,
                     sizeof(fx->arena));
  if (rc != ZBC_OK) {
    return rc;
  }

  mock_virtio_attach(&fx->dev, &fx->rx_hook, ZBC_VCON_QUEUE_RX, &fx->vcon.rx);
  mock_virtio_attach(&fx->dev, &fx->tx_hook, ZBC_VCON_QUEUE_TX, &fx->vcon.tx);

  zbc_client_init(&fx->client, (volatile void *)0);
  fx->client.transport = zbc_transport_vcon();
  fx->client.transport_ctx = &fx->vcon;

  return ZBC_OK;
}

/* Fixtures are large (ring arenas); keep them off the stack. */
static vcon_fixture_t g_fx;

/*------------------------------------------------------------------------
 * Test: initialization succeeds on a console, fails on anything else
 *------------------------------------------------------------------------*/

static void test_vcon_init(void) {
  TEST_ASSERT_EQ(setup_vcon(&g_fx, ""), ZBC_OK);

  /* Wrong device ID is refused */
  {
    mock_virtio_t not_console;
    zbc_vcon_state_t vcon;

    mock_virtio_init(&not_console, ZBC_VIRTIO_ID_9P, 8);
    TEST_ASSERT_EQ(zbc_vcon_init(&vcon, not_console.window.regs, g_fx.arena,
                                 sizeof(g_fx.arena)),
                   ZBC_ERR_DEVICE_ERROR);
  }

  /* Arena too small is refused */
  {
    mock_virtio_t console;
    zbc_vcon_state_t vcon;

    mock_virtio_init(&console, ZBC_VIRTIO_ID_CONSOLE, 8);
    TEST_ASSERT_EQ(zbc_vcon_init(&vcon, console.window.regs, g_fx.arena, 64),
                   ZBC_ERR_BUFFER_FULL);
  }
}

/*------------------------------------------------------------------------
 * Test: WRITE0 and WRITEC reach the transmit queue
 *------------------------------------------------------------------------*/

static void test_vcon_write0_writec(void) {
  GUARDED_BUF(buf, 64);
  uintptr_t args[1];
  zbc_response_t response;
  char c = '!';

  GUARDED_INIT(buf);
  TEST_ASSERT_EQ(setup_vcon(&g_fx, ""), ZBC_OK);

  args[0] = (uintptr_t) "Hello, vcon\n";
  TEST_ASSERT_EQ(
      zbc_call(&response, &g_fx.client, buf, buf_size, SH_SYS_WRITE0, args),
      ZBC_OK);
  TEST_ASSERT_EQ(response.result, 0);

  args[0] = (uintptr_t)&c;
  TEST_ASSERT_EQ(
      zbc_call(&response, &g_fx.client, buf, buf_size, SH_SYS_WRITEC, args),
      ZBC_OK);

  TEST_ASSERT_EQ((int)g_fx.console.tx_len, 13);
  TEST_ASSERT_MEM_EQ(g_fx.console.tx_log, "Hello, vcon\n!", 13);
  TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Test: WRITE on fds 1/2 transmits; other fds fail with EBADF
 *------------------------------------------------------------------------*/

static void test_vcon_write_fds(void) {
  GUARDED_BUF(buf, 64);
  uintptr_t args[3];
  zbc_response_t response;

  GUARDED_INIT(buf);
  TEST_ASSERT_EQ(setup_vcon(&g_fx, ""), ZBC_OK);

  args[0] = 1;
  args[1] = (uintptr_t) "out";
  args[2] = 3;
  TEST_ASSERT_EQ(
      zbc_call(&response, &g_fx.client, buf, buf_size, SH_SYS_WRITE, args),
      ZBC_OK);
  TEST_ASSERT_EQ(response.result, 0); /* bytes NOT written */

  args[0] = 2;
  args[1] = (uintptr_t) "err";
  TEST_ASSERT_EQ(
      zbc_call(&response, &g_fx.client, buf, buf_size, SH_SYS_WRITE, args),
      ZBC_OK);
  TEST_ASSERT_EQ(response.result, 0);

  TEST_ASSERT_MEM_EQ(g_fx.console.tx_log, "outerr", 6);

  /* File fds are not the console's business */
  args[0] = 5;
  TEST_ASSERT_EQ(
      zbc_call(&response, &g_fx.client, buf, buf_size, SH_SYS_WRITE, args),
      ZBC_OK);
  TEST_ASSERT_EQ(response.result, -1);
  TEST_ASSERT_EQ(response.error_code, ZBC_ERRNO_EBADF);
  TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Test: READC and READ on fd 0 consume the receive script
 *------------------------------------------------------------------------*/

static void test_vcon_read(void) {
  GUARDED_BUF(buf, 64);
  uint8_t dest[16];
  uintptr_t args[3];
  zbc_response_t response;

  GUARDED_INIT(buf);
  TEST_ASSERT_EQ(setup_vcon(&g_fx, "abc"), ZBC_OK);

  /* READC takes the first character */
  TEST_ASSERT_EQ(zbc_call(&response, &g_fx.client, buf, buf_size, SH_SYS_READC,
                          (uintptr_t *)0),
                 ZBC_OK);
  TEST_ASSERT_EQ(response.result, 'a');

  /* READ(fd 0) drains the rest; result is bytes NOT read */
  memset(dest, 0, sizeof(dest));
  args[0] = 0;
  args[1] = (uintptr_t)dest;
  args[2] = 8;
  TEST_ASSERT_EQ(
      zbc_call(&response, &g_fx.client, buf, buf_size, SH_SYS_READ, args),
      ZBC_OK);
  TEST_ASSERT_EQ(response.result, 6); /* asked 8, got "bc" */
  TEST_ASSERT_MEM_EQ(dest, "bc", 2);

  /* READ on a file fd fails with EBADF */
  args[0] = 3;
  TEST_ASSERT_EQ(
      zbc_call(&response, &g_fx.client, buf, buf_size, SH_SYS_READ, args),
      ZBC_OK);
  TEST_ASSERT_EQ(response.result, -1);
  TEST_ASSERT_EQ(response.error_code, ZBC_ERRNO_EBADF);
  TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Test: ISTTY, ISERROR, and sticky SYS_ERRNO semantics
 *------------------------------------------------------------------------*/

static void test_vcon_queries(void) {
  GUARDED_BUF(buf, 64);
  uintptr_t args[3];
  zbc_response_t response;

  GUARDED_INIT(buf);
  TEST_ASSERT_EQ(setup_vcon(&g_fx, ""), ZBC_OK);

  args[0] = 0;
  TEST_ASSERT_EQ(
      zbc_call(&response, &g_fx.client, buf, buf_size, SH_SYS_ISTTY, args),
      ZBC_OK);
  TEST_ASSERT_EQ(response.result, 1);
  args[0] = 3;
  TEST_ASSERT_EQ(
      zbc_call(&response, &g_fx.client, buf, buf_size, SH_SYS_ISTTY, args),
      ZBC_OK);
  TEST_ASSERT_EQ(response.result, 0);

  args[0] = (uintptr_t)-1;
  TEST_ASSERT_EQ(
      zbc_call(&response, &g_fx.client, buf, buf_size, SH_SYS_ISERROR, args),
      ZBC_OK);
  TEST_ASSERT_EQ(response.result, 1);
  args[0] = 0;
  TEST_ASSERT_EQ(
      zbc_call(&response, &g_fx.client, buf, buf_size, SH_SYS_ISERROR, args),
      ZBC_OK);
  TEST_ASSERT_EQ(response.result, 0);

  /* errno starts clean, goes sticky after a failure */
  TEST_ASSERT_EQ(zbc_call(&response, &g_fx.client, buf, buf_size, SH_SYS_ERRNO,
                          (uintptr_t *)0),
                 ZBC_OK);
  TEST_ASSERT_EQ(response.result, 0);

  args[0] = 7; /* bad fd */
  args[1] = (uintptr_t) "x";
  args[2] = 1;
  TEST_ASSERT_EQ(
      zbc_call(&response, &g_fx.client, buf, buf_size, SH_SYS_WRITE, args),
      ZBC_OK);
  TEST_ASSERT_EQ(zbc_call(&response, &g_fx.client, buf, buf_size, SH_SYS_ERRNO,
                          (uintptr_t *)0),
                 ZBC_OK);
  TEST_ASSERT_EQ(response.result, ZBC_ERRNO_EBADF);
  TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Test: unimplemented opcodes fail with ENOSYS, never hang
 *------------------------------------------------------------------------*/

static void test_vcon_enosys(void) {
  GUARDED_BUF(buf, 64);
  uintptr_t args[3];
  zbc_response_t response;

  GUARDED_INIT(buf);
  TEST_ASSERT_EQ(setup_vcon(&g_fx, ""), ZBC_OK);

  args[0] = (uintptr_t) "/tmp/file";
  args[1] = SH_OPEN_R;
  args[2] = 9;
  TEST_ASSERT_EQ(
      zbc_call(&response, &g_fx.client, buf, buf_size, SH_SYS_OPEN, args),
      ZBC_OK);
  TEST_ASSERT_EQ(response.result, -1);
  TEST_ASSERT_EQ(response.error_code, ZBC_ERRNO_ENOSYS);

  TEST_ASSERT_EQ(zbc_call(&response, &g_fx.client, buf, buf_size, SH_SYS_TIME,
                          (uintptr_t *)0),
                 ZBC_OK);
  TEST_ASSERT_EQ(response.result, -1);
  TEST_ASSERT_EQ(response.error_code, ZBC_ERRNO_ENOSYS);
  TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Test: the high-level API works unchanged over the vcon transport
 *------------------------------------------------------------------------*/

static void test_vcon_through_api(void) {
  GUARDED_BUF(buf, 64);
  zbc_api_t api;

  GUARDED_INIT(buf);
  TEST_ASSERT_EQ(setup_vcon(&g_fx, ""), ZBC_OK);
  zbc_api_init(&api, &g_fx.client, buf, buf_size);

  zbc_api_write0(&api, "via api");
  TEST_ASSERT_MEM_EQ(g_fx.console.tx_log, "via api", 7);

  TEST_ASSERT_EQ(zbc_api_write(&api, 1, "!", 1), 0);
  TEST_ASSERT_MEM_EQ(g_fx.console.tx_log, "via api!", 8);

  /* File API fails cleanly through the console transport */
  TEST_ASSERT_EQ(zbc_api_open(&api, "/nope", SH_OPEN_R), -1);
  TEST_ASSERT_EQ(zbc_api_errno(&api), ZBC_ERRNO_ENOSYS);
  TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Suite runner
 *------------------------------------------------------------------------*/

void run_vcon_tests(void) {
  BEGIN_SUITE("virtio-console Transport");

  RUN_TEST(vcon_init);
  RUN_TEST(vcon_write0_writec);
  RUN_TEST(vcon_write_fds);
  RUN_TEST(vcon_read);
  RUN_TEST(vcon_queries);
  RUN_TEST(vcon_enosys);
  RUN_TEST(vcon_through_api);

  END_SUITE();
}
