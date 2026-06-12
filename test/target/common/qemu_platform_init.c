/*
 * QEMU Platform Init (shared scaffolding)
 *
 * See qemu_platform_init.h for the contract. This file owns the
 * vcon / 9p / composite / fallback state shared across all QEMU
 * virt-class target platforms; each platform's platform_init.c
 * supplies only the per-arch constants and the SYS_EXIT hook.
 */

#include "common/qemu_platform_init.h"

#include "zbc_9p.h"
#include "zbc_composite.h"
#include "zbc_vcon.h"
#include "zbc_virtio.h"

/* 9p message buffer: half goes to T-messages, half to R-messages, so
 * the negotiated msize is QEMU_PLATFORM_9P_MSGBUF_SIZE / 2. QEMU's 9p
 * server rejects anything below msize=4096, hence 8 KiB here. */
#define QEMU_PLATFORM_9P_MSGBUF_SIZE 8192

static zbc_vcon_state_t g_qemu_platform_vcon;
static zbc_9p_state_t g_qemu_platform_9p;
static zbc_composite_state_t g_qemu_platform_composite;

static uint8_t g_qemu_platform_vcon_arena[ZBC_VCON_ARENA_SIZE];
static uint8_t g_qemu_platform_9p_arena[ZBC_9P_ARENA_SIZE];
static uint8_t g_qemu_platform_9p_msgbuf[QEMU_PLATFORM_9P_MSGBUF_SIZE];

/* The fallback transport needs to find the per-platform exit hook;
 * stashed here at install time. The transport context slot in the
 * client state belongs to the composite, so the hook can't ride
 * along inside it. */
static const zbc_qemu_platform_cfg_t *g_qemu_platform_cfg;

/* Tick count captured at install. SH_SYS_CLOCK and SH_SYS_ELAPSED
 * report time relative to this anchor so the spec's "since program
 * start" semantics line up with whatever moment the platform brought
 * the transport up. */
static uint64_t g_qemu_platform_boot_ticks;

/*------------------------------------------------------------------------
 * Fallback transport (handles the opcodes vcon and 9p don't)
 *------------------------------------------------------------------------*/

static int qemu_platform_fallback_call(zbc_response_t *response,
                                       zbc_client_state_t *state, void *buf,
                                       size_t buf_size, int opcode,
                                       uintptr_t *args)
{
    (void)state;
    (void)buf;
    (void)buf_size;

    if (!response) {
        return ZBC_ERR_NULL_ARG;
    }

    response->data = (const uint8_t *)0;
    response->data_size = 0;
    response->is_error = 0;
    response->proto_error = 0;

    switch (opcode) {
    case SH_SYS_EXIT:
    case SH_SYS_EXIT_EXTENDED:
        if (g_qemu_platform_cfg && g_qemu_platform_cfg->exit_fn) {
            g_qemu_platform_cfg->exit_fn(args ? args[0] : 0);
        }
        /* exit_fn is expected NOT to return; if it does, fall through
         * to a synthetic success so the guest's TARGET_EXIT spins on
         * its own infinite loop instead of looping back into the
         * test driver. */
        response->result = 0;
        response->error_code = 0;
        return ZBC_OK;
    case SH_SYS_CLOCK:
        /* Centiseconds since the moment zbc_qemu_platform_install
         * captured the boot anchor. Falls back to -1 when no platform
         * tick source is wired.
         *
         * The 64-bit delta is narrowed to 32 bits before the divide
         * because 32-bit RISC-V / ARM / x86 don't have a 64/32-bit
         * divide instruction (clang would otherwise emit a libgcc
         * call to __udivdi3, which freestanding builds can't link).
         * The narrow is safe in spirit: SYS_CLOCK returns an `int`,
         * so the answer wraps in 2^31 centiseconds (~248 days)
         * regardless; narrowing the dividend matches that wrap. */
        if (g_qemu_platform_cfg && g_qemu_platform_cfg->ticks_fn
            && g_qemu_platform_cfg->tick_hz != 0) {
            uint64_t now = g_qemu_platform_cfg->ticks_fn();
            uint32_t delta_lo = (uint32_t)(now - g_qemu_platform_boot_ticks);
            uint32_t hz_per_centi = g_qemu_platform_cfg->tick_hz / 100u;
            if (hz_per_centi == 0u) {
                hz_per_centi = 1u;
            }
            response->result = (int)(delta_lo / hz_per_centi);
            response->error_code = 0;
        } else {
            response->result = -1;
            response->error_code = ZBC_ERRNO_ENOSYS;
        }
        return ZBC_OK;
    case SH_SYS_ELAPSED:
        /* 64-bit tick count since boot. The guest passes a pointer to
         * a uint64_t in args[0]; we write the ticks straight in since
         * the fallback runs in the guest's address space. */
        if (g_qemu_platform_cfg && g_qemu_platform_cfg->ticks_fn
            && args && args[0]) {
            uint64_t now = g_qemu_platform_cfg->ticks_fn();
            uint64_t delta = now - g_qemu_platform_boot_ticks;
            uint64_t *dst = (uint64_t *)args[0];
            *dst = delta;
            response->result = 0;
            response->error_code = 0;
        } else {
            response->result = -1;
            response->error_code = ZBC_ERRNO_ENOSYS;
        }
        return ZBC_OK;
    case SH_SYS_TICKFREQ:
        if (g_qemu_platform_cfg && g_qemu_platform_cfg->tick_hz != 0) {
            response->result = (int)g_qemu_platform_cfg->tick_hz;
            response->error_code = 0;
        } else {
            response->result = -1;
            response->error_code = ZBC_ERRNO_ENOSYS;
        }
        return ZBC_OK;
    case SH_SYS_TIME:
        /* Wall-clock seconds since epoch -- no RTC reachable from
         * here. The 9p server's mtime would be close but isn't the
         * guest's notion of "now"; report unsupported and let the
         * guest (Linux's NTP, the harness's tolerant assertion, ...)
         * decide what to do. */
        response->result = -1;
        response->error_code = ZBC_ERRNO_ENOSYS;
        return ZBC_OK;
    case SH_SYS_ISERROR:
        /* Convention: non-zero status is an error. The harness probes
         * with both 0 and -1 and asserts the corresponding 0 / 1. */
        response->result = (args && args[0] != 0) ? 1 : 0;
        response->error_code = 0;
        return ZBC_OK;
    default:
        response->result = -1;
        response->error_code = ZBC_ERRNO_ENOSYS;
        return ZBC_OK;
    }
}

static const zbc_transport_t g_qemu_platform_fallback_vtable = {
    qemu_platform_fallback_call};

/*------------------------------------------------------------------------
 * Installation
 *------------------------------------------------------------------------*/

void zbc_qemu_platform_install(zbc_client_state_t *state,
                               const zbc_qemu_platform_cfg_t *cfg)
{
    volatile void *vcon_slot;
    volatile void *p9_slot;

    g_qemu_platform_cfg = cfg;

    /* Anchor for SH_SYS_CLOCK / SH_SYS_ELAPSED. Read once, before any
     * device init runs, so "since program start" reflects time from
     * the earliest point the platform code can see. */
    if (cfg && cfg->ticks_fn) {
        g_qemu_platform_boot_ticks = cfg->ticks_fn();
    } else {
        g_qemu_platform_boot_ticks = 0;
    }

    g_qemu_platform_composite.console = (const zbc_transport_t *)0;
    g_qemu_platform_composite.console_ctx = (void *)0;
    g_qemu_platform_composite.file = (const zbc_transport_t *)0;
    g_qemu_platform_composite.file_ctx = (void *)0;
    g_qemu_platform_composite.fallback = &g_qemu_platform_fallback_vtable;
    g_qemu_platform_composite.fallback_ctx = (void *)0;

    if (!cfg) {
        /* No window to scan; everything will ENOSYS via the fallback. */
        state->transport = zbc_transport_composite();
        state->transport_ctx = &g_qemu_platform_composite;
        return;
    }

    vcon_slot = zbc_virtio_scan(cfg->mmio_base, cfg->mmio_stride,
                                cfg->mmio_slots, ZBC_VIRTIO_ID_CONSOLE);
    if (vcon_slot != (volatile void *)0
        && zbc_vcon_init(&g_qemu_platform_vcon, vcon_slot,
                         g_qemu_platform_vcon_arena,
                         sizeof(g_qemu_platform_vcon_arena)) == ZBC_OK) {
        g_qemu_platform_composite.console = zbc_transport_vcon();
        g_qemu_platform_composite.console_ctx = &g_qemu_platform_vcon;
    }

    p9_slot = zbc_virtio_scan(cfg->mmio_base, cfg->mmio_stride,
                              cfg->mmio_slots, ZBC_VIRTIO_ID_9P);
    if (p9_slot != (volatile void *)0
        && zbc_9p_setup(&g_qemu_platform_9p, p9_slot,
                        g_qemu_platform_9p_arena,
                        sizeof(g_qemu_platform_9p_arena),
                        g_qemu_platform_9p_msgbuf,
                        sizeof(g_qemu_platform_9p_msgbuf)) == ZBC_OK
        && zbc_9p_mount(&g_qemu_platform_9p) == ZBC_OK) {
        g_qemu_platform_composite.file = zbc_transport_9p();
        g_qemu_platform_composite.file_ctx = &g_qemu_platform_9p;
    }

    state->transport = zbc_transport_composite();
    state->transport_ctx = &g_qemu_platform_composite;
}
