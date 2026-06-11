/*
 * Mock virtio-mmio Device Implementation
 */

#include "mock_virtio.h"

#include <string.h>

/*------------------------------------------------------------------------
 * Register window setup
 *------------------------------------------------------------------------*/

static void reg_set32(mock_virtio_t *dev, uint32_t off, uint32_t val) {
  ZBC_WRITE_U32_LE(dev->window.regs + off, val);
}

void mock_virtio_init(mock_virtio_t *dev, uint32_t device_id,
                      uint16_t queue_num_max) {
  memset(dev, 0, sizeof(*dev));

  reg_set32(dev, ZBC_VIRTIO_REG_MAGIC, (uint32_t)ZBC_VIRTIO_MMIO_MAGIC);
  reg_set32(dev, ZBC_VIRTIO_REG_VERSION, ZBC_VIRTIO_VERSION_MODERN);
  reg_set32(dev, ZBC_VIRTIO_REG_DEVICE_ID, device_id);
  reg_set32(dev, ZBC_VIRTIO_REG_VENDOR_ID, 0x4d4f434bUL); /* "KCOM" */
  /*
   * The passive array returns this word for every FEATURES_SEL value.
   * The driver only requires bit 0 of word 1 (VIRTIO_F_VERSION_1) and
   * ignores everything else, so one value serves all selectors.
   */
  reg_set32(dev, ZBC_VIRTIO_REG_DEV_FEATURES, ZBC_VIRTIO_F_VERSION_1_MASK);
  reg_set32(dev, ZBC_VIRTIO_REG_QUEUE_NUM_MAX, queue_num_max);
}

void mock_virtio_bind_queue(mock_virtio_t *dev, int queue_index,
                            const zbc_virtq_t *q) {
  mock_virtio_queue_t *mq = &dev->queues[queue_index];

  mq->q = q;
  mq->last_avail_idx = 0;
  mq->used_idx = 0;
  mq->bound = 1;
}

void mock_virtio_attach(mock_virtio_t *dev, mock_virtio_hook_t *hook,
                        int queue_index, zbc_virtq_t *q) {
  mock_virtio_bind_queue(dev, queue_index, q);
  hook->dev = dev;
  hook->queue_index = queue_index;
  q->notify_callback = mock_virtio_kick;
  q->notify_ctx = hook;
}

/*------------------------------------------------------------------------
 * Descriptor chain walking
 *------------------------------------------------------------------------*/

/* Decoded view of one chain: at most one out and one in region (the
 * layout the ZBC driver produces; sufficient for all tests). */
typedef struct {
  const uint8_t *out;
  size_t out_len;
  uint8_t *in;
  size_t in_len;
} mock_chain_t;

static void read_desc(const zbc_virtq_t *q, uint16_t index, uintptr_t *addr,
                      uint32_t *len, uint16_t *flags, uint16_t *next) {
  const uint8_t *d = q->desc + (size_t)ZBC_VIRTQ_DESC_ENTRY_SIZE * index;
  uintptr_t lo = (uintptr_t)ZBC_READ_U32_LE(d);
  uintptr_t hi = (uintptr_t)ZBC_READ_U32_LE(d + 4);

  /* Double shift: defined even when uintptr_t is 32 bits. */
  *addr = lo | ((hi << 16) << 16);
  *len = ZBC_READ_U32_LE(d + 8);
  *flags = ZBC_READ_U16_LE(d + 12);
  *next = ZBC_READ_U16_LE(d + 14);
}

static void walk_chain(const zbc_virtq_t *q, uint16_t head,
                       mock_chain_t *chain) {
  uint16_t index = head;
  int hops;

  memset(chain, 0, sizeof(*chain));

  for (hops = 0; hops < 8; hops++) {
    uintptr_t addr;
    uint32_t len;
    uint16_t flags, next;

    read_desc(q, index, &addr, &len, &flags, &next);

    if (flags & ZBC_VIRTQ_DESC_F_WRITE) {
      chain->in = (uint8_t *)addr;
      chain->in_len = len;
    } else {
      chain->out = (const uint8_t *)addr;
      chain->out_len = len;
    }

    if (!(flags & ZBC_VIRTQ_DESC_F_NEXT)) {
      break;
    }
    index = next;
  }
}

/*------------------------------------------------------------------------
 * Queue servicing
 *------------------------------------------------------------------------*/

static void push_used(mock_virtio_queue_t *mq, uint16_t head, uint32_t len) {
  const zbc_virtq_t *q = mq->q;
  uint8_t *entry = q->used + ZBC_VIRTQ_USED_HDR_SIZE +
                   (size_t)ZBC_VIRTQ_USED_ENTRY_SIZE * (mq->used_idx % q->size);

  ZBC_WRITE_U32_LE(entry, head);
  ZBC_WRITE_U32_LE(entry + 4, len);
  mq->used_idx++;
  ZBC_WRITE_U16_LE(q->used + 2, mq->used_idx);
}

void mock_virtio_kick(void *ctx) {
  mock_virtio_hook_t *hook = (mock_virtio_hook_t *)ctx;
  mock_virtio_t *dev = hook->dev;
  mock_virtio_queue_t *mq = &dev->queues[hook->queue_index];
  const zbc_virtq_t *q = mq->q;
  uint16_t avail_idx;

  dev->kick_count++;

  if (!mq->bound || !dev->service) {
    return;
  }

  avail_idx = ZBC_READ_U16_LE(q->avail + 2);
  while (mq->last_avail_idx != avail_idx) {
    uint16_t head = ZBC_READ_U16_LE(q->avail + ZBC_VIRTQ_AVAIL_HDR_SIZE +
                                    (size_t)2 * (mq->last_avail_idx % q->size));
    mock_chain_t chain;
    int written;

    walk_chain(q, head, &chain);
    written = dev->service(dev->service_ctx, hook->queue_index, chain.out,
                           chain.out_len, chain.in, chain.in_len);
    if (written < 0) {
      return; /* pending: leave the chain available, stop consuming */
    }

    mq->last_avail_idx++;
    push_used(mq, head, (uint32_t)written);
  }
}
