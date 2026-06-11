/*
 * ZBC virtio-console Transport
 *
 * Console semihosting opcodes over virtio-console queues; everything
 * else fails with -1 / ENOSYS. Observable semantics match the host
 * backends: WRITE/READ return bytes NOT transferred, fds 0-2 are the
 * console, errno is sticky for SYS_ERRNO.
 */

#include "zbc_vcon.h"

/*========================================================================
 * Initialization
 *========================================================================*/

int zbc_vcon_init(zbc_vcon_state_t *vcon, volatile void *mmio, void *queue_mem,
                  size_t mem_size) {
  uint32_t device_id;
  uint8_t *arena = (uint8_t *)queue_mem;
  size_t half;
  int rc;

  if (!vcon || !mmio || !queue_mem) {
    return ZBC_ERR_NULL_ARG;
  }
  if (mem_size < ZBC_VCON_ARENA_SIZE) {
    return ZBC_ERR_BUFFER_FULL;
  }

  rc = zbc_virtio_probe(mmio, &device_id);
  if (rc != ZBC_OK) {
    return rc;
  }
  if (device_id != ZBC_VIRTIO_ID_CONSOLE) {
    ZBC_LOG_ERROR("vcon: device ID %u is not a console", (unsigned)device_id);
    return ZBC_ERR_DEVICE_ERROR;
  }

  rc = zbc_virtio_init(mmio);
  if (rc != ZBC_OK) {
    return rc;
  }

  half = mem_size / 2;
  rc = zbc_virtq_init(&vcon->rx, mmio, ZBC_VCON_QUEUE_RX, arena, half);
  if (rc != ZBC_OK) {
    return rc;
  }
  rc = zbc_virtq_init(&vcon->tx, mmio, ZBC_VCON_QUEUE_TX, arena + half,
                      mem_size - half);
  if (rc != ZBC_OK) {
    return rc;
  }

  vcon->last_errno = 0;

  return zbc_virtio_start(mmio);
}

/*========================================================================
 * Console I/O helpers
 *========================================================================*/

/* Transmit a buffer. The device reads it in place (zero copy). */
static int vcon_tx(zbc_vcon_state_t *vcon, const void *data, size_t len) {
  if (len == 0) {
    return ZBC_OK;
  }
  return zbc_virtq_xfer(&vcon->tx, data, len, (void *)0, 0, (uint32_t *)0);
}

/* Receive into a buffer; blocks until the device returns some bytes. */
static int vcon_rx(zbc_vcon_state_t *vcon, void *dest, size_t len,
                   size_t *got) {
  uint32_t used_len = 0;
  int rc;

  *got = 0;
  if (len == 0) {
    return ZBC_OK;
  }
  rc = zbc_virtq_xfer(&vcon->rx, (const void *)0, 0, dest, len, &used_len);
  if (rc != ZBC_OK) {
    return rc;
  }
  *got = (size_t)used_len;
  return ZBC_OK;
}

/*========================================================================
 * Transport entry point
 *========================================================================*/

static void vcon_fill_response(zbc_response_t *response, int result,
                          int error_code) {
  response->result = result;
  response->error_code = error_code;
  response->data = (const uint8_t *)0;
  response->data_size = 0;
  response->is_error = 0;
  response->proto_error = 0;
}

static int vcon_transport_call(zbc_response_t *response,
                               zbc_client_state_t *state, void *buf,
                               size_t buf_size, int opcode, uintptr_t *args) {
  zbc_vcon_state_t *vcon = (zbc_vcon_state_t *)state->transport_ctx;
  int rc;

  (void)buf;
  (void)buf_size;

  if (!vcon) {
    return ZBC_ERR_NOT_INITIALIZED;
  }

  switch (opcode) {
  case SH_SYS_WRITEC: {
    /* args[0] = pointer to the character (ARM parameter block layout) */
    uint8_t c = *(const uint8_t *)args[0];
    rc = vcon_tx(vcon, &c, 1);
    if (rc != ZBC_OK) {
      return rc;
    }
    vcon_fill_response(response, 0, 0);
    return ZBC_OK;
  }

  case SH_SYS_WRITE0: {
    const char *str = (const char *)args[0];
    rc = vcon_tx(vcon, str, zbc_strlen(str));
    if (rc != ZBC_OK) {
      return rc;
    }
    vcon_fill_response(response, 0, 0);
    return ZBC_OK;
  }

  case SH_SYS_WRITE: {
    /* args = {fd, buf, count}; returns bytes NOT written */
    int fd = (int)args[0];
    if (fd == 1 || fd == 2) {
      rc = vcon_tx(vcon, (const void *)args[1], (size_t)args[2]);
      if (rc != ZBC_OK) {
        return rc;
      }
      vcon_fill_response(response, 0, 0);
    } else {
      vcon->last_errno = ZBC_ERRNO_EBADF;
      vcon_fill_response(response, -1, ZBC_ERRNO_EBADF);
    }
    return ZBC_OK;
  }

  case SH_SYS_READ: {
    /* args = {fd, buf, count}; returns bytes NOT read */
    int fd = (int)args[0];
    if (fd == 0) {
      uint8_t *dest = (uint8_t *)args[1];
      size_t count = (size_t)args[2];
      size_t got;

      rc = vcon_rx(vcon, dest, count, &got);
      if (rc != ZBC_OK) {
        return rc;
      }
      vcon_fill_response(response, (int)(count - got), 0);
      response->data = dest;
      response->data_size = got;
    } else {
      vcon->last_errno = ZBC_ERRNO_EBADF;
      vcon_fill_response(response, -1, ZBC_ERRNO_EBADF);
    }
    return ZBC_OK;
  }

  case SH_SYS_READC: {
    uint8_t c = 0;
    size_t got;

    rc = vcon_rx(vcon, &c, 1, &got);
    if (rc != ZBC_OK) {
      return rc;
    }
    vcon_fill_response(response, (got == 1) ? (int)c : -1, 0);
    return ZBC_OK;
  }

  case SH_SYS_ISTTY: {
    int fd = (int)args[0];
    vcon_fill_response(response, (fd >= 0 && fd <= 2) ? 1 : 0, 0);
    return ZBC_OK;
  }

  case SH_SYS_ISERROR: {
    intptr_t status = (intptr_t)args[0];
    vcon_fill_response(response, (status < 0) ? 1 : 0, 0);
    return ZBC_OK;
  }

  case SH_SYS_ERRNO:
    vcon_fill_response(response, vcon->last_errno, 0);
    return ZBC_OK;

  default:
    /* File ops belong to the 9p transport; time/exit to platform hooks. */
    vcon->last_errno = ZBC_ERRNO_ENOSYS;
    vcon_fill_response(response, -1, ZBC_ERRNO_ENOSYS);
    return ZBC_OK;
  }
}

static const zbc_transport_t vcon_transport_vtable = {vcon_transport_call};

const zbc_transport_t *zbc_transport_vcon(void) {
  return &vcon_transport_vtable;
}
