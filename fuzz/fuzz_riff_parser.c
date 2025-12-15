/*
 * Fuzz harness for RIFF parsing
 *
 * Targets: zbc_host_process() - parses untrusted RIFF data from guest
 *
 * Uses dummy backend to avoid real filesystem side effects.
 * Build with: cmake -DENABLE_FUZZING=ON -DCMAKE_C_COMPILER=clang
 * Run with: ./fuzz_riff_parser corpus/riff_parser/
 */

#ifndef ZBC_HOST
#define ZBC_HOST
#endif
#include "zbc_semihost.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/*
 * Mock memory ops that read directly from fuzzer-provided buffer
 */
typedef struct {
    const uint8_t *data;
    size_t size;
    uint8_t work_buf[4096];
} fuzz_ctx_t;

static uint8_t fuzz_read_u8(uintptr_t addr, void *ctx)
{
    fuzz_ctx_t *fctx = (fuzz_ctx_t *)ctx;
    if (addr >= fctx->size) {
        return 0;
    }
    return fctx->data[addr];
}

static void fuzz_write_u8(uintptr_t addr, uint8_t val, void *ctx)
{
    fuzz_ctx_t *fctx = (fuzz_ctx_t *)ctx;
    /* Write to work buffer if in range, else ignore */
    if (addr < sizeof(fctx->work_buf)) {
        fctx->work_buf[addr] = val;
    }
}

static void fuzz_read_block(void *dest, uintptr_t addr, size_t size, void *ctx)
{
    fuzz_ctx_t *fctx = (fuzz_ctx_t *)ctx;
    size_t i;
    uint8_t *d = (uint8_t *)dest;

    for (i = 0; i < size; i++) {
        if (addr + i < fctx->size) {
            d[i] = fctx->data[addr + i];
        } else {
            d[i] = 0;
        }
    }
}

static void fuzz_write_block(uintptr_t addr, const void *src, size_t size,
                             void *ctx)
{
    fuzz_ctx_t *fctx = (fuzz_ctx_t *)ctx;
    const uint8_t *s = (const uint8_t *)src;
    size_t i;

    for (i = 0; i < size; i++) {
        if (addr + i < sizeof(fctx->work_buf)) {
            fctx->work_buf[addr + i] = s[i];
        }
    }
}

/*
 * libFuzzer entry point
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    fuzz_ctx_t fctx;
    zbc_host_state_t state;
    zbc_host_mem_ops_t mem_ops;

    /* Set up fuzzer context */
    memset(&fctx, 0, sizeof(fctx));
    fctx.data = data;
    fctx.size = size;

    /* Set up memory operations */
    mem_ops.read_u8 = fuzz_read_u8;
    mem_ops.write_u8 = fuzz_write_u8;
    mem_ops.read_block = fuzz_read_block;
    mem_ops.write_block = fuzz_write_block;

    /* Initialize host with dummy backend (no real I/O) */
    zbc_host_init(&state, &mem_ops, &fctx,
                  zbc_backend_dummy(), NULL,
                  fctx.work_buf, sizeof(fctx.work_buf));

    /* Process the fuzzed input as a RIFF request at address 0 */
    zbc_host_process(&state, 0);

    return 0;
}
