/*
 * ZBC Composite Transport
 *
 * Opcode-class router. See zbc_composite.h for the public contract.
 *
 * A child transport reads its own context from state->transport_ctx
 * (per the per-transport contract), so before dispatching we swap the
 * child's context into that field and restore the caller's pointer
 * afterwards. The state pointer is never reentered concurrently.
 */

#include "zbc_composite.h"

static void composite_fill_enosys(zbc_response_t *response) {
  response->result = -1;
  response->error_code = ZBC_ERRNO_ENOSYS;
  response->data = (const uint8_t *)0;
  response->data_size = 0;
  response->is_error = 0;
  response->proto_error = 0;
}

static int composite_select(int opcode, const zbc_composite_state_t *cc,
                            const zbc_transport_t **child_out,
                            void **ctx_out) {
  switch (opcode) {
  case SH_SYS_WRITEC:
  case SH_SYS_WRITE0:
  case SH_SYS_READC:
    *child_out = cc->console;
    *ctx_out = cc->console_ctx;
    return 1;
  case SH_SYS_OPEN:
  case SH_SYS_CLOSE:
  case SH_SYS_READ:
  case SH_SYS_WRITE:
  case SH_SYS_SEEK:
  case SH_SYS_FLEN:
  case SH_SYS_REMOVE:
  case SH_SYS_RENAME:
  case SH_SYS_TMPNAM:
  case SH_SYS_ISTTY:
    *child_out = cc->file;
    *ctx_out = cc->file_ctx;
    return 1;
  default:
    *child_out = cc->fallback;
    *ctx_out = cc->fallback_ctx;
    return 1;
  }
}

static int composite_call(zbc_response_t *response,
                          zbc_client_state_t *state, void *buf,
                          size_t buf_size, int opcode, uintptr_t *args) {
  zbc_composite_state_t *cc;
  const zbc_transport_t *child;
  void *child_ctx;
  void *saved_ctx;
  int rc;

  if (!response || !state) {
    return ZBC_ERR_NULL_ARG;
  }
  cc = (zbc_composite_state_t *)state->transport_ctx;
  if (!cc) {
    return ZBC_ERR_NULL_ARG;
  }

  child = (const zbc_transport_t *)0;
  child_ctx = (void *)0;
  composite_select(opcode, cc, &child, &child_ctx);

  if (!child) {
    composite_fill_enosys(response);
    return ZBC_OK;
  }

  saved_ctx = state->transport_ctx;
  state->transport_ctx = child_ctx;
  rc = child->call(response, state, buf, buf_size, opcode, args);
  state->transport_ctx = saved_ctx;
  return rc;
}

static const zbc_transport_t composite_transport_vtable = {composite_call};

const zbc_transport_t *zbc_transport_composite(void) {
  return &composite_transport_vtable;
}
