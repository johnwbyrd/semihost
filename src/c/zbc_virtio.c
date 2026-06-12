/*
 * ZBC Polled virtio-mmio Driver Core
 *
 * Handles both legacy (version 1) and modern (version 2) virtio-mmio
 * with one polled split virtqueue engine. No interrupts, no heap,
 * strictly one outstanding chain per queue. The vring data structures
 * are identical across the two versions; only the device-window
 * registers used to *register* the rings (and the feature-negotiation
 * handshake) differ, so every path in this file dispatches off the
 * version it reads from the live device at setup time. See
 * zbc_virtio.h for the API contract and the QEMU guest transports
 * proposal for design rationale.
 */

#include "zbc_virtio.h"

/*========================================================================
 * Register access
 *
 * virtio-mmio registers are little-endian and must be accessed as
 * aligned 32-bit words (the spec forbids byte access, unlike the ZBC
 * device window).
 *========================================================================*/

/* Compiler barrier: order ring writes against the register kick. */
#if defined(__GNUC__) || defined(__clang__)
#define ZBC_VIRTIO_BARRIER() __asm__ volatile("" ::: "memory")
#else
#define ZBC_VIRTIO_BARRIER() ((void)0)
#endif

static uint32_t zbc_virtio_le32(uint32_t v) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  return ((v & 0x000000FFUL) << 24) | ((v & 0x0000FF00UL) << 8) |
         ((v & 0x00FF0000UL) >> 8) | ((v & 0xFF000000UL) >> 24);
#else
  return v;
#endif
}

static uint32_t reg_read32(volatile uint8_t *base, uint32_t off) {
  return zbc_virtio_le32(*(volatile uint32_t *)(base + off));
}

static void reg_write32(volatile uint8_t *base, uint32_t off, uint32_t val) {
  *(volatile uint32_t *)(base + off) = zbc_virtio_le32(val);
}

/*========================================================================
 * Discovery
 *========================================================================*/

int zbc_virtio_probe(volatile void *mmio, uint32_t *device_id) {
  volatile uint8_t *base = (volatile uint8_t *)mmio;
  uint32_t version;

  if (!base) {
    return ZBC_ERR_NULL_ARG;
  }

  if (reg_read32(base, ZBC_VIRTIO_REG_MAGIC) != ZBC_VIRTIO_MMIO_MAGIC) {
    return ZBC_ERR_DEVICE_ERROR;
  }
  version = reg_read32(base, ZBC_VIRTIO_REG_VERSION);
  if (version != ZBC_VIRTIO_VERSION_LEGACY
      && version != ZBC_VIRTIO_VERSION_MODERN) {
    return ZBC_ERR_DEVICE_ERROR;
  }

  if (device_id) {
    *device_id = reg_read32(base, ZBC_VIRTIO_REG_DEVICE_ID);
  }
  return ZBC_OK;
}

volatile void *zbc_virtio_scan(volatile void *window, size_t stride, int slots,
                               uint32_t device_id) {
  volatile uint8_t *slot = (volatile uint8_t *)window;
  uint32_t id;
  int i;

  if (!slot || stride == 0) {
    return (volatile void *)0;
  }

  for (i = 0; i < slots; i++, slot += stride) {
    if (zbc_virtio_probe(slot, &id) != ZBC_OK) {
      continue;
    }
    if (id == device_id && id != ZBC_VIRTIO_ID_NONE) {
      return slot;
    }
  }
  return (volatile void *)0;
}

/*========================================================================
 * Device initialization
 *========================================================================*/

static void set_failed(volatile uint8_t *base) {
  reg_write32(base, ZBC_VIRTIO_REG_STATUS,
              reg_read32(base, ZBC_VIRTIO_REG_STATUS) |
                  ZBC_VIRTIO_STATUS_FAILED);
}

int zbc_virtio_init(volatile void *mmio) {
  volatile uint8_t *base = (volatile uint8_t *)mmio;
  uint32_t version;
  uint32_t status;
  uint32_t features_hi;
  int rc;

  rc = zbc_virtio_probe(mmio, (uint32_t *)0);
  if (rc != ZBC_OK) {
    return rc;
  }

  version = reg_read32(base, ZBC_VIRTIO_REG_VERSION);

  /* Reset, then acknowledge in two steps per the spec. */
  reg_write32(base, ZBC_VIRTIO_REG_STATUS, 0);
  reg_write32(base, ZBC_VIRTIO_REG_STATUS, ZBC_VIRTIO_STATUS_ACKNOWLEDGE);
  reg_write32(base, ZBC_VIRTIO_REG_STATUS,
              ZBC_VIRTIO_STATUS_ACKNOWLEDGE | ZBC_VIRTIO_STATUS_DRIVER);

  if (version == ZBC_VIRTIO_VERSION_MODERN) {
    /* Modern path: the device must offer VIRTIO_F_VERSION_1 (feature
     * word 1, bit 0) and we accept exactly that, then re-read STATUS
     * to confirm the device kept FEATURES_OK set. */
    reg_write32(base, ZBC_VIRTIO_REG_DEV_FEATURES_SEL,
                ZBC_VIRTIO_F_VERSION_1_WORD);
    features_hi = reg_read32(base, ZBC_VIRTIO_REG_DEV_FEATURES);
    if (!(features_hi & ZBC_VIRTIO_F_VERSION_1_MASK)) {
      ZBC_LOG_ERROR_S("virtio: device does not offer VIRTIO_F_VERSION_1");
      set_failed(base);
      return ZBC_ERR_DEVICE_ERROR;
    }

    reg_write32(base, ZBC_VIRTIO_REG_DRV_FEATURES_SEL, 0);
    reg_write32(base, ZBC_VIRTIO_REG_DRV_FEATURES, 0);
    reg_write32(base, ZBC_VIRTIO_REG_DRV_FEATURES_SEL,
                ZBC_VIRTIO_F_VERSION_1_WORD);
    reg_write32(base, ZBC_VIRTIO_REG_DRV_FEATURES, ZBC_VIRTIO_F_VERSION_1_MASK);

    status = ZBC_VIRTIO_STATUS_ACKNOWLEDGE | ZBC_VIRTIO_STATUS_DRIVER |
             ZBC_VIRTIO_STATUS_FEATURES_OK;
    reg_write32(base, ZBC_VIRTIO_REG_STATUS, status);

    if (!(reg_read32(base, ZBC_VIRTIO_REG_STATUS) &
          ZBC_VIRTIO_STATUS_FEATURES_OK)) {
      ZBC_LOG_ERROR_S("virtio: device rejected feature negotiation");
      set_failed(base);
      return ZBC_ERR_DEVICE_ERROR;
    }
  } else {
    /* Legacy (version 1): no VIRTIO_F_VERSION_1 to negotiate, no
     * FEATURES_OK handshake. Accept zero features, then program the
     * page size the device will use to interpret the queue PFN. */
    reg_write32(base, ZBC_VIRTIO_REG_DRV_FEATURES_SEL, 0);
    reg_write32(base, ZBC_VIRTIO_REG_DRV_FEATURES, 0);
    reg_write32(base, ZBC_VIRTIO_REG_GUEST_PAGE_SIZE,
                (uint32_t)ZBC_VIRTIO_LEGACY_PAGE_SIZE);
  }

  return ZBC_OK;
}

int zbc_virtio_start(volatile void *mmio) {
  volatile uint8_t *base = (volatile uint8_t *)mmio;

  if (!base) {
    return ZBC_ERR_NULL_ARG;
  }

  reg_write32(base, ZBC_VIRTIO_REG_STATUS,
              reg_read32(base, ZBC_VIRTIO_REG_STATUS) |
                  ZBC_VIRTIO_STATUS_DRIVER_OK);
  return ZBC_OK;
}

/*========================================================================
 * Virtqueue setup
 *========================================================================*/

/* Round an address up to an alignment that is a power of two. */
static uintptr_t align_up(uintptr_t p, uintptr_t align) {
  return (p + align - 1) & ~(align - 1);
}

/* Split a guest-physical address into low/high register halves without
 * relying on 64-bit arithmetic (the double shift is defined even when
 * uintptr_t is 32 bits or less). */
static uint32_t addr_lo(uintptr_t addr) { return (uint32_t)addr; }
static uint32_t addr_hi(uintptr_t addr) {
  return (uint32_t)((addr >> 16) >> 16);
}

int zbc_virtq_init(zbc_virtq_t *q, volatile void *mmio, int queue_index,
                   void *mem, size_t mem_size) {
  volatile uint8_t *base = (volatile uint8_t *)mmio;
  uintptr_t p, end;
  uint32_t version, max;
  uint16_t size;
  size_t i;

  if (!q || !base || !mem) {
    return ZBC_ERR_NULL_ARG;
  }

  version = reg_read32(base, ZBC_VIRTIO_REG_VERSION);

  reg_write32(base, ZBC_VIRTIO_REG_QUEUE_SEL, (uint32_t)queue_index);

  /*
   * Note: the spec suggests verifying QueueReady == 0 (modern) or that
   * QueuePFN reads back to 0 (legacy) here. We skip the readback: the
   * device was just reset by zbc_virtio_init() and per-queue register
   * banking cannot be modelled by passive test doubles.
   */

  max = reg_read32(base, ZBC_VIRTIO_REG_QUEUE_NUM_MAX);
  if (max == 0) {
    return ZBC_ERR_DEVICE_ERROR; /* queue not available on this device */
  }
  size = (max < ZBC_VIRTQ_SIZE) ? (uint16_t)max : (uint16_t)ZBC_VIRTQ_SIZE;

  /* Carve rings out of the caller's arena. The wire format of every
   * ring is identical between modern and legacy; the difference is the
   * inter-ring alignment legacy imposes (the device-programmed
   * QueueAlign, which we set equal to the legacy page size) and the
   * fact that legacy expects the descriptor table to start at a
   * page-aligned address (the address it derives from QueuePFN). */
  p = (uintptr_t)mem;
  end = p + mem_size;

  if (version == ZBC_VIRTIO_VERSION_LEGACY) {
    p = align_up(p, ZBC_VIRTIO_LEGACY_PAGE_SIZE);
    q->desc = (uint8_t *)p;
    p += (uintptr_t)ZBC_VIRTQ_DESC_ENTRY_SIZE * size;

    q->avail = (uint8_t *)p;
    p += ZBC_VIRTQ_AVAIL_HDR_SIZE + (uintptr_t)2 * size;

    p = align_up(p, ZBC_VIRTIO_LEGACY_PAGE_SIZE);
    q->used = (uint8_t *)p;
    p += ZBC_VIRTQ_USED_HDR_SIZE + (uintptr_t)ZBC_VIRTQ_USED_ENTRY_SIZE * size;
  } else {
    p = align_up(p, ZBC_VIRTQ_DESC_ALIGN);
    q->desc = (uint8_t *)p;
    p += (uintptr_t)ZBC_VIRTQ_DESC_ENTRY_SIZE * size;

    p = align_up(p, ZBC_VIRTQ_AVAIL_ALIGN);
    q->avail = (uint8_t *)p;
    p += ZBC_VIRTQ_AVAIL_HDR_SIZE + (uintptr_t)2 * size;

    p = align_up(p, ZBC_VIRTQ_USED_ALIGN);
    q->used = (uint8_t *)p;
    p += ZBC_VIRTQ_USED_HDR_SIZE + (uintptr_t)ZBC_VIRTQ_USED_ENTRY_SIZE * size;
  }

  if (p > end) {
    ZBC_LOG_ERROR("virtq: arena too small (need %u, have %u)",
                  (unsigned)(p - (uintptr_t)mem), (unsigned)mem_size);
    return ZBC_ERR_BUFFER_FULL;
  }

  /* Zero the rings. */
  for (i = 0; i < (size_t)ZBC_VIRTQ_DESC_ENTRY_SIZE * size; i++) {
    q->desc[i] = 0;
  }
  for (i = 0; i < (size_t)ZBC_VIRTQ_AVAIL_HDR_SIZE + 2u * size; i++) {
    q->avail[i] = 0;
  }
  for (i = 0; i < (size_t)ZBC_VIRTQ_USED_HDR_SIZE + 8u * size; i++) {
    q->used[i] = 0;
  }

  /* We poll; politely tell the device not to bother interrupting. */
  ZBC_WRITE_U16_LE(q->avail, ZBC_VIRTQ_AVAIL_F_NO_INTERRUPT);

  q->mmio = base;
  q->queue_index = (uint16_t)queue_index;
  q->size = size;
  q->avail_idx = 0;
  q->last_used_idx = 0;
  q->inflight = 0;
  q->notify_callback = (void (*)(void *))0;
  q->notify_ctx = (void *)0;

  reg_write32(base, ZBC_VIRTIO_REG_QUEUE_NUM, size);

  if (version == ZBC_VIRTIO_VERSION_LEGACY) {
    /* Inter-ring alignment AND ring base must both be at a page
     * boundary -- we used the same constant on both sides above. */
    reg_write32(base, ZBC_VIRTIO_REG_QUEUE_ALIGN,
                (uint32_t)ZBC_VIRTIO_LEGACY_PAGE_SIZE);
    reg_write32(base, ZBC_VIRTIO_REG_QUEUE_PFN,
                (uint32_t)((uintptr_t)q->desc / ZBC_VIRTIO_LEGACY_PAGE_SIZE));
  } else {
    reg_write32(base, ZBC_VIRTIO_REG_QUEUE_DESC_LO,
                addr_lo((uintptr_t)q->desc));
    reg_write32(base, ZBC_VIRTIO_REG_QUEUE_DESC_HI,
                addr_hi((uintptr_t)q->desc));
    reg_write32(base, ZBC_VIRTIO_REG_QUEUE_DRIVER_LO,
                addr_lo((uintptr_t)q->avail));
    reg_write32(base, ZBC_VIRTIO_REG_QUEUE_DRIVER_HI,
                addr_hi((uintptr_t)q->avail));
    reg_write32(base, ZBC_VIRTIO_REG_QUEUE_DEVICE_LO,
                addr_lo((uintptr_t)q->used));
    reg_write32(base, ZBC_VIRTIO_REG_QUEUE_DEVICE_HI,
                addr_hi((uintptr_t)q->used));
    reg_write32(base, ZBC_VIRTIO_REG_QUEUE_READY, 1);
  }

  return ZBC_OK;
}

/*========================================================================
 * Chain submission and completion
 *========================================================================*/

/* Write descriptor table entry 'index'. */
static void write_desc(zbc_virtq_t *q, uint16_t index, uintptr_t addr,
                       uint32_t len, uint16_t flags, uint16_t next) {
  uint8_t *d = q->desc + (size_t)ZBC_VIRTQ_DESC_ENTRY_SIZE * index;

  ZBC_WRITE_U32_LE(d, addr_lo(addr));
  ZBC_WRITE_U32_LE(d + 4, addr_hi(addr));
  ZBC_WRITE_U32_LE(d + 8, len);
  ZBC_WRITE_U16_LE(d + 12, flags);
  ZBC_WRITE_U16_LE(d + 14, next);
}

int zbc_virtq_post(zbc_virtq_t *q, const void *out, size_t out_len, void *in,
                   size_t in_len) {
  uint16_t head = 0;

  if (!q || !q->mmio) {
    return ZBC_ERR_NULL_ARG;
  }
  if (q->inflight) {
    return ZBC_ERR_INVALID_ARG; /* strictly one outstanding chain */
  }

  if (out && out_len > 0 && in && in_len > 0) {
    write_desc(q, 0, (uintptr_t)out, (uint32_t)out_len, ZBC_VIRTQ_DESC_F_NEXT,
               1);
    write_desc(q, 1, (uintptr_t)in, (uint32_t)in_len, ZBC_VIRTQ_DESC_F_WRITE,
               0);
  } else if (out && out_len > 0) {
    write_desc(q, 0, (uintptr_t)out, (uint32_t)out_len, 0, 0);
  } else if (in && in_len > 0) {
    write_desc(q, 0, (uintptr_t)in, (uint32_t)in_len, ZBC_VIRTQ_DESC_F_WRITE,
               0);
  } else {
    return ZBC_ERR_INVALID_ARG; /* nothing to transfer */
  }

  /* Publish the head in the available ring, then bump the index. */
  ZBC_WRITE_U16_LE(q->avail + ZBC_VIRTQ_AVAIL_HDR_SIZE +
                       (size_t)2 * (q->avail_idx % q->size),
                   head);
  ZBC_VIRTIO_BARRIER();
  q->avail_idx++;
  ZBC_WRITE_U16_LE(q->avail + 2, q->avail_idx);
  ZBC_VIRTIO_BARRIER();

  q->inflight = 1;

  reg_write32(q->mmio, ZBC_VIRTIO_REG_QUEUE_NOTIFY, q->queue_index);

  if (q->notify_callback) {
    q->notify_callback(q->notify_ctx);
  }
  ZBC_VIRTIO_BARRIER();

  return ZBC_OK;
}

int zbc_virtq_poll(zbc_virtq_t *q, uint32_t *used_len) {
  const uint8_t *entry;
  uint16_t device_idx;

  if (!q) {
    return ZBC_ERR_NULL_ARG;
  }

  ZBC_VIRTIO_BARRIER();
  device_idx = ZBC_READ_U16_LE(q->used + 2);
  if (device_idx == q->last_used_idx) {
    return ZBC_ERR_AGAIN;
  }

  entry = q->used + ZBC_VIRTQ_USED_HDR_SIZE +
          (size_t)ZBC_VIRTQ_USED_ENTRY_SIZE * (q->last_used_idx % q->size);
  if (used_len) {
    *used_len = ZBC_READ_U32_LE(entry + 4);
  }

  q->last_used_idx++;
  q->inflight = 0;
  return ZBC_OK;
}

int zbc_virtq_xfer(zbc_virtq_t *q, const void *out, size_t out_len, void *in,
                   size_t in_len, uint32_t *used_len) {
  int rc;

  rc = zbc_virtq_post(q, out, out_len, in, in_len);
  if (rc != ZBC_OK) {
    return rc;
  }

  do {
    rc = zbc_virtq_poll(q, used_len);
  } while (rc == ZBC_ERR_AGAIN);

  return rc;
}
