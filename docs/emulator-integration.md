# Emulator Integration Guide

How to add ZBC semihosting support to your emulator.

## Overview

The semihosting device is a 32-byte memory-mapped peripheral that provides I/O services to guest programs. When the guest writes to the DOORBELL register, your emulator processes the request and writes the response back to guest memory.

## Device Register Map

Map these 32 bytes at your chosen base address (e.g., `0xFFFF0000`):

| Offset | Size | Name | Access | Description |
|--------|------|------|--------|-------------|
| 0x00 | 8 | SIGNATURE | R | ASCII "SEMIHOST" |
| 0x08 | 16 | RIFF_PTR | RW | Pointer to RIFF buffer in guest RAM |
| 0x18 | 1 | DOORBELL | W | Write triggers request processing |
| 0x19 | 1 | IRQ_STATUS | R | Bit 0: response ready |
| 0x1A | 1 | IRQ_ENABLE | RW | Bit 0: enable IRQ on response |
| 0x1B | 1 | IRQ_ACK | W | Write 1 to clear IRQ |
| 0x1C | 1 | STATUS | R | Bit 0: response ready, Bit 7: device present |
| 0x1D | 3 | — | — | Reserved |

## Request Flow

1. Guest writes RIFF buffer address to RIFF_PTR
2. Guest writes any value to DOORBELL
3. **Your emulator**: Read RIFF from guest memory, process it, write response
4. Your emulator: Set STATUS bit 0 (and optionally assert IRQ)
5. Guest reads response from RIFF buffer

## Using the Host Library

The host library handles RIFF parsing and syscall dispatch. You provide:
- Memory operations to read/write guest RAM
- A backend that implements the actual syscalls

### Minimal Integration

```c
#include "zbc_semi_host.h"
#include "zbc_semi_backend.h"

/* Your emulator's memory access functions */
static uint8_t my_read_u8(void *ctx, uint64_t addr) {
    return read_guest_memory(addr);
}

static void my_write_u8(void *ctx, uint64_t addr, uint8_t val) {
    write_guest_memory(addr, val);
}

static void my_read_block(void *ctx, uint64_t addr, void *buf, size_t len) {
    for (size_t i = 0; i < len; i++)
        ((uint8_t*)buf)[i] = read_guest_memory(addr + i);
}

static void my_write_block(void *ctx, uint64_t addr, const void *buf, size_t len) {
    for (size_t i = 0; i < len; i++)
        write_guest_memory(addr + i, ((const uint8_t*)buf)[i]);
}

/* Setup */
static zbc_host_state_t host;
static uint8_t work_buffer[1024];

void semihost_init(void) {
    zbc_host_mem_ops_t mem_ops = {
        .read_u8 = my_read_u8,
        .write_u8 = my_write_u8,
        .read_block = my_read_block,
        .write_block = my_write_block
    };

    zbc_host_init(&host, &mem_ops, NULL,
                  zbc_backend_ansi(), NULL,
                  work_buffer, sizeof(work_buffer));
}

/* Called when guest writes to DOORBELL */
void semihost_doorbell(uint64_t riff_ptr) {
    zbc_host_process(&host, riff_ptr);
    /* Now set STATUS bit 0 in your device registers */
}
```

## Writing a Custom Backend

The ANSI backend uses standard C file I/O. For sandboxing, logging, or virtual filesystems, implement your own backend.

### Backend Structure

```c
typedef struct zbc_backend {
    int (*open)(void *ctx, const char *path, size_t path_len, int mode);
    int (*close)(void *ctx, int fd);
    int (*read)(void *ctx, int fd, void *buf, size_t count);
    int (*write)(void *ctx, int fd, const void *buf, size_t count);
    int (*seek)(void *ctx, int fd, int pos);
    int (*flen)(void *ctx, int fd);
    int (*remove)(void *ctx, const char *path, size_t path_len);
    int (*rename)(void *ctx, const char *old, size_t old_len,
                  const char *new, size_t new_len);
    int (*tmpnam)(void *ctx, char *buf, size_t buf_size, int id);
    void (*writec)(void *ctx, char c);
    void (*write0)(void *ctx, const char *str);
    int (*readc)(void *ctx);
    int (*iserror)(void *ctx, int status);
    int (*istty)(void *ctx, int fd);
    int (*clock)(void *ctx);
    int (*time)(void *ctx);
    int (*elapsed)(void *ctx, unsigned int *lo, unsigned int *hi);
    int (*tickfreq)(void *ctx);
    int (*do_system)(void *ctx, const char *cmd, size_t cmd_len);
    int (*get_cmdline)(void *ctx, char *buf, size_t buf_size);
    int (*heapinfo)(void *ctx, unsigned int *heap_base,
                    unsigned int *heap_limit, unsigned int *stack_base,
                    unsigned int *stack_limit);
    void (*do_exit)(void *ctx, unsigned int reason, unsigned int subcode);
    int (*get_errno)(void *ctx);
} zbc_backend_t;
```

### Return Value Conventions

- **open**: Returns fd (≥0) on success, -1 on error
- **read/write**: Returns bytes NOT transferred (0 = complete success)
- **close, seek, remove, rename**: Returns 0 on success, -1 on error
- **flen**: Returns file length, -1 on error
- **clock**: Returns centiseconds since start
- **time**: Returns seconds since Unix epoch

### Example: Sandboxed Backend

```c
static int sandboxed_open(void *ctx, const char *path, size_t len, int mode) {
    char safe_path[256];

    /* Reject absolute paths and parent traversal */
    if (path[0] == '/' || strstr(path, ".."))
        return -1;

    /* Prefix with sandbox directory */
    snprintf(safe_path, sizeof(safe_path), "/sandbox/%.*s", (int)len, path);

    /* Use ANSI backend for actual I/O */
    return zbc_backend_ansi()->open(ctx, safe_path, strlen(safe_path), mode);
}

static const zbc_backend_t sandboxed_backend = {
    .open = sandboxed_open,
    .close = zbc_backend_ansi()->close,
    .read = zbc_backend_ansi()->read,
    .write = zbc_backend_ansi()->write,
    /* ... delegate other operations to ANSI backend ... */
};
```

### Partial Implementation

Set unused function pointers to NULL. The host library returns appropriate errors for unimplemented operations.

```c
static const zbc_backend_t minimal_backend = {
    .open = my_open,
    .close = my_close,
    .read = my_read,
    .write = my_write,
    .writec = my_writec,
    /* Everything else NULL - returns error to guest */
};
```

## Built-in Backends

| Backend | Function | Description |
|---------|----------|-------------|
| Dummy | `zbc_backend_dummy()` | All ops succeed, no side effects |
| ANSI C | `zbc_backend_ansi()` | Real file I/O via fopen/fread/etc |

Call `zbc_backend_ansi_cleanup()` before exit to close open files.

## See Also

- [Protocol Specification](specification.md) — RIFF format details
- `include/zbc_semi_backend.h` — Full backend API documentation
