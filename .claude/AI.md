# AI Context: ZBC Semihosting

Read before making changes.

## Rules

### Never
1. Use `long` in APIs or structs — size varies by platform (32-bit on Windows x64, 64-bit on Linux x64)
2. Assume pointer or integer sizes — must work from 8-bit (6502) to 64-bit systems
3. Cast between `uint64_t` and pointer types
4. Hardcode `/tmp` — use TMPDIR/TMP/TEMP environment variables
5. Propose the host chasing pointers in guest memory — all data goes through RIFF chunks
6. Use C99+ features — this is C90 strict (no `//` comments, no `inline`, no VLAs)
7. Allocate memory in core libraries — caller provides all buffers
8. Consider "backwards compatibility."  Writing to support imaginary existing users is an antifeature.

### Always
1. Read int_size, ptr_size, endianness from CNFG chunk; marshal data accordingly
2. Use fixed-width types (`int32_t`, `uint32_t`, etc.) for values that cross the wire
3. Use `int` where exact size doesn't matter; `size_t` for memory sizes
4. Test mentally: "Would this work on 6502? 8051? ARM64? Windows x64?"
5. Return errors via return values, not global errno
6. Use `zbc_` prefix for all library functions
7. Rewrite code in the simplest, cleanest, most elegant fashion possible

## Why RIFF Marshalling?

Unlike debugger-based semihosting where the host can read guest memory directly, ZBC is a discrete bus peripheral. Having it chase pointers would require DMA, bus arbitration, and access to guest address space.

Instead, the guest packs all data into a single contiguous RIFF buffer. The device reads/writes only that buffer. This is standard DMA-style I/O — you don't expect a disk controller to scatter-gather from arbitrary application pointers either.

## Key Files

- `docs/specification.md` — Wire protocol
- `include/zbc_semi_backend.h` — Backend vtable definition
