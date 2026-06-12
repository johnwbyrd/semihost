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

static const zbc_qemu_platform_cfg_t g_arm_cfg = {
    ARM_VIRTIO_MMIO_BASE,
    ARM_VIRTIO_MMIO_STRIDE,
    ARM_VIRTIO_MMIO_SLOTS,
    arm_exit,
};

void zbc_platform_init_transport(zbc_client_state_t *state)
{
    zbc_qemu_platform_install(state, &g_arm_cfg);
}
