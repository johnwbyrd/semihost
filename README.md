# ZBC Semihosting Libraries

C libraries implementing a RIFF-based semihosting protocol for embedded systems development.

## Overview

This project provides client and host libraries for a memory-mapped semihosting peripheral. Unlike traditional semihosting (which relies on trap instructions and debugger support), this approach uses standard memory-mapped I/O, making it architecture-agnostic and suitable for any CPU from 8-bit to 128-bit.

The protocol implements all ARM semihosting syscalls (file I/O, console I/O, timekeeping) using RIFF (Resource Interchange File Format) as the wire format. This enables self-describing, extensible communication between guest and host.

### Key Features

- **Architecture-agnostic**: Works with any integer size (8-bit through 128-bit)
- **ARM semihosting compatible**: Uses standard ARM semihosting syscall numbers
- **Zero allocation**: Libraries allocate no memory; caller provides all buffers
- **C90 compatible**: Maximum portability across compilers and platforms
- **Self-describing protocol**: Explicit configuration of integer size, pointer size, and endianness

## Project Structure

```
semihost/
  include/
    zbc_semi_common.h   # Shared definitions (FourCCs, opcodes, error codes)
    zbc_semi_client.h   # Client library public API
    zbc_semi_host.h     # Host library public API
  src/
    client/
      zbc_client_core.c     # RIFF builder, parser, device I/O
      zbc_client_syscalls.c # High-level syscall wrappers
    host/
      zbc_host_core.c       # RIFF parsing, dispatch, response building
  test/
    test_main.c             # Test runner
    test_client_builder.c   # Client builder unit tests
    test_roundtrip.c        # Client/host integration tests
    test_harness.h          # Test framework
    mock_device.h/c         # Mock device for testing
    mock_memory.h/c         # Mock memory operations
  docs/
    specification.md        # Full protocol specification
```

## Libraries

### Client Library (zbc_semi_client)

The client library runs on the guest (embedded target) side. It provides:

- **RIFF builder**: Constructs RIFF request structures with CALL, PARM, and DATA chunks
- **Response parser**: Extracts results and data from RETN responses
- **Device I/O**: Writes to device registers and polls for completion
- **High-level syscall API**: Convenient wrappers for all ARM semihosting operations

Example usage:

```c
#include "zbc_semi_client.h"

uint8_t buffer[256];
zbc_client_state_t client;
zbc_builder_t builder;
zbc_response_t response;
size_t riff_size;

/* Initialize client with device base address */
zbc_client_init(&client, (void *)0xFFFF0000);

/* Build SYS_WRITE request */
zbc_builder_start(&builder, buffer, sizeof(buffer), &client);
zbc_builder_begin_call(&builder, SH_SYS_WRITE);
zbc_builder_add_parm_int(&builder, 1);  /* fd = stdout */
zbc_builder_add_data_binary(&builder, "Hello\n", 6);
zbc_builder_add_parm_uint(&builder, 6); /* length */
zbc_builder_finish(&builder, &riff_size);

/* Submit and wait for response */
zbc_client_submit_poll(&client, buffer, riff_size);

/* Parse response */
zbc_parse_response(&response, buffer, sizeof(buffer), &client);
```

### Host Library (zbc_semi_host)

The host library runs on the emulator or hardware implementation side. It provides:

- **RIFF parser**: Extracts opcode, parameters, and data from CALL chunks
- **Syscall dispatch**: Routes requests to registered handlers via vtable
- **Response builder**: Constructs RETN chunks with results and optional data
- **Memory abstraction**: Vtable for guest memory access (supports emulators and DMA)

Example usage:

```c
#include "zbc_semi_host.h"

uint8_t work_buffer[1024];
zbc_host_state_t host;
zbc_host_mem_ops_t mem_ops = {
    .read_u8 = my_read_u8,
    .write_u8 = my_write_u8,
    .read_block = my_read_block,
    .write_block = my_write_block
};

/* Initialize host */
zbc_host_init(&host, &mem_ops, NULL, work_buffer, sizeof(work_buffer));

/* Register syscall handlers */
zbc_host_set_handler(&host, SH_SYS_WRITE, my_write_handler);
zbc_host_set_handler(&host, SH_SYS_READ, my_read_handler);

/* Process a request (called when guest writes DOORBELL) */
uint64_t riff_addr = /* read from RIFF_PTR register */;
zbc_host_process(&host, riff_addr);
```

## Building

The project uses CMake and builds on Windows, macOS, and Linux.

```bash
# Configure
cmake -B build

# Build
cmake --build build

# Run tests
ctest --test-dir build
```

For specific generators:

```bash
cmake -B build -G "Unix Makefiles"
cmake -B build -G "Visual Studio 17 2022"
cmake -B build -G "Xcode"
```

## Testing

The test suite includes:

- **Client builder tests**: Verify RIFF structure construction, boundary conditions, and error handling
- **Round-trip tests**: Full client-to-host integration testing all supported syscalls

Run tests:

```bash
ctest --test-dir build --output-on-failure
```

Run specific test suites:

```bash
./build/test/zbc_tests builder    # Client builder tests only
./build/test/zbc_tests roundtrip  # Round-trip tests only
```

## Protocol Overview

Communication uses RIFF containers with form type 'SEMI'. The guest builds a request containing:

1. **CNFG chunk** (first request only): Declares int_size, ptr_size, and endianness
2. **CALL chunk**: Contains opcode and nested PARM/DATA chunks for arguments

The device processes the request and overwrites the CALL chunk with:

- **RETN chunk**: Contains result, errno, and optional DATA for returned bytes
- **ERRO chunk**: Contains error code if the request was malformed

See [docs/specification.md](docs/specification.md) for the complete protocol specification.

## Supported Syscalls

| Opcode | Name | Description |
|--------|------|-------------|
| 0x01 | SYS_OPEN | Open file |
| 0x02 | SYS_CLOSE | Close file |
| 0x03 | SYS_WRITEC | Write character to console |
| 0x04 | SYS_WRITE0 | Write null-terminated string |
| 0x05 | SYS_WRITE | Write to file |
| 0x06 | SYS_READ | Read from file |
| 0x07 | SYS_READC | Read character from console |
| 0x08 | SYS_ISERROR | Check if value is error |
| 0x09 | SYS_ISTTY | Check if fd is TTY |
| 0x0A | SYS_SEEK | Seek in file |
| 0x0C | SYS_FLEN | Get file length |
| 0x0D | SYS_TMPNAM | Generate temp filename |
| 0x0E | SYS_REMOVE | Delete file |
| 0x0F | SYS_RENAME | Rename file |
| 0x10 | SYS_CLOCK | Get centiseconds since start |
| 0x11 | SYS_TIME | Get seconds since epoch |
| 0x12 | SYS_SYSTEM | Execute shell command |
| 0x13 | SYS_ERRNO | Get last errno |
| 0x15 | SYS_GET_CMDLINE | Get command line |
| 0x16 | SYS_HEAPINFO | Get heap/stack info |
| 0x18 | SYS_EXIT | Exit program |
| 0x20 | SYS_EXIT_EXTENDED | Exit with extended info |
| 0x30 | SYS_ELAPSED | Get elapsed ticks |
| 0x31 | SYS_TICKFREQ | Get tick frequency |

## Design Principles

### Why RIFF?

The protocol marshals all data into RIFF chunks rather than passing pointers. This design is intentional for memory-mapped peripherals:

1. **Simpler hardware**: Device reads/writes a single contiguous buffer rather than chasing arbitrary pointers via DMA
2. **Security**: Device cannot access arbitrary guest memory locations
3. **Architecture independence**: No assumptions about pointer formats or memory layouts

### Why not trap-based semihosting?

Traditional ARM semihosting uses BKPT/SVC instructions which:

- Require debugger or supervisor support
- Are architecture-specific
- Don't work in all execution environments
- Complicate CPU models in emulators

Memory-mapped I/O works everywhere with standard load/store operations.

## License

BSD-3-Clause
