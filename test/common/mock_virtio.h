/*
 * Mock virtio-mmio Device for Driver Testing
 *
 * A passive register window plus an active queue servicer. The driver
 * under test reads and writes the register array directly; queue
 * processing is triggered through the zbc_virtq_t notify callback (the
 * same hook pattern mock_device uses with doorbell_callback).
 *
 * Because the register array is passive it cannot model per-queue
 * register banking; instead the test binds each driver queue's ring
 * memory to the mock with mock_virtio_bind_queue() after setup.
 */

#ifndef MOCK_VIRTIO_H
#define MOCK_VIRTIO_H

#include "zbc_virtio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOCK_VIRTIO_MAX_QUEUES 4

/*
 * Queue servicer: called once per descriptor chain the driver makes
 * available. 'out' is the device-readable request, 'in' the
 * device-writable response region (either may be NULL). Return the
 * number of bytes written into 'in' (>= 0) to complete the chain, or
 * -1 to leave it pending (no used entry; the driver keeps polling).
 */
typedef int (*mock_virtio_service_fn)(void *ctx, int queue_index,
                                      const uint8_t *out, size_t out_len,
                                      uint8_t *in, size_t in_len);

typedef struct {
  const zbc_virtq_t *q;     /* driver's queue state (ring pointers) */
  uint16_t last_avail_idx;  /* device-side consumption cursor */
  uint16_t used_idx;        /* device-side used index */
  int bound;
} mock_virtio_queue_t;

typedef struct mock_virtio {
  /* Register window; union forces the 32-bit alignment the driver needs */
  union {
    uint8_t regs[ZBC_VIRTIO_MMIO_SLOT_SIZE];
    uint32_t align_force[ZBC_VIRTIO_MMIO_SLOT_SIZE / 4];
  } window;

  mock_virtio_queue_t queues[MOCK_VIRTIO_MAX_QUEUES];

  mock_virtio_service_fn service;
  void *service_ctx;

  int kick_count;
} mock_virtio_t;

/* Hook context for one queue: attach to zbc_virtq_t notify_callback. */
typedef struct {
  mock_virtio_t *dev;
  int queue_index;
} mock_virtio_hook_t;

/*
 * Initialize the mock with a device ID and QueueNumMax. Pre-fills magic,
 * version 2, and a feature word offering VIRTIO_F_VERSION_1.
 */
void mock_virtio_init(mock_virtio_t *dev, uint32_t device_id,
                      uint16_t queue_num_max);

/* Bind a driver queue's ring memory so the mock can service it. */
void mock_virtio_bind_queue(mock_virtio_t *dev, int queue_index,
                            const zbc_virtq_t *q);

/*
 * Notify callback for zbc_virtq_t: processes all new available chains on
 * the hooked queue through the service function. ctx is a
 * mock_virtio_hook_t pointer.
 */
void mock_virtio_kick(void *ctx);

/* Convenience: bind queue and attach the kick hook in one call. */
void mock_virtio_attach(mock_virtio_t *dev, mock_virtio_hook_t *hook,
                        int queue_index, zbc_virtq_t *q);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_VIRTIO_H */
