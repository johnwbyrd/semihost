/**
 * @file zbc_composite.h
 * @brief Composite guest transport (opcode-class routing)
 *
 * Multiplexes one client across multiple underlying transports by
 * routing each call to either a "console" or a "file" child based on
 * the opcode class. Anything outside those two classes goes to the
 * fallback child, or fails -1 / ZBC_ERRNO_ENOSYS if no fallback is set.
 *
 * Routing is by opcode only, not by fd. SYS_WRITE / SYS_READ go to the
 * file transport even when the fd is 0, 1, or 2; a guest that wants
 * stdio bytes over the console transport should use SYS_WRITEC /
 * SYS_WRITE0 / SYS_READC instead.
 *
 * Opcode classes:
 *
 * - Console:  SYS_WRITEC, SYS_WRITE0, SYS_READC
 * - File:     SYS_OPEN, SYS_CLOSE, SYS_READ, SYS_WRITE, SYS_SEEK,
 *             SYS_FLEN, SYS_REMOVE, SYS_RENAME, SYS_TMPNAM, SYS_ISTTY
 * - Fallback: everything else (SYS_EXIT, SYS_CLOCK, SYS_TIME, ...)
 *
 * Each child transport sees its own context at state->transport_ctx
 * for the duration of its call: the composite swaps the pointer in
 * before dispatching and restores it after the child returns.
 */

#ifndef ZBC_COMPOSITE_H
#define ZBC_COMPOSITE_H

#include "zbc_client.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Composite transport state. Fill the child pointers and contexts you
 * want, leave the others NULL, place a pointer to the struct in
 * state->transport_ctx, and assign zbc_transport_composite() to
 * state->transport. NULL child slots fail their opcode class with
 * -1 / ZBC_ERRNO_ENOSYS.
 */
typedef struct {
    const zbc_transport_t *console;  /**< Handles SYS_WRITEC/WRITE0/READC */
    void *console_ctx;               /**< Context for the console child */
    const zbc_transport_t *file;     /**< Handles SYS_OPEN/READ/WRITE/... */
    void *file_ctx;                  /**< Context for the file child */
    const zbc_transport_t *fallback; /**< Handles everything else */
    void *fallback_ctx;              /**< Context for the fallback child */
} zbc_composite_state_t;

/**
 * The composite transport vtable.
 *
 * Usage:
 * @code
 *   static zbc_vcon_state_t vcon;
 *   static zbc_9p_state_t p9;
 *   static zbc_composite_state_t composite;
 *
 *   zbc_client_init(&client, NULL);
 *   zbc_vcon_init(&vcon, console_slot, vcon_arena, sizeof(vcon_arena));
 *   zbc_9p_init(&p9, p9_slot, p9_arena, sizeof(p9_arena), "zbc");
 *
 *   composite.console = zbc_transport_vcon();
 *   composite.console_ctx = &vcon;
 *   composite.file = zbc_transport_9p();
 *   composite.file_ctx = &p9;
 *   composite.fallback = (const zbc_transport_t *)0;
 *   composite.fallback_ctx = (void *)0;
 *
 *   client.transport = zbc_transport_composite();
 *   client.transport_ctx = &composite;
 * @endcode
 *
 * @return Pointer to the transport vtable (static, never NULL)
 */
const zbc_transport_t *zbc_transport_composite(void);

#ifdef __cplusplus
}
#endif

#endif /* ZBC_COMPOSITE_H */
