/*
 * riscv64 (QEMU virt) Platform Init
 *
 * The virt board's MMIO map is shared between the 32- and 64-bit
 * RISC-V machine types: 8 virtio-mmio slots starting at 0x10001000
 * with stride 0x1000, and the sifive_test "finisher" syscon device at
 * 0x100000 (write 0x5555 for clean exit, 0x3333 for failure). So the
 * only thing that differs from riscv32 is the symbol prefix and the
 * width of uintptr_t, which the shared helper handles via the cfg
 * struct.
 */

#include "common/qemu_platform_init.h"

#include "zbc_protocol.h"

#define RISCV64_VIRTIO_MMIO_BASE   ((volatile void *)0x10001000UL)
#define RISCV64_VIRTIO_MMIO_STRIDE 0x1000U
#define RISCV64_VIRTIO_MMIO_SLOTS  8

#define RISCV64_TEST_FINISHER ((volatile uint32_t *)0x100000UL)
#define RISCV64_TEST_PASS     0x5555U
#define RISCV64_TEST_FAIL     0x3333U

static void riscv64_exit(uintptr_t exit_code)
{
    *RISCV64_TEST_FINISHER = (exit_code == 0) ? RISCV64_TEST_PASS
                                              : RISCV64_TEST_FAIL;
}

static const zbc_qemu_platform_cfg_t g_riscv64_cfg = {
    RISCV64_VIRTIO_MMIO_BASE,
    RISCV64_VIRTIO_MMIO_STRIDE,
    RISCV64_VIRTIO_MMIO_SLOTS,
    riscv64_exit,
};

void zbc_platform_init_transport(zbc_client_state_t *state)
{
    zbc_qemu_platform_install(state, &g_riscv64_cfg);
}
