/*
 * virtio Core Tests
 *
 * Exercises the polled virtio-mmio driver against the mock device:
 * probe, window scan, init handshake (including rejection paths),
 * virtqueue setup, descriptor chains in all three shapes, and ring
 * wraparound.
 */

#include "mock_virtio.h"
#include "test_harness.h"
#include <string.h>

/*------------------------------------------------------------------------
 * Echo service: copies the out buffer into the in buffer
 *------------------------------------------------------------------------*/

static int echo_service(void *ctx, int queue_index, const uint8_t *out,
                        size_t out_len, uint8_t *in, size_t in_len) {
  size_t n = out_len < in_len ? out_len : in_len;
  size_t i;

  (void)ctx;
  (void)queue_index;

  for (i = 0; i < n; i++) {
    in[i] = out[i];
  }
  return (int)n;
}

/*------------------------------------------------------------------------
 * Test: probe accepts a modern device and reports its ID
 *------------------------------------------------------------------------*/

static void test_virtio_probe_ok(void) {
  mock_virtio_t dev;
  uint32_t id = 0;

  mock_virtio_init(&dev, ZBC_VIRTIO_ID_9P, 8);

  TEST_ASSERT_EQ(zbc_virtio_probe(dev.window.regs, &id), ZBC_OK);
  TEST_ASSERT_EQ((int)id, ZBC_VIRTIO_ID_9P);
}

/*------------------------------------------------------------------------
 * Test: probe rejects bad magic and legacy version
 *------------------------------------------------------------------------*/

static void test_virtio_probe_rejects(void) {
  mock_virtio_t dev;

  /* Wrong magic */
  mock_virtio_init(&dev, ZBC_VIRTIO_ID_9P, 8);
  ZBC_WRITE_U32_LE(dev.window.regs + ZBC_VIRTIO_REG_MAGIC, 0xDEADBEEFUL);
  TEST_ASSERT_EQ(zbc_virtio_probe(dev.window.regs, (uint32_t *)0),
                 ZBC_ERR_DEVICE_ERROR);

  /* Legacy version 1 */
  mock_virtio_init(&dev, ZBC_VIRTIO_ID_9P, 8);
  ZBC_WRITE_U32_LE(dev.window.regs + ZBC_VIRTIO_REG_VERSION, 1);
  TEST_ASSERT_EQ(zbc_virtio_probe(dev.window.regs, (uint32_t *)0),
                 ZBC_ERR_DEVICE_ERROR);

  /* NULL base */
  TEST_ASSERT_EQ(zbc_virtio_probe((volatile void *)0, (uint32_t *)0),
                 ZBC_ERR_NULL_ARG);
}

/*------------------------------------------------------------------------
 * Test: window scan finds the right slot by device ID
 *------------------------------------------------------------------------*/

static void test_virtio_scan(void) {
  /* Three contiguous slots: empty, console, 9p */
  static mock_virtio_t window[3];
  volatile void *found;

  mock_virtio_init(&window[0], ZBC_VIRTIO_ID_NONE, 8);
  mock_virtio_init(&window[1], ZBC_VIRTIO_ID_CONSOLE, 8);
  mock_virtio_init(&window[2], ZBC_VIRTIO_ID_9P, 8);

  found = zbc_virtio_scan(window[0].window.regs, sizeof(mock_virtio_t), 3,
                          ZBC_VIRTIO_ID_9P);
  TEST_ASSERT(found == (volatile void *)window[2].window.regs);

  found = zbc_virtio_scan(window[0].window.regs, sizeof(mock_virtio_t), 3,
                          ZBC_VIRTIO_ID_CONSOLE);
  TEST_ASSERT(found == (volatile void *)window[1].window.regs);

  /* Device not present in the window */
  found = zbc_virtio_scan(window[0].window.regs, sizeof(mock_virtio_t), 3,
                          0x10 /* some other ID */);
  TEST_ASSERT(found == (volatile void *)0);

  /* Scanning for "none" never matches an empty slot */
  found = zbc_virtio_scan(window[0].window.regs, sizeof(mock_virtio_t), 3,
                          ZBC_VIRTIO_ID_NONE);
  TEST_ASSERT(found == (volatile void *)0);
}

/*------------------------------------------------------------------------
 * Test: init handshake reaches FEATURES_OK and acknowledges VERSION_1
 *------------------------------------------------------------------------*/

static void test_virtio_init_handshake(void) {
  mock_virtio_t dev;
  uint32_t status;

  mock_virtio_init(&dev, ZBC_VIRTIO_ID_CONSOLE, 8);

  TEST_ASSERT_EQ(zbc_virtio_init(dev.window.regs), ZBC_OK);

  status = ZBC_READ_U32_LE(dev.window.regs + ZBC_VIRTIO_REG_STATUS);
  TEST_ASSERT(status & ZBC_VIRTIO_STATUS_ACKNOWLEDGE);
  TEST_ASSERT(status & ZBC_VIRTIO_STATUS_DRIVER);
  TEST_ASSERT(status & ZBC_VIRTIO_STATUS_FEATURES_OK);
  TEST_ASSERT(!(status & ZBC_VIRTIO_STATUS_DRIVER_OK));
  TEST_ASSERT(!(status & ZBC_VIRTIO_STATUS_FAILED));

  /* Driver acknowledged exactly VIRTIO_F_VERSION_1 */
  TEST_ASSERT_EQ(
      (int)ZBC_READ_U32_LE(dev.window.regs + ZBC_VIRTIO_REG_DRV_FEATURES),
      (int)ZBC_VIRTIO_F_VERSION_1_MASK);

  TEST_ASSERT_EQ(zbc_virtio_start(dev.window.regs), ZBC_OK);
  status = ZBC_READ_U32_LE(dev.window.regs + ZBC_VIRTIO_REG_STATUS);
  TEST_ASSERT(status & ZBC_VIRTIO_STATUS_DRIVER_OK);
}

/*------------------------------------------------------------------------
 * Test: init fails (and flags FAILED) without VIRTIO_F_VERSION_1
 *------------------------------------------------------------------------*/

static void test_virtio_init_requires_version1(void) {
  mock_virtio_t dev;
  uint32_t status;

  mock_virtio_init(&dev, ZBC_VIRTIO_ID_CONSOLE, 8);
  ZBC_WRITE_U32_LE(dev.window.regs + ZBC_VIRTIO_REG_DEV_FEATURES, 0);

  TEST_ASSERT_EQ(zbc_virtio_init(dev.window.regs), ZBC_ERR_DEVICE_ERROR);

  status = ZBC_READ_U32_LE(dev.window.regs + ZBC_VIRTIO_REG_STATUS);
  TEST_ASSERT(status & ZBC_VIRTIO_STATUS_FAILED);
}

/*------------------------------------------------------------------------
 * Test: queue setup negotiates size and programs the ring registers
 *------------------------------------------------------------------------*/

static void test_virtq_init(void) {
  static uint8_t arena[ZBC_VIRTQ_ARENA_SIZE];
  mock_virtio_t dev;
  zbc_virtq_t q;

  mock_virtio_init(&dev, ZBC_VIRTIO_ID_CONSOLE, 256);
  TEST_ASSERT_EQ(zbc_virtio_init(dev.window.regs), ZBC_OK);
  TEST_ASSERT_EQ(zbc_virtq_init(&q, dev.window.regs, 0, arena, sizeof(arena)),
                 ZBC_OK);

  /* Size clamped to ZBC_VIRTQ_SIZE even though the device offers 256 */
  TEST_ASSERT_EQ(q.size, ZBC_VIRTQ_SIZE);
  TEST_ASSERT_EQ(
      (int)ZBC_READ_U32_LE(dev.window.regs + ZBC_VIRTIO_REG_QUEUE_NUM),
      ZBC_VIRTQ_SIZE);
  TEST_ASSERT_EQ(
      (int)ZBC_READ_U32_LE(dev.window.regs + ZBC_VIRTIO_REG_QUEUE_READY), 1);

  /* Ring alignment per the split-virtqueue spec */
  TEST_ASSERT_EQ((int)((uintptr_t)q.desc % ZBC_VIRTQ_DESC_ALIGN), 0);
  TEST_ASSERT_EQ((int)((uintptr_t)q.used % ZBC_VIRTQ_USED_ALIGN), 0);

  /* Registers point at the rings */
  TEST_ASSERT_EQ(
      (int)ZBC_READ_U32_LE(dev.window.regs + ZBC_VIRTIO_REG_QUEUE_DESC_LO),
      (int)(uint32_t)(uintptr_t)q.desc);

  /* Arena too small is rejected */
  TEST_ASSERT_EQ(zbc_virtq_init(&q, dev.window.regs, 0, arena, 16),
                 ZBC_ERR_BUFFER_FULL);

  /* Unavailable queue (QueueNumMax == 0) is rejected */
  ZBC_WRITE_U32_LE(dev.window.regs + ZBC_VIRTIO_REG_QUEUE_NUM_MAX, 0);
  TEST_ASSERT_EQ(zbc_virtq_init(&q, dev.window.regs, 1, arena, sizeof(arena)),
                 ZBC_ERR_DEVICE_ERROR);
}

/*------------------------------------------------------------------------
 * Test: out+in chain round-trips through the echo service
 *------------------------------------------------------------------------*/

static void test_virtq_xfer_echo(void) {
  static uint8_t arena[ZBC_VIRTQ_ARENA_SIZE];
  mock_virtio_t dev;
  mock_virtio_hook_t hook;
  zbc_virtq_t q;
  const char *msg = "hello, virtqueue";
  uint8_t reply[32];
  uint32_t used_len = 0;

  mock_virtio_init(&dev, ZBC_VIRTIO_ID_9P, 8);
  dev.service = echo_service;
  TEST_ASSERT_EQ(zbc_virtio_init(dev.window.regs), ZBC_OK);
  TEST_ASSERT_EQ(zbc_virtq_init(&q, dev.window.regs, 0, arena, sizeof(arena)),
                 ZBC_OK);
  mock_virtio_attach(&dev, &hook, 0, &q);
  TEST_ASSERT_EQ(zbc_virtio_start(dev.window.regs), ZBC_OK);

  memset(reply, 0, sizeof(reply));
  TEST_ASSERT_EQ(
      zbc_virtq_xfer(&q, msg, strlen(msg), reply, sizeof(reply), &used_len),
      ZBC_OK);

  TEST_ASSERT_EQ((int)used_len, (int)strlen(msg));
  TEST_ASSERT_MEM_EQ(reply, msg, strlen(msg));
  TEST_ASSERT_EQ(dev.kick_count, 1);
}

/*------------------------------------------------------------------------
 * Test: out-only and in-only chains
 *------------------------------------------------------------------------*/

typedef struct {
  uint8_t captured[64];
  size_t captured_len;
  const char *feed;
} oneway_ctx_t;

static int oneway_service(void *vctx, int queue_index, const uint8_t *out,
                          size_t out_len, uint8_t *in, size_t in_len) {
  oneway_ctx_t *ctx = (oneway_ctx_t *)vctx;
  size_t i;

  (void)queue_index;

  if (out && out_len > 0) { /* capture transmitted bytes */
    for (i = 0; i < out_len && ctx->captured_len < sizeof(ctx->captured); i++) {
      ctx->captured[ctx->captured_len++] = out[i];
    }
    return 0;
  }
  if (in && in_len > 0) { /* feed scripted bytes */
    size_t n = strlen(ctx->feed);
    if (n > in_len) {
      n = in_len;
    }
    for (i = 0; i < n; i++) {
      in[i] = (uint8_t)ctx->feed[i];
    }
    return (int)n;
  }
  return 0;
}

static void test_virtq_xfer_oneway(void) {
  static uint8_t arena[ZBC_VIRTQ_ARENA_SIZE];
  mock_virtio_t dev;
  mock_virtio_hook_t hook;
  zbc_virtq_t q;
  oneway_ctx_t ctx;
  uint8_t inbuf[8];
  uint32_t used_len = 99;

  memset(&ctx, 0, sizeof(ctx));
  ctx.feed = "xyz";

  mock_virtio_init(&dev, ZBC_VIRTIO_ID_CONSOLE, 8);
  dev.service = oneway_service;
  dev.service_ctx = &ctx;
  TEST_ASSERT_EQ(zbc_virtio_init(dev.window.regs), ZBC_OK);
  TEST_ASSERT_EQ(zbc_virtq_init(&q, dev.window.regs, 0, arena, sizeof(arena)),
                 ZBC_OK);
  mock_virtio_attach(&dev, &hook, 0, &q);
  TEST_ASSERT_EQ(zbc_virtio_start(dev.window.regs), ZBC_OK);

  /* Out-only (transmit) */
  TEST_ASSERT_EQ(zbc_virtq_xfer(&q, "ping", 4, (void *)0, 0, &used_len),
                 ZBC_OK);
  TEST_ASSERT_EQ((int)used_len, 0);
  TEST_ASSERT_EQ((int)ctx.captured_len, 4);
  TEST_ASSERT_MEM_EQ(ctx.captured, "ping", 4);

  /* In-only (receive) */
  memset(inbuf, 0, sizeof(inbuf));
  TEST_ASSERT_EQ(
      zbc_virtq_xfer(&q, (const void *)0, 0, inbuf, sizeof(inbuf), &used_len),
      ZBC_OK);
  TEST_ASSERT_EQ((int)used_len, 3);
  TEST_ASSERT_MEM_EQ(inbuf, "xyz", 3);

  /* Empty chain is rejected */
  TEST_ASSERT_EQ(zbc_virtq_post(&q, (const void *)0, 0, (void *)0, 0),
                 ZBC_ERR_INVALID_ARG);
}

/*------------------------------------------------------------------------
 * Test: ring indices wrap correctly across many transfers
 *------------------------------------------------------------------------*/

static void test_virtq_wraparound(void) {
  static uint8_t arena[ZBC_VIRTQ_ARENA_SIZE];
  mock_virtio_t dev;
  mock_virtio_hook_t hook;
  zbc_virtq_t q;
  uint8_t reply[8];
  uint32_t used_len;
  int i;

  mock_virtio_init(&dev, ZBC_VIRTIO_ID_9P, 8);
  dev.service = echo_service;
  TEST_ASSERT_EQ(zbc_virtio_init(dev.window.regs), ZBC_OK);
  TEST_ASSERT_EQ(zbc_virtq_init(&q, dev.window.regs, 0, arena, sizeof(arena)),
                 ZBC_OK);
  mock_virtio_attach(&dev, &hook, 0, &q);
  TEST_ASSERT_EQ(zbc_virtio_start(dev.window.regs), ZBC_OK);

  /* 3x the ring size forces both rings to wrap repeatedly */
  for (i = 0; i < 3 * ZBC_VIRTQ_SIZE; i++) {
    uint8_t msg[4];
    msg[0] = (uint8_t)i;
    msg[1] = (uint8_t)(i + 1);
    msg[2] = (uint8_t)(i + 2);
    msg[3] = (uint8_t)(i + 3);
    memset(reply, 0xAA, sizeof(reply));
    used_len = 0;

    TEST_ASSERT_EQ(zbc_virtq_xfer(&q, msg, 4, reply, sizeof(reply), &used_len),
                   ZBC_OK);
    TEST_ASSERT_EQ((int)used_len, 4);
    TEST_ASSERT_MEM_EQ(reply, msg, 4);
  }

  TEST_ASSERT_EQ(dev.kick_count, 3 * ZBC_VIRTQ_SIZE);
}

/*------------------------------------------------------------------------
 * Test: only one chain may be outstanding; poll reports AGAIN when idle
 *------------------------------------------------------------------------*/

static int pending_service(void *ctx, int queue_index, const uint8_t *out,
                           size_t out_len, uint8_t *in, size_t in_len) {
  (void)ctx;
  (void)queue_index;
  (void)out;
  (void)out_len;
  (void)in;
  (void)in_len;
  return -1; /* leave the chain pending */
}

static void test_virtq_single_outstanding(void) {
  static uint8_t arena[ZBC_VIRTQ_ARENA_SIZE];
  mock_virtio_t dev;
  mock_virtio_hook_t hook;
  zbc_virtq_t q;
  uint8_t inbuf[4];
  uint32_t used_len;

  mock_virtio_init(&dev, ZBC_VIRTIO_ID_CONSOLE, 8);
  dev.service = pending_service;
  TEST_ASSERT_EQ(zbc_virtio_init(dev.window.regs), ZBC_OK);
  TEST_ASSERT_EQ(zbc_virtq_init(&q, dev.window.regs, 0, arena, sizeof(arena)),
                 ZBC_OK);
  mock_virtio_attach(&dev, &hook, 0, &q);
  TEST_ASSERT_EQ(zbc_virtio_start(dev.window.regs), ZBC_OK);

  /* Post a chain the device leaves pending */
  TEST_ASSERT_EQ(zbc_virtq_post(&q, (const void *)0, 0, inbuf, sizeof(inbuf)),
                 ZBC_OK);
  TEST_ASSERT_EQ(zbc_virtq_poll(&q, &used_len), ZBC_ERR_AGAIN);

  /* A second post while one is outstanding is refused */
  TEST_ASSERT_EQ(zbc_virtq_post(&q, "x", 1, (void *)0, 0), ZBC_ERR_INVALID_ARG);
}

/*------------------------------------------------------------------------
 * Suite runner
 *------------------------------------------------------------------------*/

void run_virtio_tests(void) {
  BEGIN_SUITE("virtio Core");

  RUN_TEST(virtio_probe_ok);
  RUN_TEST(virtio_probe_rejects);
  RUN_TEST(virtio_scan);
  RUN_TEST(virtio_init_handshake);
  RUN_TEST(virtio_init_requires_version1);
  RUN_TEST(virtq_init);
  RUN_TEST(virtq_xfer_echo);
  RUN_TEST(virtq_xfer_oneway);
  RUN_TEST(virtq_wraparound);
  RUN_TEST(virtq_single_outstanding);

  END_SUITE();
}
