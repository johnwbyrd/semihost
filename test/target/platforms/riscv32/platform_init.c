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

/* QEMU virt's CLINT timer: 64-bit mtime at 0x0200_BFF8, runs at a
 * fixed 10 MHz (timebase-frequency = 10000000 in the QEMU virt DTB).
 * On rv32 we read the two halves as 32-bit MMIO and retry on overflow
 * across the upper word. */
#define RISCV32_CLINT_MTIME_LO ((volatile uint32_t *)0x0200BFF8UL)
#define RISCV32_CLINT_MTIME_HI ((volatile uint32_t *)0x0200BFFCUL)
#define RISCV32_TICK_HZ        10000000U

static uint64_t riscv32_read_ticks(void)
{
    uint32_t hi1, lo, hi2;
    do {
        hi1 = *RISCV32_CLINT_MTIME_HI;
        lo  = *RISCV32_CLINT_MTIME_LO;
        hi2 = *RISCV32_CLINT_MTIME_HI;
    } while (hi1 != hi2);
    return ((uint64_t)hi1 << 32) | lo;
}

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
    riscv32_read_ticks,
    RISCV32_TICK_HZ,
};

void zbc_platform_init_transport(zbc_client_state_t *state)
{
    zbc_qemu_platform_install(state, &g_riscv32_cfg);
}
