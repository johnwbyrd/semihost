/*
 * aarch64 (QEMU virt) Platform Init
 *
 * The virt board exposes 32 virtio-mmio slots at base 0x0a000000 with
 * stride 0x200. SYS_EXIT uses PSCI: SYSTEM_OFF (HVC #0 with the PSCI
 * function ID 0x84000008) terminates the VM with exit code 0;
 * SYSTEM_RESET (0x84000009) combined with QEMU's -no-reboot terminates
 * with a non-zero exit code, which ctest then reads as a failure.
 *
 * QEMU virt intercepts HVC #0 itself regardless of which EL the guest
 * runs at, so we issue the call straight from EL1 without setting up
 * vectors.
 */

#include "common/qemu_platform_init.h"

#include "zbc_protocol.h"

#define AARCH64_VIRTIO_MMIO_BASE   ((volatile void *)0x0a000000UL)
#define AARCH64_VIRTIO_MMIO_STRIDE 0x200U
#define AARCH64_VIRTIO_MMIO_SLOTS  32

/* PSCI 1.0 base set, SMC Calling Convention function IDs. */
#define AARCH64_PSCI_SYSTEM_OFF   0x84000008UL
#define AARCH64_PSCI_SYSTEM_RESET 0x84000009UL

static void aarch64_psci_call(unsigned long fn_id)
{
    register unsigned long x0 __asm__("x0") = fn_id;
    __asm__ volatile ("hvc #0" : "+r"(x0) : : "memory");
}

static void aarch64_exit(uintptr_t exit_code)
{
    aarch64_psci_call(exit_code == 0 ? AARCH64_PSCI_SYSTEM_OFF
                                     : AARCH64_PSCI_SYSTEM_RESET);
}

static const zbc_qemu_platform_cfg_t g_aarch64_cfg = {
    AARCH64_VIRTIO_MMIO_BASE,
    AARCH64_VIRTIO_MMIO_STRIDE,
    AARCH64_VIRTIO_MMIO_SLOTS,
    aarch64_exit,
};

void zbc_platform_init_transport(zbc_client_state_t *state)
{
    zbc_qemu_platform_install(state, &g_aarch64_cfg);
}
