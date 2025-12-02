# Client Library Guide

How to use the client library in embedded targets.

## Overview

The client library runs on the guest (embedded target) and communicates with the semihosting device. It handles RIFF encoding/decoding so your code can make simple syscalls.

Most users won't need this directly — if you're using picolibc or newlib with ZBC support, semihosting is already integrated. This guide is for toolchain developers porting to new targets.

## Initialization

```c
#include "zbc_semi_client.h"

zbc_client_state_t client;

/* Initialize with device base address */
zbc_client_init(&client, (void *)0xFFFF0000);

/* Optional: verify device is present */
if (!zbc_client_check_signature(&client)) {
    /* No semihosting device at this address */
}
```

## High-Level Syscall API

The library provides wrappers for all ARM semihosting syscalls:

```c
uint8_t buffer[256];

/* Open a file */
int fd = zbc_sys_open(&client, buffer, sizeof(buffer),
                      "/tmp/test.txt", 4);  /* mode 4 = write */

/* Write to file */
const char *msg = "Hello\n";
int result = zbc_sys_write(&client, buffer, sizeof(buffer),
                           fd, msg, strlen(msg));

/* Close file */
zbc_sys_close(&client, buffer, sizeof(buffer), fd);

/* Console output */
zbc_sys_write0(&client, buffer, sizeof(buffer), "Hello, world!\n");

/* Get time */
unsigned int seconds = zbc_sys_time(&client, buffer, sizeof(buffer));
```

## Low-Level Builder API

For more control, build RIFF requests manually:

```c
zbc_builder_t builder;
zbc_response_t response;
size_t riff_size;

/* Start a new request */
zbc_builder_start(&builder, buffer, sizeof(buffer), &client);

/* Begin a syscall (SYS_WRITE = 0x05) */
zbc_builder_begin_call(&builder, 0x05);

/* Add parameters */
zbc_builder_add_parm_int(&builder, fd);           /* file descriptor */
zbc_builder_add_data_binary(&builder, data, len); /* data to write */
zbc_builder_add_parm_uint(&builder, len);         /* length */

/* Finish and get RIFF size */
zbc_builder_finish(&builder, &riff_size);

/* Submit to device and wait */
zbc_client_submit_poll(&client, buffer, riff_size);

/* Parse response */
zbc_parse_response(&response, buffer, sizeof(buffer), &client);

/* Check result */
if (response.result < 0) {
    /* Error occurred, check response.host_errno */
}
```

## Buffer Management

The library never allocates memory. You provide buffers for all operations.

**Buffer sizing:**
- 256 bytes: Sufficient for most syscalls
- 1024 bytes: Comfortable for large file operations
- Match your largest expected read/write size

**Stack vs static:**
```c
/* Stack allocation (typical) */
void my_function(void) {
    uint8_t buffer[256];
    zbc_sys_write0(&client, buffer, sizeof(buffer), "Hello\n");
}

/* Static allocation (for interrupt handlers or limited stack) */
static uint8_t semihost_buffer[256];
```

## picolibc Integration

To integrate with picolibc's semihosting layer, implement `sys_semihost()`:

```c
#include "zbc_semi_client.h"

static zbc_client_state_t client;
static uint8_t riff_buf[1024];
static int initialized = 0;

uintptr_t sys_semihost(uintptr_t op, uintptr_t param)
{
    if (!initialized) {
        zbc_client_init(&client, (void *)SEMIHOST_DEVICE_ADDR);
        initialized = 1;
    }

    return zbc_semihost(&client, riff_buf, sizeof(riff_buf), op, param);
}
```

The `zbc_semihost()` function handles the ARM-style parameter block format used by picolibc.

## Available Syscalls

| Function | Syscall | Description |
|----------|---------|-------------|
| `zbc_sys_open` | SYS_OPEN | Open file |
| `zbc_sys_close` | SYS_CLOSE | Close file |
| `zbc_sys_read` | SYS_READ | Read from file |
| `zbc_sys_write` | SYS_WRITE | Write to file |
| `zbc_sys_writec` | SYS_WRITEC | Write character to console |
| `zbc_sys_write0` | SYS_WRITE0 | Write string to console |
| `zbc_sys_readc` | SYS_READC | Read character from console |
| `zbc_sys_seek` | SYS_SEEK | Seek in file |
| `zbc_sys_flen` | SYS_FLEN | Get file length |
| `zbc_sys_remove` | SYS_REMOVE | Delete file |
| `zbc_sys_rename` | SYS_RENAME | Rename file |
| `zbc_sys_tmpnam` | SYS_TMPNAM | Generate temp filename |
| `zbc_sys_clock` | SYS_CLOCK | Centiseconds since start |
| `zbc_sys_time` | SYS_TIME | Seconds since epoch |
| `zbc_sys_errno` | SYS_ERRNO | Get last error |
| `zbc_sys_get_cmdline` | SYS_GET_CMDLINE | Get command line |
| `zbc_sys_heapinfo` | SYS_HEAPINFO | Get heap/stack info |
| `zbc_sys_exit` | SYS_EXIT | Exit program |
| `zbc_sys_tickfreq` | SYS_TICKFREQ | Get tick frequency |

## See Also

- [Protocol Specification](specification.md) — RIFF format details
- `include/zbc_semi_client.h` — Full API documentation
