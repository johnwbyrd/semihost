/**
 * @file zbc_9p.h
 * @brief virtio-9p (9P2000.L) guest file transport
 *
 * Serves the file semihosting opcodes over a virtio-9p device (virtio
 * device ID 9) on a stock emulator -- QEMU's built-in 9p export, no
 * external daemon. The opcode-to-9P mapping is specified normatively in
 * docs/source/qemu-transports-proposal.rst.
 *
 * Opcodes served: SYS_OPEN, SYS_CLOSE, SYS_READ, SYS_WRITE, SYS_SEEK,
 * SYS_FLEN, SYS_REMOVE, SYS_RENAME, SYS_TMPNAM, SYS_ISTTY, SYS_ISERROR,
 * SYS_ERRNO. Console fds (0-2) and everything else fail with
 * EBADF/ENOSYS; route them to the console transport in a composite.
 *
 * Because 9p reads and writes carry explicit offsets, the file position
 * (and therefore SYS_SEEK) is pure guest-side state in the fd table,
 * which is private to this transport. Offsets and sizes are carried at
 * the wire's full 64-bit width regardless of the guest's word size, so
 * even an 8-bit guest can stream through files larger than its own
 * address space.
 */

#ifndef ZBC_9P_H
#define ZBC_9P_H

#include "zbc_9p_wire.h"
#include "zbc_client.h"
#include "zbc_virtio.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Open files at once. fds are slot index + 3 (0-2 are the console). */
#define ZBC_9P_MAX_FILES 8

/** Ring arena size zbc_9p_setup() needs (one request/response queue). */
#define ZBC_9P_ARENA_SIZE ZBC_VIRTQ_ARENA_SIZE

/** Smallest workable message buffer (split into request/reply halves). */
#define ZBC_9P_MSGBUF_MIN 256

/** One open file: 9p fid plus guest-side position state. */
typedef struct {
  uint64_t offset; /**< File position (SYS_SEEK state), full wire width */
  uint8_t in_use;
} zbc_9p_file_t;

/**
 * 9p transport state. Place a pointer to an initialized instance in the
 * client state's transport_ctx.
 */
typedef struct {
  zbc_virtq_t vq; /**< Request/response queue (queue 0) */
  uint8_t *tx;    /**< T-message build buffer */
  uint8_t *rx;    /**< R-message receive buffer */
  uint32_t msize; /**< Negotiated maximum message size */
  zbc_9p_file_t files[ZBC_9P_MAX_FILES];
  int last_errno;     /**< Sticky errno for SYS_ERRNO */
  int tmpnam_counter; /**< For SYS_TMPNAM name generation */
} zbc_9p_state_t;

/**
 * Set up the virtio device and queue (no 9p traffic yet).
 *
 * Probes the slot (DeviceID must be ZBC_VIRTIO_ID_9P), runs the virtio
 * handshake, sets up queue 0 from the ring arena, splits the message
 * buffer into request/reply halves, and starts the device. Call
 * zbc_9p_mount() next to establish the 9p session.
 *
 * @param s              Transport state to initialize
 * @param mmio           virtio-mmio slot base (e.g. from zbc_virtio_scan())
 * @param queue_mem      Ring arena, at least ZBC_9P_ARENA_SIZE bytes
 * @param queue_mem_size Ring arena size
 * @param msg_buf        Message buffer, at least ZBC_9P_MSGBUF_MIN bytes;
 *                       its half-size bounds the I/O chunk per round trip
 * @param msg_buf_size   Message buffer size
 * @return ZBC_OK, ZBC_ERR_NULL_ARG, ZBC_ERR_BUFFER_FULL, or
 *         ZBC_ERR_DEVICE_ERROR
 */
int zbc_9p_setup(zbc_9p_state_t *s, volatile void *mmio, void *queue_mem,
                 size_t queue_mem_size, void *msg_buf, size_t msg_buf_size);

/**
 * Establish the 9p session: Tversion (negotiating msize) and Tattach
 * (obtaining the root fid). Call after zbc_9p_setup().
 *
 * @param s Transport state from zbc_9p_setup()
 * @return ZBC_OK or ZBC_ERR_DEVICE_ERROR (version/attach refused)
 */
int zbc_9p_mount(zbc_9p_state_t *s);

/**
 * The virtio-9p file transport vtable.
 *
 * Usage:
 * @code
 *   static zbc_9p_state_t p9;
 *   static uint8_t arena[ZBC_9P_ARENA_SIZE];
 *   static uint8_t msgs[2048];
 *
 *   zbc_client_init(&client, NULL);
 *   zbc_9p_setup(&p9, slot, arena, sizeof(arena), msgs, sizeof(msgs));
 *   zbc_9p_mount(&p9);
 *   client.transport = zbc_transport_9p();
 *   client.transport_ctx = &p9;
 * @endcode
 *
 * @return Pointer to the transport vtable (static, never NULL)
 */
const zbc_transport_t *zbc_transport_9p(void);

#ifdef __cplusplus
}
#endif

#endif /* ZBC_9P_H */
