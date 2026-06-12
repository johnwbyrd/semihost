/*
 * arm (QEMU virt, 32-bit) Platform Init
 *
 * QEMU's virt machine memory map is shared between AArch32 and AArch64,
 * so the virtio-mmio window is identical to the aarch64 platform: 32
 * slots at base 0x0a000000 with stride 0x200. SYS_EXIT goes through
 * PSCI via HVC #0: QEMU's arm virt machine defaults to secure=off,
 * which selects HVC as the PSCI conduit (the SMC trap is treated as
 * UNDEF when secure extensions are absent). aarch64 virt also uses
 * HVC; only -machine virt,secure=on flips arm to SMC.
 */

#include "common/qemu_platform_init.h"

#include "zbc_protocol.h"

#define ARM_VIRTIO_MMIO_BASE   ((volatile void *)0x0a000000UL)
#define ARM_VIRTIO_MMIO_STRIDE 0x200U
#define ARM_VIRTIO_MMIO_SLOTS  32

/* PSCI 1.0 base set, same function IDs as aarch64. */
#define ARM_PSCI_SYSTEM_OFF   0x84000008UL
#define ARM_PSCI_SYSTEM_RESET 0x84000009UL

static void arm_psci_call(unsigned long fn_id)
{
    register unsigned long r0 __asm__("r0") = fn_id;
    __asm__ volatile ("hvc #0" : "+r"(r0) : : "memory");
}

static void arm_exit(uintptr_t exit_code)
{
    arm_psci_call(exit_code == 0 ? ARM_PSCI_SYSTEM_OFF
                                 : ARM_PSCI_SYSTEM_RESET);
}

/* ARMv7-A Generic Timer (cortex-a15 has it).
 * CNTVCT (64-bit virtual count): MRRC p15, 1, lo, hi, c14.
 * CNTFRQ (32-bit frequency):     MRC  p15, 0, r, c14, c0, 0. */
static uint64_t arm_read_ticks(void)
{
    uint32_t lo, hi;
    __asm__ volatile ("mrrc p15, 1, %0, %1, c14" : "=r"(lo), "=r"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static uint32_t arm_read_freq(void)
{
    uint32_t freq;
    __asm__ volatile ("mrc p15, 0, %0, c14, c0, 0" : "=r"(freq));
    return freq;
}

static zbc_qemu_platform_cfg_t g_arm_cfg = {
    ARM_VIRTIO_MMIO_BASE,
    ARM_VIRTIO_MMIO_STRIDE,
    ARM_VIRTIO_MMIO_SLOTS,
    arm_exit,
    arm_read_ticks,
    0, /* tick_hz filled in at init from CNTFRQ */
};

void zbc_platform_init_transport(zbc_client_state_t *state)
{
    g_arm_cfg.tick_hz = arm_read_freq();
    zbc_qemu_platform_install(state, &g_arm_cfg);
}
