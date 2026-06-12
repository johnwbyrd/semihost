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

/*
 * ADDING A NEW OPCODE? Read this first.
 *
 * The opcode -> {console, file, fallback} routing here is a closed
 * switch, not a registration table. A SH_SYS_* opcode that doesn't
 * appear in one of the case lists below falls through to the default
 * arm and reaches the fallback transport, which on every QEMU
 * virt-class platform returns -1 / ZBC_ERRNO_ENOSYS (38).
 *
 * That failure mode is silent: the guest just sees ENOSYS, the host
 * never sees the call, and the 9p (or vcon) implementation you spent
 * the morning writing never runs even though it's wired correctly.
 * The SYS_STAT bringup hit exactly this wall -- end-to-end tests
 * returned -1 from the composite path until "case SH_SYS_STAT:" was
 * added to the file class below.
 *
 * Add new opcodes to the case list whose transport serves them:
 *   - console class: vcon (virtio-console) handles writec / write0 /
 *     readc and is the natural home for any future console-only ops
 *   - file class:    9p (virtio-9p) handles open / close / read /
 *     write / seek / flen / remove / rename / tmpnam / istty / stat
 *     and the natural home for further Linux-extension file ops
 *     (SYS_READDIR, SYS_FSTAT, SYS_MKDIR, SYS_FSYNC, ...)
 *   - default arm:   meta opcodes (SYS_EXIT, SYS_CLOCK, ...) ride the
 *     per-platform fallback transport from qemu_platform_init.c
 *
 * Don't forget the host-side dispatch and backend vtable too; see
 * docs/source/linux-extensions.rst for the full per-opcode
 * checklist.
 */
static int composite_select(int opcode, const zbc_composite_state_t *cc,
                            const zbc_transport_t **child_out,
                            void **ctx_out) {
  switch (opcode) {
  case SH_SYS_WRITEC:
  case SH_SYS_WRITE0:
  case SH_SYS_READC:
  case SH_SYS_READC_POLL:
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
  case SH_SYS_STAT:     /* Linux extensions; same routing class as the other file ops */
  case SH_SYS_OPENDIR:
  case SH_SYS_READDIR:
  case SH_SYS_CLOSEDIR:
  case SH_SYS_FSTAT:
  case SH_SYS_MKDIR:
  case SH_SYS_RMDIR:
  case SH_SYS_FTRUNCATE:
  case SH_SYS_FSYNC:
  case SH_SYS_LINK:
  case SH_SYS_SYMLINK:
  case SH_SYS_READLINK:
  case SH_SYS_LSTAT:
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
