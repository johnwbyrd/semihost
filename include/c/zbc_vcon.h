/**
 * @file zbc_vcon.h
 * @brief virtio-console guest transport
 *
 * Serves the console semihosting opcodes over a virtio-console device
 * (virtio device ID 3) on a stock emulator -- no ZBC device required.
 * The MULTIPORT feature is never negotiated, which pins the device to
 * the simple two-queue layout: queue 0 receive, queue 1 transmit.
 *
 * Opcodes served: SYS_WRITEC, SYS_WRITE0, SYS_READC, SYS_WRITE and
 * SYS_READ on fds 0-2, SYS_ISTTY, SYS_ISERROR, SYS_ERRNO. Everything
 * else fails with -1 / ZBC_ERRNO_ENOSYS (file operations belong to the
 * 9p transport; see docs/source/qemu-transports.rst).
 */

#ifndef ZBC_VCON_H
#define ZBC_VCON_H

#include "zbc_client.h"
#include "zbc_virtio.h"

#ifdef __cplusplus
extern "C" {
#endif

/** virtio-console queue indices (without MULTIPORT). */
#define ZBC_VCON_QUEUE_RX 0
#define ZBC_VCON_QUEUE_TX 1

/** Ring arena size zbc_vcon_init() needs (two queues). */
#define ZBC_VCON_ARENA_SIZE (2 * ZBC_VIRTQ_ARENA_SIZE)

/**
 * Console transport state. Place a pointer to an initialized instance in
 * the client state's transport_ctx.
 */
typedef struct {
    zbc_virtq_t rx;      /**< Receive queue (device -> guest) */
    zbc_virtq_t tx;      /**< Transmit queue (guest -> device) */
    int last_errno;      /**< Sticky errno for SYS_ERRNO */
} zbc_vcon_state_t;

/**
 * Initialize the console transport over a virtio-console device.
 *
 * Probes the slot (the DeviceID must be ZBC_VIRTIO_ID_CONSOLE), runs the
 * virtio init handshake, sets up the receive and transmit queues from
 * the caller-provided arena, and starts the device.
 *
 * @param vcon      Transport state to initialize
 * @param mmio      virtio-mmio slot base (e.g. from zbc_virtio_scan())
 * @param queue_mem Ring arena, at least ZBC_VCON_ARENA_SIZE bytes,
 *                  guest-physical, valid for the transport's lifetime
 * @param mem_size  Arena size in bytes
 * @return ZBC_OK, ZBC_ERR_NULL_ARG, ZBC_ERR_BUFFER_FULL (arena too
 *         small), or ZBC_ERR_DEVICE_ERROR (wrong device / init failure)
 */
int zbc_vcon_init(zbc_vcon_state_t *vcon, volatile void *mmio,
                  void *queue_mem, size_t mem_size);

/**
 * The virtio-console transport vtable.
 *
 * Usage:
 * @code
 *   static zbc_vcon_state_t vcon;
 *   static uint8_t arena[ZBC_VCON_ARENA_SIZE];
 *
 *   zbc_client_init(&client, NULL);
 *   zbc_vcon_init(&vcon, slot, arena, sizeof(arena));
 *   client.transport = zbc_transport_vcon();
 *   client.transport_ctx = &vcon;
 * @endcode
 *
 * @return Pointer to the transport vtable (static, never NULL)
 */
const zbc_transport_t *zbc_transport_vcon(void);

#ifdef __cplusplus
}
#endif

#endif /* ZBC_VCON_H */
