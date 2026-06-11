/*
 * ZBC Null Transport
 *
 * Selected when transport discovery finds no usable device, or assigned
 * explicitly. Every operation fails immediately and deterministically:
 * result -1 with errno ZBC_ERRNO_ENOSYS in the response. No hardware is
 * touched and nothing ever hangs.
 *
 * See docs/source/qemu-transports-proposal.rst ("Design Decisions").
 */

#include "zbc_semihost.h"

static int null_transport_call(zbc_response_t *response,
                               zbc_client_state_t *state, void *buf,
                               size_t buf_size, int opcode, uintptr_t *args) {
  (void)state;
  (void)buf;
  (void)buf_size;
  (void)opcode;
  (void)args;

  if (!response) {
    return ZBC_ERR_INVALID_ARG;
  }

  response->result = -1;
  response->error_code = ZBC_ERRNO_ENOSYS;
  response->data = (const uint8_t *)0;
  response->data_size = 0;
  response->is_error = 0;
  response->proto_error = 0;

  return ZBC_OK;
}

static const zbc_transport_t null_transport_vtable = {null_transport_call};

const zbc_transport_t *zbc_transport_null(void) {
  return &null_transport_vtable;
}
