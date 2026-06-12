/**
 * @file zbc_virtio.h
 * @brief Polled virtio-mmio driver core for the ZBC guest transports
 *
 * A deliberately small guest-side virtio driver: modern (version 2)
 * virtio-mmio only, one polled split virtqueue engine, no interrupts, no
 * heap. The virtio-console and virtio-9p transports are built on top of
 * this core; see docs/source/qemu-transports.rst.
 *
 * Constraints inherited from the ZBC client library:
 * - C90, freestanding-friendly, zero heap allocation (caller provides all
 *   ring memory).
 * - Descriptor addresses are guest-physical: run with the MMU off or
 *   identity-mapped, the same assumption the ZBC device spec makes.
 * - Strictly one outstanding request per queue; completion is detected by
 *   polling the used ring (mirrors "portable guests poll RESPONSE_READY").
 */

#ifndef ZBC_VIRTIO_H
#define ZBC_VIRTIO_H

#include "zbc_protocol.h"
#include "zbc_virtio_wire.h"

#ifdef __cplusplus
extern "C" {
#endif

/*========================================================================
 * Queue sizing
 *========================================================================*/

/** Descriptors per queue. Serial request/response use needs very few. */
#define ZBC_VIRTQ_SIZE 8

/**
 * Bytes of caller-provided memory one queue needs (rings + alignment
 * slack). Pass an arena at least this large to zbc_virtq_init().
 *
 * Sized for the *larger* of the two transport modes: legacy virtio-mmio
 * v1 lays the rings out as one contiguous block aligned to
 * ZBC_VIRTIO_LEGACY_PAGE_SIZE, with another page boundary padded
 * between the available and used rings (the device-programmed
 * QueueAlign). So the worst case is two page-sized blocks plus up to
 * one page of headroom at the start of the caller's arena to round its
 * address up. Modern v2 needs only ~240 bytes and wastes the rest, but
 * the universal size keeps a guest binary portable across both
 * versions without per-platform sizing.
 */
#define ZBC_VIRTQ_ARENA_SIZE                                                   \
  (3 * ZBC_VIRTIO_LEGACY_PAGE_SIZE)

/*========================================================================
 * Types
 *========================================================================*/

/**
 * One split virtqueue, driven synchronously.
 *
 * Initialize with zbc_virtq_init() after zbc_virtio_init() and before
 * zbc_virtio_start(). All fields are managed by the library; the notify
 * callback exists for testing (mirrors doorbell_callback in the client
 * state) and is invoked right after the QueueNotify register write.
 */
typedef struct zbc_virtq_s {
  volatile uint8_t *mmio; /**< Device register window */
  uint16_t queue_index;   /**< Queue index at the device */
  uint16_t size;          /**< Negotiated queue size */
  uint16_t avail_idx;     /**< Driver's running available index */
  uint16_t last_used_idx; /**< Last used index consumed */
  uint8_t inflight;       /**< 1 while a chain is outstanding */
  uint8_t *desc;          /**< Descriptor table (16 * size bytes) */
  uint8_t *avail;         /**< Available ring */
  uint8_t *used;          /**< Used ring */
  void (*notify_callback)(void *); /**< For testing */
  void *notify_ctx;                /**< Context for notify callback */
} zbc_virtq_t;

/*========================================================================
 * Device discovery and initialization
 *========================================================================*/

/**
 * Probe one virtio-mmio slot.
 *
 * Checks the MagicValue and Version registers. Like the ZBC SIGNATURE
 * check, this never has side effects on the device.
 *
 * @param mmio           Slot base address (must allow aligned 32-bit access)
 * @param[out] device_id Receives the DeviceID register value (may be NULL)
 * @return ZBC_OK if a modern virtio-mmio device (or empty slot, device_id
 *         0) is present; ZBC_ERR_DEVICE_ERROR if magic/version mismatch;
 *         ZBC_ERR_NULL_ARG if mmio is NULL
 */
int zbc_virtio_probe(volatile void *mmio, uint32_t *device_id);

/**
 * Scan a virtio-mmio window for a device.
 *
 * Walks 'slots' slots of 'stride' bytes from 'window', probing each, and
 * returns the base of the first slot whose DeviceID matches.
 *
 * @param window    Base of the platform's virtio-mmio window
 * @param stride    Slot stride (e.g. 0x200 on ARM virt, 0x1000 on RISC-V)
 * @param slots     Number of slots in the window
 * @param device_id Device ID to find (ZBC_VIRTIO_ID_*)
 * @return Slot base, or NULL if not found
 */
volatile void *zbc_virtio_scan(volatile void *window, size_t stride,
                               int slots, uint32_t device_id);

/**
 * Reset and initialize a virtio device up to FEATURES_OK.
 *
 * Performs the modern init handshake: reset, ACKNOWLEDGE, DRIVER,
 * negotiate exactly VIRTIO_F_VERSION_1 (no other feature is offered
 * back), FEATURES_OK. On any failure the FAILED status bit is set and an
 * error returned. Set up queues next, then call zbc_virtio_start().
 *
 * @param mmio Device base address
 * @return ZBC_OK, ZBC_ERR_NULL_ARG, or ZBC_ERR_DEVICE_ERROR
 */
int zbc_virtio_init(volatile void *mmio);

/**
 * Complete initialization: set DRIVER_OK. Call after all queues are set
 * up with zbc_virtq_init().
 *
 * @param mmio Device base address
 * @return ZBC_OK or ZBC_ERR_NULL_ARG
 */
int zbc_virtio_start(volatile void *mmio);

/*========================================================================
 * Virtqueue operations
 *========================================================================*/

/**
 * Set up one virtqueue.
 *
 * Carves the descriptor table and rings out of the caller-provided arena
 * (aligning internally), programs the queue registers, and marks the
 * queue ready. The arena must stay valid for the queue's lifetime and be
 * at least ZBC_VIRTQ_ARENA_SIZE bytes.
 *
 * @param q           Queue state to initialize
 * @param mmio        Device base address
 * @param queue_index Queue index at the device
 * @param mem         Ring arena (caller-provided, guest-physical)
 * @param mem_size    Arena size in bytes
 * @return ZBC_OK, ZBC_ERR_NULL_ARG, ZBC_ERR_BUFFER_FULL (arena too
 *         small), or ZBC_ERR_DEVICE_ERROR (queue unavailable)
 */
int zbc_virtq_init(zbc_virtq_t *q, volatile void *mmio, int queue_index,
                   void *mem, size_t mem_size);

/**
 * Post one descriptor chain and notify the device.
 *
 * The chain is device-readable 'out' followed by device-writable 'in';
 * either may be omitted (NULL/0), not both. Buffers must stay valid until
 * the completion is consumed via zbc_virtq_poll(). Only one chain may be
 * outstanding per queue.
 *
 * @param q       Initialized queue
 * @param out     Device-readable buffer (request), or NULL
 * @param out_len Length of out
 * @param in      Device-writable buffer (response), or NULL
 * @param in_len  Length of in
 * @return ZBC_OK, ZBC_ERR_NULL_ARG, ZBC_ERR_INVALID_ARG (no buffers, or
 *         a chain is already outstanding)
 */
int zbc_virtq_post(zbc_virtq_t *q, const void *out, size_t out_len,
                   void *in, size_t in_len);

/**
 * Check for completion of the outstanding chain (non-blocking).
 *
 * @param q             Initialized queue
 * @param[out] used_len Bytes the device wrote into 'in' (may be NULL)
 * @return ZBC_OK if completed, ZBC_ERR_AGAIN if still pending,
 *         ZBC_ERR_NULL_ARG
 */
int zbc_virtq_poll(zbc_virtq_t *q, uint32_t *used_len);

/**
 * Post a chain and spin until it completes.
 *
 * Blocking by design: semihosting operations are synchronous, and the
 * caller decides what "the device never answers" means (a device that
 * accepts DRIVER_OK and then never completes is broken hardware, the
 * same failure class as a ZBC device that never sets RESPONSE_READY).
 *
 * @param q             Initialized queue
 * @param out,out_len   Device-readable buffer
 * @param in,in_len     Device-writable buffer
 * @param[out] used_len Bytes the device wrote into 'in' (may be NULL)
 * @return ZBC_OK or an error from zbc_virtq_post()
 */
int zbc_virtq_xfer(zbc_virtq_t *q, const void *out, size_t out_len,
                   void *in, size_t in_len, uint32_t *used_len);

#ifdef __cplusplus
}
#endif

#endif /* ZBC_VIRTIO_H */
