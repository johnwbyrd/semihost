/**
 * @file zbc_virtio_wire.h
 * @brief virtio-mmio and split-virtqueue wire constants
 *
 * Pure wire-format constants for the virtio guest transports, kept in one
 * shared header (like zbc_protocol.h for the RIFF transport) so that every
 * implementation derives them from a single definition and cannot drift.
 *
 * Scope covers virtio-mmio **modern** (version 2) AND **legacy**
 * (version 1) device windows plus the split virtqueue layout, so a guest
 * binary boots unmodified on a stock QEMU "virt" board -- which defaults
 * to legacy v1 -- as well as on a force-modern emulator or future
 * silicon. virtio-pci stays out of scope; see
 * docs/source/qemu-transports.rst.
 *
 * Wire-byte layout of the descriptor / available / used rings is
 * identical between the two versions; only the device-window register
 * set used to *register* the rings (and the feature-negotiation
 * handshake) differs. All multi-byte fields are little-endian per the
 * virtio 1.x specification; legacy devices on big-endian hosts are out
 * of scope.
 */

#ifndef ZBC_VIRTIO_WIRE_H
#define ZBC_VIRTIO_WIRE_H

/*========================================================================
 * virtio-mmio register window (offsets from the device base)
 *
 * Registers are 32 bits wide and MUST be accessed as aligned 32-bit
 * reads/writes (unlike the byte-addressable ZBC device window).
 *========================================================================*/

/* Shared by both versions. */
#define ZBC_VIRTIO_REG_MAGIC            0x000 /* R:  0x74726976 ("virt") */
#define ZBC_VIRTIO_REG_VERSION          0x004 /* R:  1 = legacy, 2 = modern */
#define ZBC_VIRTIO_REG_DEVICE_ID        0x008 /* R:  0 = empty slot */
#define ZBC_VIRTIO_REG_VENDOR_ID        0x00C /* R */
#define ZBC_VIRTIO_REG_DEV_FEATURES     0x010 /* R:  word per FEATURES_SEL */
#define ZBC_VIRTIO_REG_DEV_FEATURES_SEL 0x014 /* W */
#define ZBC_VIRTIO_REG_DRV_FEATURES     0x020 /* W */
#define ZBC_VIRTIO_REG_DRV_FEATURES_SEL 0x024 /* W */
#define ZBC_VIRTIO_REG_QUEUE_SEL        0x030 /* W */
#define ZBC_VIRTIO_REG_QUEUE_NUM_MAX    0x034 /* R:  0 = queue unavailable */
#define ZBC_VIRTIO_REG_QUEUE_NUM        0x038 /* W */
#define ZBC_VIRTIO_REG_QUEUE_NOTIFY     0x050 /* W:  queue index */
#define ZBC_VIRTIO_REG_INT_STATUS       0x060 /* R */
#define ZBC_VIRTIO_REG_INT_ACK          0x064 /* W */
#define ZBC_VIRTIO_REG_STATUS           0x070 /* RW: 0 = reset */
#define ZBC_VIRTIO_REG_CONFIG_GEN       0x0FC /* R */
#define ZBC_VIRTIO_REG_CONFIG           0x100 /* device-specific config */

/* Modern-only registers (split LO/HI for each ring + a per-queue ready). */
#define ZBC_VIRTIO_REG_QUEUE_READY      0x044 /* RW */
#define ZBC_VIRTIO_REG_QUEUE_DESC_LO    0x080 /* W */
#define ZBC_VIRTIO_REG_QUEUE_DESC_HI    0x084 /* W */
#define ZBC_VIRTIO_REG_QUEUE_DRIVER_LO  0x090 /* W:  avail ring */
#define ZBC_VIRTIO_REG_QUEUE_DRIVER_HI  0x094 /* W */
#define ZBC_VIRTIO_REG_QUEUE_DEVICE_LO  0x0A0 /* W:  used ring */
#define ZBC_VIRTIO_REG_QUEUE_DEVICE_HI  0x0A4 /* W */

/* Legacy-only registers (single PFN; rings contiguous in one block,
 * inter-ring alignment programmed via QueueAlign). */
#define ZBC_VIRTIO_REG_GUEST_PAGE_SIZE  0x028 /* W:  legacy PFN page size */
#define ZBC_VIRTIO_REG_QUEUE_ALIGN      0x03C /* W:  legacy ring alignment */
#define ZBC_VIRTIO_REG_QUEUE_PFN        0x040 /* RW: legacy queue base / page */

/** Minimum span of one virtio-mmio slot (the smallest stride in use). */
#define ZBC_VIRTIO_MMIO_SLOT_SIZE       0x200

/** MagicValue: "virt" read as a little-endian 32-bit value. */
#define ZBC_VIRTIO_MMIO_MAGIC           0x74726976UL

/** Device interface versions this library speaks. */
#define ZBC_VIRTIO_VERSION_LEGACY       1
#define ZBC_VIRTIO_VERSION_MODERN       2

/** Page size we tell legacy devices to compute the queue PFN against. */
#define ZBC_VIRTIO_LEGACY_PAGE_SIZE     4096

/*========================================================================
 * Device status bits (STATUS register)
 *========================================================================*/

#define ZBC_VIRTIO_STATUS_ACKNOWLEDGE   0x01
#define ZBC_VIRTIO_STATUS_DRIVER        0x02
#define ZBC_VIRTIO_STATUS_DRIVER_OK     0x04
#define ZBC_VIRTIO_STATUS_FEATURES_OK   0x08
#define ZBC_VIRTIO_STATUS_NEEDS_RESET   0x40
#define ZBC_VIRTIO_STATUS_FAILED        0x80

/*========================================================================
 * Feature bits
 *
 * Features are selected 32 bits at a time via DEV_FEATURES_SEL /
 * DRV_FEATURES_SEL. VIRTIO_F_VERSION_1 is global feature bit 32, i.e.
 * bit 0 of feature word 1. It is the only feature this library
 * acknowledges.
 *========================================================================*/

#define ZBC_VIRTIO_F_VERSION_1_WORD     1
#define ZBC_VIRTIO_F_VERSION_1_MASK     0x00000001UL

/*========================================================================
 * Device IDs used by the ZBC guest transports
 *========================================================================*/

#define ZBC_VIRTIO_ID_NONE              0  /* empty slot */
#define ZBC_VIRTIO_ID_CONSOLE           3
#define ZBC_VIRTIO_ID_9P                9

/*========================================================================
 * Split virtqueue layout
 *
 * Descriptor table entry (16 bytes, little-endian):
 *   addr[8]  - guest-physical buffer address
 *   len[4]   - buffer length
 *   flags[2] - ZBC_VIRTQ_DESC_F_*
 *   next[2]  - next descriptor index (when F_NEXT set)
 *
 * Available ring (driver -> device):
 *   flags[2], idx[2], ring[2 * queue_size]
 *
 * Used ring (device -> driver):
 *   flags[2], idx[2], ring[8 * queue_size]
 *   each used element: id[4] (head descriptor index), len[4] (bytes the
 *   device wrote into device-writable buffers)
 *
 * Alignment: descriptor table 16, available ring 2, used ring 4.
 *========================================================================*/

#define ZBC_VIRTQ_DESC_F_NEXT           1  /* chain continues at 'next' */
#define ZBC_VIRTQ_DESC_F_WRITE          2  /* device-writable buffer */

#define ZBC_VIRTQ_AVAIL_F_NO_INTERRUPT  1  /* polite hint: driver polls */

#define ZBC_VIRTQ_DESC_ENTRY_SIZE       16
#define ZBC_VIRTQ_AVAIL_HDR_SIZE        4  /* flags + idx */
#define ZBC_VIRTQ_USED_HDR_SIZE         4  /* flags + idx */
#define ZBC_VIRTQ_USED_ENTRY_SIZE       8  /* id + len */

#define ZBC_VIRTQ_DESC_ALIGN            16
#define ZBC_VIRTQ_AVAIL_ALIGN           2
#define ZBC_VIRTQ_USED_ALIGN            4

#endif /* ZBC_VIRTIO_WIRE_H */
