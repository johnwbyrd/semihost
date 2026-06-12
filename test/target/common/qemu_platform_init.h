/*
 * QEMU Platform Init (shared scaffolding)
 *
 * Every QEMU virt-class target wires up its semihosting transport the
 * same way: scan the virtio-mmio window for a virtio-console and a
 * virtio-9p device, bring each up with the standard ZBC arena sizes,
 * compose them behind zbc_transport_composite(), and install a small
 * fallback transport that handles the meta opcodes (SYS_EXIT,
 * SYS_CLOCK, SYS_ISERROR) neither vcon nor 9p serve.
 *
 * Each platform supplies only:
 *   - the virtio-mmio window (base, stride, slot count)
 *   - an exit hook that terminates the VM (sifive_test write on
 *     riscv32 virt, PSCI HVC on aarch64 virt, the QEMU isa-debug-exit
 *     port on i386 microvm, ...)
 *
 * platforms/<arch>/platform_init.c fills a zbc_qemu_platform_cfg_t and
 * delegates zbc_platform_init_transport() to zbc_qemu_platform_install().
 */

#ifndef ZBC_QEMU_PLATFORM_INIT_H
#define ZBC_QEMU_PLATFORM_INIT_H

#include "zbc_client.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Per-platform configuration consumed by zbc_qemu_platform_install().
 *
 * exit_fn is called once for SH_SYS_EXIT or SH_SYS_EXIT_EXTENDED and
 * is expected NOT to return; if it does, the fallback spins. The
 * argument is args[0] from the syscall (0 = success, non-zero = the
 * test reported failures).
 *
 * ticks_fn reads a monotonic 64-bit tick count from the platform's
 * native timer hardware (CLINT mtime on RISC-V, CNTVCT on ARM
 * Generic Timer, RDTSC on x86). Used to serve SH_SYS_CLOCK,
 * SH_SYS_ELAPSED, and SH_SYS_TICKFREQ. If NULL, those opcodes fall
 * back to -1 (not supported). When non-NULL, tick_hz must be set to
 * the counter's frequency in Hz.
 */
typedef struct {
    volatile void *mmio_base; /**< Base of the virtio-mmio window */
    size_t mmio_stride;       /**< Bytes between slots */
    int mmio_slots;           /**< Number of slots in the window */
    void (*exit_fn)(uintptr_t exit_code);
    uint64_t (*ticks_fn)(void);
    uint32_t tick_hz;
} zbc_qemu_platform_cfg_t;

/**
 * Bring up the standard virtio-console + virtio-9p + composite stack
 * on the configured QEMU virt-class platform and install it as the
 * client's transport. Any device the scan fails to find (or fails to
 * initialise) is left NULL in the composite; its opcodes then fail
 * deterministically with -1 / ZBC_ERRNO_ENOSYS via the fallback
 * transport. Idempotent enough for test use; not meant for repeated
 * setup/teardown.
 */
void zbc_qemu_platform_install(zbc_client_state_t *state,
                               const zbc_qemu_platform_cfg_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* ZBC_QEMU_PLATFORM_INIT_H */
