/*
 * microvm (QEMU) Platform Init
 *
 * QEMU's microvm machine exposes 8 virtio-mmio slots at base
 * 0xFEB00000 with stride 0x200. SYS_EXIT goes through the
 * -device isa-debug-exit window at I/O port 0xf4: any byte written
 * makes QEMU shut down with process exit (val<<1)|1 (so a clean exit
 * code 0 is unreachable -- ctest grades the test by stdout match on
 * "RESULT: PASS" / "RESULT: FAIL" via the runner's regex properties).
 */

#include "common/qemu_platform_init.h"

#include "zbc_protocol.h"

/* QEMU's microvm allocates 24 virtio-mmio buses (VIRT_MMIO_NUM_TRANSPORTS
 * in QEMU's hw/i386/microvm.c). Auto-bound -device flags grab the
 * highest-numbered free bus, so devices typically land at slot 22 / 23
 * rather than 0 / 1. The library finds them either way -- just scan
 * the whole window. */
#define MICROVM_VIRTIO_MMIO_BASE   ((volatile void *)0xFEB00000UL)
#define MICROVM_VIRTIO_MMIO_STRIDE 0x200U
#define MICROVM_VIRTIO_MMIO_SLOTS  24

/* QEMU isa-debug-exit device: write any byte to terminate the VM. */
#define MICROVM_DEBUG_EXIT_PORT 0xF4

static void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void microvm_exit(uintptr_t exit_code)
{
    /* The exit_code byte is folded into QEMU's process code as
     * (byte<<1)|1, so 0 -> 1 and non-zero -> >= 3. We pass it through
     * raw -- ctest reads pass/fail from the test's own stdout, not
     * from the process code. */
    outb(MICROVM_DEBUG_EXIT_PORT, (uint8_t)exit_code);

    /* If the write fails to terminate the VM (e.g. isa-debug-exit
     * isn't wired), halt forever and let the runner timeout. */
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

/* x86 TSC. QEMU's TCG presents a 1 GHz timestamp counter; that's not
 * spec but it has been stable across QEMU releases and is what
 * microvm guests have to live with absent CPUID-based calibration. */
#define MICROVM_TICK_HZ 1000000000U

static uint64_t microvm_read_ticks(void)
{
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static const zbc_qemu_platform_cfg_t g_microvm_cfg = {
    MICROVM_VIRTIO_MMIO_BASE,
    MICROVM_VIRTIO_MMIO_STRIDE,
    MICROVM_VIRTIO_MMIO_SLOTS,
    microvm_exit,
    microvm_read_ticks,
    MICROVM_TICK_HZ,
};

void zbc_platform_init_transport(zbc_client_state_t *state)
{
    zbc_qemu_platform_install(state, &g_microvm_cfg);
}
