/*
 * aarch64 (QEMU virt) Platform Init
 *
 * Probes the QEMU `virt` board's virtio-mmio window for a virtio-console
 * and a virtio-9p device, brings them up, and installs the composite
 * transport. The virt board exposes 32 virtio-mmio slots at base
 * 0x0a000000 with stride 0x200, much larger headroom than the riscv32
 * virt board's 8 slots. SYS_EXIT goes through a tiny fallback transport
 * that calls PSCI -- SYSTEM_OFF on success, SYSTEM_RESET on failure
 * (with QEMU's -no-reboot translating that into a non-zero process
 * exit code).
 */

#include "zbc_9p.h"
#include "zbc_client.h"
#include "zbc_composite.h"
#include "zbc_vcon.h"
#include "zbc_virtio.h"

/* QEMU virt machine virtio-mmio window. */
#define AARCH64_VIRTIO_MMIO_BASE   ((volatile void *)0x0a000000UL)
#define AARCH64_VIRTIO_MMIO_STRIDE 0x200U
#define AARCH64_VIRTIO_MMIO_SLOTS  32

/* 9p mount tag QEMU's -fsdev / -device virtio-9p-device wires up. */
#define AARCH64_9P_MOUNT_TAG "zbc"

/* 9p message buffer: half goes to T-messages, half to R-messages, so
 * the negotiated msize is AARCH64_9P_MSGBUF_SIZE / 2. QEMU's 9p server
 * rejects anything below msize=4096, hence 8 KiB here. */
#define AARCH64_9P_MSGBUF_SIZE 8192

/* PSCI function IDs (PSCI 1.0 base set, SMC Calling Convention).
 * QEMU's virt machine answers PSCI calls regardless of EL -- it
 * intercepts HVC #0 directly, so the guest issues them from EL1
 * without needing to drop privilege or set up EL2 vectors. */
#define AARCH64_PSCI_SYSTEM_OFF   0x84000008UL
#define AARCH64_PSCI_SYSTEM_RESET 0x84000009UL

static zbc_vcon_state_t g_aarch64_vcon;
static zbc_9p_state_t g_aarch64_9p;
static zbc_composite_state_t g_aarch64_composite;

static uint8_t g_aarch64_vcon_arena[ZBC_VCON_ARENA_SIZE];
static uint8_t g_aarch64_9p_arena[ZBC_9P_ARENA_SIZE];
static uint8_t g_aarch64_9p_msgbuf[AARCH64_9P_MSGBUF_SIZE];

/*------------------------------------------------------------------------
 * Platform fallback transport
 *
 * Same shape as the riscv32 fallback (handles the meta opcodes
 * neither vcon nor 9p serve) but the exit mechanism differs: aarch64
 * virt has no sifive_test device, so SYS_EXIT routes through PSCI.
 *------------------------------------------------------------------------*/

static void aarch64_psci_call(unsigned long fn_id)
{
    register unsigned long x0 __asm__("x0") = fn_id;
    __asm__ volatile ("hvc #0" : "+r"(x0) : : "memory");
    /* SYSTEM_OFF and SYSTEM_RESET do not return; if they do, the
     * caller spins. */
}

static int aarch64_fallback_call(zbc_response_t *response,
                                 zbc_client_state_t *state, void *buf,
                                 size_t buf_size, int opcode,
                                 uintptr_t *args)
{
    uintptr_t exit_code;

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
        exit_code = args ? args[0] : 0;
        if (exit_code == 0) {
            /* Clean exit, QEMU process returns 0. */
            aarch64_psci_call(AARCH64_PSCI_SYSTEM_OFF);
        } else {
            /* QEMU's -no-reboot turns SYSTEM_RESET into a non-zero
             * process exit, which is exactly what ctest needs to flag
             * the test as failed. */
            aarch64_psci_call(AARCH64_PSCI_SYSTEM_RESET);
        }
        /* Unreachable if PSCI is honored. */
        response->result = 0;
        response->error_code = 0;
        return ZBC_OK;
    case SH_SYS_CLOCK:
        response->result = 0;
        response->error_code = 0;
        return ZBC_OK;
    case SH_SYS_ISERROR:
        response->result = (args && args[0] != 0) ? 1 : 0;
        response->error_code = 0;
        return ZBC_OK;
    default:
        response->result = -1;
        response->error_code = ZBC_ERRNO_ENOSYS;
        return ZBC_OK;
    }
}

static const zbc_transport_t g_aarch64_fallback_vtable = {aarch64_fallback_call};

void zbc_platform_init_transport(zbc_client_state_t *state)
{
    volatile void *vcon_slot;
    volatile void *p9_slot;

    g_aarch64_composite.console = (const zbc_transport_t *)0;
    g_aarch64_composite.console_ctx = (void *)0;
    g_aarch64_composite.file = (const zbc_transport_t *)0;
    g_aarch64_composite.file_ctx = (void *)0;
    g_aarch64_composite.fallback = &g_aarch64_fallback_vtable;
    g_aarch64_composite.fallback_ctx = (void *)0;

    vcon_slot = zbc_virtio_scan(AARCH64_VIRTIO_MMIO_BASE,
                                AARCH64_VIRTIO_MMIO_STRIDE,
                                AARCH64_VIRTIO_MMIO_SLOTS,
                                ZBC_VIRTIO_ID_CONSOLE);
    if (vcon_slot != (volatile void *)0
        && zbc_vcon_init(&g_aarch64_vcon, vcon_slot,
                         g_aarch64_vcon_arena,
                         sizeof(g_aarch64_vcon_arena)) == ZBC_OK) {
        g_aarch64_composite.console = zbc_transport_vcon();
        g_aarch64_composite.console_ctx = &g_aarch64_vcon;
    }

    p9_slot = zbc_virtio_scan(AARCH64_VIRTIO_MMIO_BASE,
                              AARCH64_VIRTIO_MMIO_STRIDE,
                              AARCH64_VIRTIO_MMIO_SLOTS,
                              ZBC_VIRTIO_ID_9P);
    if (p9_slot != (volatile void *)0
        && zbc_9p_setup(&g_aarch64_9p, p9_slot,
                        g_aarch64_9p_arena, sizeof(g_aarch64_9p_arena),
                        g_aarch64_9p_msgbuf,
                        sizeof(g_aarch64_9p_msgbuf)) == ZBC_OK
        && zbc_9p_mount(&g_aarch64_9p) == ZBC_OK) {
        g_aarch64_composite.file = zbc_transport_9p();
        g_aarch64_composite.file_ctx = &g_aarch64_9p;
    }

    state->transport = zbc_transport_composite();
    state->transport_ctx = &g_aarch64_composite;
}
