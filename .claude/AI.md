# AI Context for ZBC Semihosting Project

This file contains critical design information that must persist across conversation context resets.

---

## ⚠️ MANDATORY: READ THIS ENTIRE FILE BEFORE ANY WORK ⚠️

**DO NOT SKIM. DO NOT SKIP. READ EVERY SECTION.**

If you are resuming from a context reset or summary, you MUST read this file completely before writing any code, making any suggestions, or taking any action. The summary does not contain all the critical constraints.

Previous AI sessions have made serious mistakes by:
- Not reading this file before starting work
- Assuming the summary contained all requirements
- Writing code that only works on the development machine
- Using `long` everywhere (which varies by platform)
- Hardcoding paths like `/tmp` (Linux-only)

**YOU WILL MAKE THESE SAME MISTAKES IF YOU DON'T READ THIS FILE.**

---

## CRITICAL: NEVER ASSUME POINTER OR INTEGER SIZES

**STOP. READ THIS BEFORE WRITING ANY CODE.**

The client library must run on EVERYTHING from a MOS 6502 (8-bit, 16-bit pointers) to 64-bit systems.

**YOU MUST NEVER:**
- Use `sizeof(int)`, `sizeof(long)`, `sizeof(void*)` as if they're fixed values
- Assume `long` is 32 bits or 64 bits (it's 32 on Windows x64, 64 on Linux x64!)
- Assume pointers are any particular size
- Cast between `uint64_t` and pointer types
- Use `uintptr_t` assuming it exists or has a known size
- Write code that only works on your development machine (Linux x86_64)
- Use `long` in APIs or data structures - it's not portable!

**YOU MUST:**
- Read int_size, ptr_size, endianness from the CNFG chunk
- Marshal/unmarshal integers byte-by-byte using the configured sizes
- Test mentally: "Would this work on a 6502? An 8051? A PDP-11? ARM64? Windows x64?"
- Use the protocol's explicit size fields, never implicit host sizes
- Use `int32_t`, `uint32_t`, `int64_t`, `uint64_t` for fixed-width values
- Use `int` only where the exact size doesn't matter (loop counters, booleans)
- Use `size_t` for memory sizes and counts

**If you find yourself writing `sizeof(long)` or `(void*)(uintptr_t)value`, STOP.**

**If you find yourself writing `long` in a function signature or struct, STOP and think about what size you actually need.**

---

## CRITICAL: CROSS-PLATFORM COMPATIBILITY

The CI runs on Ubuntu, macOS, AND Windows. Code must work on all three.

**YOU MUST NEVER:**
- Hardcode `/tmp` - use environment variables (TMPDIR, TMP, TEMP)
- Assume path separators are `/` - Windows uses `\`
- Assume any particular directory structure

**YOU MUST:**
- Check the `.github/workflows/ci.yml` to understand the test matrix
- Use portable APIs and avoid platform-specific assumptions

---

## CRITICAL DESIGN PRINCIPLE - WHY ZBC MARSHALS DATA INTO RIFF CHUNKS

**Do NOT propose passing pointers for the host to chase. This is WRONG for ZBC.**

### Background

The ARM semihosting model (and picolibc's trap-based implementation) works because a debugger or trap handler already has full access to the CPU's memory space and can read/write anywhere. The guest passes pointers, and the host just reaches into guest memory to follow them.

### Why ZBC is Different

**For a memory-mapped peripheral that operates as a discrete device on the bus**, having it autonomously master the bus to chase arbitrary pointers is:

1. **More complex hardware** - requires DMA engine, bus arbitration
2. **A security/stability concern** - device can read/write anywhere in memory
3. **Architecturally messy** - device needs to understand guest's address space, MMU state, etc.

### The Correct Approach

**The RIFF-embedded approach is cleaner for a peripheral model:**

1. Guest packs everything into a single contiguous buffer
2. Guest tells device "here's my buffer" (via RIFF_PTR register)
3. Device reads/writes ONLY that buffer
4. Guest unpacks the result

The "cost" is that the client library must copy data into/out of the RIFF buffer. But this is **standard DMA-style I/O** - you don't expect a disk controller to scatter-gather from arbitrary application pointers either.

### Picolibc Integration Implication

The client library handles the packing/unpacking internally, presenting the same API that picolibc expects (`sys_semihost(op, param)`) while copying data to/from the RIFF buffer.

The `virtual.c` in picolibc must:
1. Receive `(op, param)` where `param` points to argument struct
2. **Read the argument struct from local memory** (guest-side)
3. **Marshal those values into RIFF PARM/DATA chunks**
4. Submit via ZBC protocol (RIFF_PTR + DOORBELL)
5. Parse RETN response
6. **Copy any returned DATA back to caller's buffers** (for SYS_READ, etc.)
7. Return result

**If you find yourself suggesting the host should chase pointers, STOP and re-read this file.**

---

## Project Structure

- `/home/jbyrd/git/semihost/` - ZBC semihosting spec and libraries
  - `docs/specification.md` - Protocol specification
  - `include/zbc_semi_*.h` - Library headers
  - `src/client/` - Guest-side client library
  - `src/host/` - Emulator-side host library
  - `src/host/backends/` - Backend implementations

- `/home/jbyrd/git/picolibc/semihost/` - Picolibc semihosting integration
  - `machine/virtual/` - Memory-mapped semihosting backend (uses ZBC)
  - `common/` - Architecture-agnostic high-level API

---

## User Requirements

1. **Two independent C libraries** (C90 strict):
   - **Client library**: Guest-side. Packs semihosting calls into RIFF, dispatches to device.
   - **Host library**: Emulator-side. Parses RIFF, dispatches to backends, builds response.

2. **Zero memory allocation** (core libraries): Caller provides all buffers. Exception: ANSI backend may allocate for tracking open files.

3. **Single RIFF buffer per call**: Library works within caller-provided buffer. Fails if insufficient space.

4. **Vtable-based backend system**:
   - Init receives a vtable of typed function pointers for syscall implementations.
   - At least two backends provided:
     - **Dummy backend**: All operations succeed with no side effects.
     - **ANSI backend**: Uses only ANSI C library calls (fopen, fclose, fread, fwrite, etc.).
   - Clear documentation for users to implement custom backends.

5. **Config values cached**: Endianness, int_size, ptr_size set once per session.

6. **Error handling**: No global errno for private functions. ANSI backend may use errno where stdio requires it. Library functions return error codes.

7. **C90 strict**: No `//` comments, no C99 features.

8. **Portable integers**: No assumptions about sizeof(int) or sizeof(void*). No casting uint64_t to pointer. Must work from 6502 to 64-bit.

9. **Documentation**: All requirements recorded in this file.

---

## Key Design Decisions

- **RIFF marshalling**: All data copied into/out of RIFF DATA chunks (no pointer chasing)
- **Single buffer per call**: Caller provides one contiguous buffer
- **Zero allocation**: Core libraries allocate no memory (ANSI backend may allocate)
- **Vtable pattern**: Function pointers for memory access and backend operations
- **Return codes**: No global errno - errors via return values
- **C90 compatible**: Maximum portability
- **`zbc_` prefix**: All library functions use this prefix
- **Wrap values individually**: When returning multiple values (e.g., SYS_HEAPINFO's 4 pointers), each value gets its own PARM chunk - do NOT collapse into a single binary DATA blob

---

## Architecture Overview

### What picolibc Does

picolibc's semihosting layer provides POSIX-like functions (open, read, write, close) to embedded programs. Each function eventually calls:

```c
uintptr_t sys_semihost(uintptr_t op, uintptr_t param);
```

Where:
- `op` is the ARM semihosting opcode (SYS_READ=0x06, SYS_WRITE=0x05, etc.)
- `param` is a pointer to an architecture-specific parameter block in guest memory

For example, `sys_semihost_read(fd, buf, count)` creates a stack struct `{fd, buf, count}` and calls `sys_semihost(SYS_READ, &struct)`.

### What the Client Library Does

The client library implements `sys_semihost()` for the ZBC virtual device:

1. Receives `(op, param)` where `param` points to the parameter block
2. Reads the parameter block from guest memory (it's already in guest memory - no copy needed)
3. Based on `op`, interprets the parameter block fields
4. For SYS_WRITE: Copies data from the buffer pointer in the param block into a RIFF DATA chunk
5. For SYS_READ: Reserves space in RIFF for response DATA chunk
6. Submits RIFF to device, waits for response
7. For SYS_READ: Copies DATA chunk contents back to the buffer pointer in the param block
8. Returns result

### What the Host Library Does

The host library runs in the emulator (MAME, QEMU, etc.):

1. When guest triggers DOORBELL, host reads RIFF buffer from guest memory
2. Parses CNFG (first time) and CALL chunks
3. Dispatches to backend vtable based on opcode
4. Backend executes the actual operation (file I/O, console I/O, etc.)
5. Builds RETN response with result, errno, and any returned data
6. Writes RETN back to guest memory (overwrites CALL)

---

## Backend Vtable Design

**IMPORTANT: Use `int` not `long` in the vtable.** `long` is not portable (32 bits on Windows x64, 64 bits on Linux x64). ARM semihosting returns register-sized values (32-bit for 32-bit guests), so `int` is correct for the host-side API.

```c
/*
 * Backend vtable for semihosting operations.
 * Each function corresponds to an ARM semihosting syscall.
 * Return values follow ARM semihosting conventions.
 * ctx is passed through from zbc_host_init().
 *
 * NOTE: Use int (not long) for return values - long varies by platform!
 */
typedef struct zbc_backend {
    /* File operations */
    int (*open)(void *ctx, const char *path, size_t path_len, int mode);
    int (*close)(void *ctx, int fd);
    int (*read)(void *ctx, int fd, void *buf, size_t count);
    int (*write)(void *ctx, int fd, const void *buf, size_t count);
    int (*seek)(void *ctx, int fd, int pos);
    int (*flen)(void *ctx, int fd);
    int (*remove)(void *ctx, const char *path, size_t path_len);
    int (*rename)(void *ctx, const char *old_path, size_t old_len,
                  const char *new_path, size_t new_len);
    int (*tmpnam)(void *ctx, char *buf, size_t buf_size, int id);

    /* Console I/O */
    void (*writec)(void *ctx, char c);
    void (*write0)(void *ctx, const char *str);
    int (*readc)(void *ctx);

    /* Status queries */
    int (*iserror)(void *ctx, int status);
    int (*istty)(void *ctx, int fd);

    /* Time */
    int (*clock)(void *ctx);
    int (*time)(void *ctx);
    int (*elapsed)(void *ctx, unsigned int *lo, unsigned int *hi);
    int (*tickfreq)(void *ctx);

    /* System */
    int (*do_system)(void *ctx, const char *cmd, size_t cmd_len);
    int (*get_cmdline)(void *ctx, char *buf, size_t buf_size);
    int (*heapinfo)(void *ctx, unsigned int *heap_base,
                    unsigned int *heap_limit, unsigned int *stack_base,
                    unsigned int *stack_limit);
    void (*do_exit)(void *ctx, unsigned int reason, unsigned int subcode);

    /* Error */
    int (*get_errno)(void *ctx);
} zbc_backend_t;

/* Pre-built backends */
const zbc_backend_t *zbc_backend_dummy(void);
const zbc_backend_t *zbc_backend_ansi(void);
```

---

## Client Interface

The client provides a single entry point compatible with picolibc's `sys_semihost`:

```c
/*
 * Main semihosting entry point.
 *
 * op: ARM semihosting opcode
 * param: Pointer to parameter block (format depends on opcode)
 * riff_buf: Caller-provided buffer for RIFF request/response
 * riff_buf_size: Size of buffer
 *
 * Returns: Syscall result (interpretation depends on opcode)
 *
 * The function marshals parameters from the param block into RIFF,
 * submits to the device, and unmarshals any returned data back
 * to buffers referenced in the param block.
 */
uintptr_t zbc_semihost(
    zbc_client_state_t *state,
    unsigned char *riff_buf,
    size_t riff_buf_size,
    uintptr_t op,
    uintptr_t param
);
```

---

## Per-Opcode Parameter Block Formats

From ARM semihosting spec (and picolibc usage):

| Opcode | Param Block | Client Action |
|--------|-------------|---------------|
| SYS_OPEN (0x01) | {path_ptr, mode, path_len} | Copy string from path_ptr to DATA |
| SYS_CLOSE (0x02) | {fd} | PARM for fd |
| SYS_WRITEC (0x03) | {char_ptr} | Read byte, put in PARM |
| SYS_WRITE0 (0x04) | {str_ptr} | Copy string to DATA |
| SYS_WRITE (0x05) | {fd, buf_ptr, count} | Copy count bytes from buf_ptr to DATA |
| SYS_READ (0x06) | {fd, buf_ptr, count} | Reserve DATA; copy response to buf_ptr |
| SYS_READC (0x07) | (none) | No params; return char in result |
| SYS_ISERROR (0x08) | {status} | PARM for status |
| SYS_ISTTY (0x09) | {fd} | PARM for fd |
| SYS_SEEK (0x0A) | {fd, pos} | PARM for fd, pos |
| SYS_FLEN (0x0C) | {fd} | PARM for fd |
| SYS_TMPNAM (0x0D) | {buf_ptr, id, maxlen} | Reserve DATA; copy to buf_ptr |
| SYS_REMOVE (0x0E) | {path_ptr, path_len} | Copy string to DATA |
| SYS_RENAME (0x0F) | {old_ptr, old_len, new_ptr, new_len} | Two DATA chunks |
| SYS_CLOCK (0x10) | (none) | No params |
| SYS_TIME (0x11) | (none) | No params |
| SYS_SYSTEM (0x12) | {cmd_ptr, cmd_len} | Copy string to DATA |
| SYS_ERRNO (0x13) | (none) | No params |
| SYS_GET_CMDLINE (0x15) | {buf_ptr, size} | Reserve DATA; copy to buf_ptr |
| SYS_HEAPINFO (0x16) | {block_ptr} | 4 pointers returned in PARM chunks; copy to block |
| SYS_EXIT (0x18) | {reason} | PARM for reason |
| SYS_EXIT_EXTENDED (0x20) | {reason, subcode} | PARM for each |
| SYS_ELAPSED (0x30) | {tick_ptr} | Returns 64-bit via DATA; copy to tick_ptr |
| SYS_TICKFREQ (0x31) | (none) | No params |

---

## C90 Compliance Notes

- Use `/* */` comments only
- Declare variables at start of blocks
- No `inline` keyword (C99)
- No `_Bool` (use int)
- No designated initializers for arrays
- No variable-length arrays
- No `restrict` keyword
- Use `unsigned int` not `uint32_t` where possible for max portability
- NEVER use `long` - it's not portable!

---

## Files

### New Files
- `include/zbc_semi_backend.h` - Backend vtable definition
- `src/host/backends/zbc_backend_dummy.c` - Dummy backend
- `src/host/backends/zbc_backend_ansi.c` - ANSI C backend

### Rewritten Files
- `include/zbc_semi_client.h` - Single entry point matching picolibc
- `include/zbc_semi_host.h` - Backend vtable integration
- `include/zbc_semi_common.h` - C90 compliance review
- `src/client/zbc_client_core.c` - Per-opcode marshalling
- `src/host/zbc_host_core.c` - Backend dispatch

### Removed Files
- `src/client/zbc_client_syscalls.c` - High-level wrappers no longer needed
