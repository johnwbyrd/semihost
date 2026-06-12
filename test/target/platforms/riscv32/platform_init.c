/*
 * riscv32 (QEMU virt) Platform Init
 *
 * The virt board exposes 8 virtio-mmio slots starting at 0x10001000
 * with stride 0x1000. SYS_EXIT uses the sifive_test "finisher" syscon
 * device at 0x100000: a 32-bit write of 0x5555 terminates the VM with
 * exit code 0, 0x3333 with exit code 1.
 */

#include "common/qemu_platform_init.h"

#include "zbc_protocol.h"

#define RISCV32_VIRTIO_MMIO_BASE   ((volatile void *)0x10001000UL)
#define RISCV32_VIRTIO_MMIO_STRIDE 0x1000U
#define RISCV32_VIRTIO_MMIO_SLOTS  8

#define RISCV32_TEST_FINISHER ((volatile uint32_t *)0x100000UL)
#define RISCV32_TEST_PASS     0x5555U
#define RISCV32_TEST_FAIL     0x3333U

static void riscv32_exit(uintptr_t exit_code)
{
    *RISCV32_TEST_FINISHER = (exit_code == 0) ? RISCV32_TEST_PASS
                                              : RISCV32_TEST_FAIL;
}

static const zbc_qemu_platform_cfg_t g_riscv32_cfg = {
    RISCV32_VIRTIO_MMIO_BASE,
    RISCV32_VIRTIO_MMIO_STRIDE,
    RISCV32_VIRTIO_MMIO_SLOTS,
    riscv32_exit,
};

void zbc_platform_init_transport(zbc_client_state_t *state)
{
    zbc_qemu_platform_install(state, &g_riscv32_cfg);
}
