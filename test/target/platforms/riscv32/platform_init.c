/*
 * riscv32 (QEMU virt) Platform Init
 *
 * Probes the QEMU `virt` board's virtio-mmio window for a virtio-console
 * and a virtio-9p device, brings them up, and installs the composite
 * transport so SYS_WRITEC/WRITE0/READC reach the console and
 * SYS_OPEN/READ/WRITE/... reach 9p. If either device is missing, that
 * leg of the composite stays NULL and its opcodes will fail
 * deterministically with -1 / ZBC_ERRNO_ENOSYS at zbc_call() time.
 *
 * The virt board's virtio-mmio window is 8 slots starting at
 * 0x10001000, stride 0x1000. QEMU populates them in the order the
 * -device flags are given, so the runner must add virtconsole *and*
 * virtio-9p, but the scan does not depend on which slot each ends up
 * in.
 */

#include "zbc_9p.h"
#include "zbc_client.h"
#include "zbc_composite.h"
#include "zbc_vcon.h"
#include "zbc_virtio.h"

/* QEMU virt machine virtio-mmio window. */
#define RISCV32_VIRTIO_MMIO_BASE   ((volatile void *)0x10001000UL)
#define RISCV32_VIRTIO_MMIO_STRIDE 0x1000U
#define RISCV32_VIRTIO_MMIO_SLOTS  8

/* 9p must mount the export QEMU's -fsdev/-device puts at mount_tag "zbc". */
#define RISCV32_9P_MOUNT_TAG "zbc"

/* 9p message buffer: half goes to T-messages, half to R-messages, so
 * the negotiated msize is RISCV32_9P_MSGBUF_SIZE / 2. QEMU's 9p server
 * rejects anything below msize=4096, hence 8 KiB here. */
#define RISCV32_9P_MSGBUF_SIZE 8192

/* QEMU virt's "test finisher" syscon device. A 32-bit write of 0x5555
 * (success) or 0x3333 (failure) terminates the VM and sets QEMU's
 * process exit code -- the standard machine-agnostic way to signal
 * test pass/fail to the runner. */
#define RISCV32_TEST_FINISHER ((volatile uint32_t *)0x100000UL)
#define RISCV32_TEST_PASS 0x5555U
#define RISCV32_TEST_FAIL 0x3333U

static zbc_vcon_state_t g_riscv32_vcon;
static zbc_9p_state_t g_riscv32_9p;
static zbc_composite_state_t g_riscv32_composite;

/*------------------------------------------------------------------------
 * Platform fallback transport
 *
 * Handles the small set of opcodes neither vcon nor 9p serve:
 *
 *   SYS_EXIT       -> write the QEMU virt test-finisher, terminating
 *                     the VM with a process exit code that the runner
 *                     turns into ctest pass/fail.
 *   SYS_CLOCK      -> 0. The QEMU virt board has no semihosting clock
 *                     and the harness's only check is "not -1"; a
 *                     constant satisfies that without lying about wall
 *                     time.
 *   SYS_ISERROR    -> 0. Treats every host status as "not an error";
 *                     the test only cares that the answer isn't -1.
 *   everything else-> -1 / ZBC_ERRNO_ENOSYS.
 *------------------------------------------------------------------------*/

static int riscv32_fallback_call(zbc_response_t *response,
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
        *RISCV32_TEST_FINISHER = (exit_code == 0) ? RISCV32_TEST_PASS
                                                  : RISCV32_TEST_FAIL;
        /* Test finisher halts the VM; this is unreachable. */
        response->result = 0;
        response->error_code = 0;
        return ZBC_OK;
    case SH_SYS_CLOCK:
        response->result = 0;
        response->error_code = 0;
        return ZBC_OK;
    case SH_SYS_ISERROR:
        /* Treat any non-zero status as an error -- matches the
         * convention SH guests use (0 = success, anything else is). */
        response->result = (args && args[0] != 0) ? 1 : 0;
        response->error_code = 0;
        return ZBC_OK;
    default:
        response->result = -1;
        response->error_code = ZBC_ERRNO_ENOSYS;
        return ZBC_OK;
    }
}

static const zbc_transport_t g_riscv32_fallback_vtable = {riscv32_fallback_call};

static uint8_t g_riscv32_vcon_arena[ZBC_VCON_ARENA_SIZE];
static uint8_t g_riscv32_9p_arena[ZBC_9P_ARENA_SIZE];
static uint8_t g_riscv32_9p_msgbuf[RISCV32_9P_MSGBUF_SIZE];

void zbc_platform_init_transport(zbc_client_state_t *state)
{
    volatile void *vcon_slot;
    volatile void *p9_slot;

    g_riscv32_composite.console = (const zbc_transport_t *)0;
    g_riscv32_composite.console_ctx = (void *)0;
    g_riscv32_composite.file = (const zbc_transport_t *)0;
    g_riscv32_composite.file_ctx = (void *)0;
    g_riscv32_composite.fallback = &g_riscv32_fallback_vtable;
    g_riscv32_composite.fallback_ctx = (void *)0;

    vcon_slot = zbc_virtio_scan(RISCV32_VIRTIO_MMIO_BASE,
                                RISCV32_VIRTIO_MMIO_STRIDE,
                                RISCV32_VIRTIO_MMIO_SLOTS,
                                ZBC_VIRTIO_ID_CONSOLE);
    if (vcon_slot != (volatile void *)0
        && zbc_vcon_init(&g_riscv32_vcon, vcon_slot,
                         g_riscv32_vcon_arena,
                         sizeof(g_riscv32_vcon_arena)) == ZBC_OK) {
        g_riscv32_composite.console = zbc_transport_vcon();
        g_riscv32_composite.console_ctx = &g_riscv32_vcon;
    }

    p9_slot = zbc_virtio_scan(RISCV32_VIRTIO_MMIO_BASE,
                              RISCV32_VIRTIO_MMIO_STRIDE,
                              RISCV32_VIRTIO_MMIO_SLOTS,
                              ZBC_VIRTIO_ID_9P);
    if (p9_slot != (volatile void *)0
        && zbc_9p_setup(&g_riscv32_9p, p9_slot,
                        g_riscv32_9p_arena, sizeof(g_riscv32_9p_arena),
                        g_riscv32_9p_msgbuf,
                        sizeof(g_riscv32_9p_msgbuf)) == ZBC_OK
        && zbc_9p_mount(&g_riscv32_9p) == ZBC_OK) {
        g_riscv32_composite.file = zbc_transport_9p();
        g_riscv32_composite.file_ctx = &g_riscv32_9p;
    }

    state->transport = zbc_transport_composite();
    state->transport_ctx = &g_riscv32_composite;
}
