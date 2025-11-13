# RIFF-Based Semihosting Device Specification

**Version:** 1.0-draft
**Date:** 2025-11-12
**Status:** Experimental

---

## Table of Contents

1. [Overview](#overview)
2. [Design Principles](#design-principles)
3. [RIFF Format Primer](#riff-format-primer)
4. [Protocol Specification](#protocol-specification)
5. [Memory Layout](#memory-layout)
6. [ARM Semihosting Compatibility](#arm-semihosting-compatibility)
7. [Implementation Guide](#implementation-guide)
8. [Use Cases](#use-cases)
9. [Complete Examples](#complete-examples)
10. [Future Extensions](#future-extensions)
11. [Reference Tables](#reference-tables)

---

## Overview

### What Is This?

This specification describes a **memory-mapped semihosting interface** using the RIFF (Resource Interchange File Format) container format. It provides a platform-independent mechanism for embedded systems to perform I/O operations through a host debugger, emulator, or physical device during development.

### Why RIFF?

Traditional semihosting uses trap instructions (BKPT, SVC, EBREAK, etc.) which:
- Require debugger support
- Are architecture-specific
- Don't work in all execution environments

RIFF-based semihosting solves these problems by:
- Using **memory-mapped I/O** instead of traps
- Being **completely architecture-agnostic** (no bitness assumptions)
- **Self-describing** through explicit configuration chunks
- **Extensible** without breaking compatibility

### Key Features

- Works with any word size (8-bit, 16-bit, 24-bit, 32-bit, 64-bit, etc.)
- Compatible with ARM semihosting standard syscall numbers
- Works with existing picolibc common layer
- No trap instructions required
- Suitable for emulators, simulators, and physical hardware
- Minimal: Only 3 chunk types to start
- Extensible: Add features later without breaking existing code

---

## Design Principles

### KISS (Keep It Simple, Stupid)

The initial protocol defines only **three chunk types**:
1. `CNFG` - Configuration
2. `CALL` - Syscall request
3. `RETN` - Return value

This is sufficient for all existing ARM semihosting operations.

### YAGNI (You Aren't Gonna Need It)

Features like streaming, asynchronous operations, and host-initiated events are **not included** in the base specification. They can be added later as separate chunk types when actually needed.

### Extensibility

RIFF's chunk-based structure allows future extensions:
- Unknown chunks can be safely ignored
- New chunk types don't break old implementations
- Version negotiation happens through `CNFG` chunk

### Standards Compliance

This protocol uses the **ARM Semihosting standard** syscall numbers and argument conventions, ensuring compatibility with existing toolchains and libraries like picolibc.

---

## RIFF Format Primer

### Basic Structure

RIFF (Resource Interchange File Format) is a tagged file format used by WAV, AVI, and many other formats. It consists of:

```
'RIFF' [4 bytes]        - Magic signature
size   [4 bytes LE]     - Total size of data after this field
form   [4 bytes]        - Form type (we use 'SEMI')
chunks [size bytes]     - Series of chunks
```

### Chunk Structure

Each chunk has:

```
chunk_id    [4 bytes]       - Four-character code (e.g., 'CNFG')
chunk_size  [4 bytes LE]    - Size of chunk data (not including header)
chunk_data  [chunk_size]    - Chunk-specific data
padding     [0-1 bytes]     - Pad to even boundary if size is odd
```

---

## Protocol Specification

### Top-Level Structure

```
'RIFF' [size] 'SEMI'
  'CNFG' [config data]
  'CALL' [syscall data]
  'RETN' [return data]
```

The memory region contains a RIFF file with form type `'SEMI'` (semihosting).

---

### CNFG Chunk - Configuration

**Purpose:** Declares client architecture parameters. Must be the first chunk.

**Chunk ID:** `'CNFG'` (0x43 0x4E 0x46 0x47)

**Format:**

```
Offset  Size    Field           Description
------  ----    -----           -----------
+0x00   1       word_size       Bytes per word (1,2,4,8,16...)
+0x01   1       ptr_size        Bytes per pointer (may differ from word_size)
+0x02   1       endianness      0=Little Endian, 1=Big Endian, 2=PDP
+0x03   1       reserved        Must be 0x00
```

**Chunk Size:** Always 4 bytes

**Endianness Values:**
- `0` - Little Endian (LSB first)
- `1` - Big Endian (MSB first)
- `2` - PDP Endian (middle-endian, historic)
- `3-255` - Reserved for future use

**Notes:**
- Client writes this once per session
- Device stores these values and uses them for all subsequent operations
- `word_size` and `ptr_size` can differ (e.g., 16-bit words with 24-bit pointers)
- All multi-byte values in subsequent chunks use the declared endianness

**Example (32-bit little-endian system):**

```
'C' 'N' 'F' 'G'
0x04 0x00 0x00 0x00     // chunk_size = 4
0x04                    // word_size = 4 bytes
0x04                    // ptr_size = 4 bytes
0x00                    // little endian
0x00                    // reserved
```

---

### CALL Chunk - Syscall Request

**Purpose:** Request a semihosting operation using ARM standard syscall numbers.

**Chunk ID:** `'CALL'` (0x43 0x41 0x4C 0x4C)

**Format:**

```
Offset  Size            Field           Description
------  ----            -----           -----------
+0x00   1               opcode          ARM syscall number (SYS_*)
+0x01   3               reserved        Must be 0x00
+0x04   ptr_size        arg_ptr         Guest memory pointer to argument array
```

**Chunk Size:** `4 + ptr_size` bytes

**Opcode Values:**

Uses standard ARM semihosting syscall numbers (see [Reference Tables](#reference-tables)).

**Argument Pointer (arg_ptr):**

Points to a location in guest memory containing an array of arguments. The device reads this array according to:
- **Element size:** `word_size` bytes per argument
- **Endianness:** As declared in `CNFG`
- **Layout:** Sequential words, no padding
- **Interpretation:** According to ARM semihosting spec for that opcode

**Example (32-bit system calling SYS_WRITE):**

```
'C' 'A' 'L' 'L'
0x08 0x00 0x00 0x00     // chunk_size = 8 (4 + ptr_size)
0x05                    // opcode = SYS_WRITE
0x00 0x00 0x00          // reserved
0x00 0x10 0x00 0x00     // arg_ptr = 0x00001000 (LE)
```

---

### RETN Chunk - Return Value

**Purpose:** Device response containing syscall result and error status.

**Chunk ID:** `'RETN'` (0x52 0x45 0x54 0x4E)

**Format:**

```
Offset          Size            Field       Description
------          ----            -----       -----------
+0x00           word_size       result      Syscall return value
+word_size      4               errno       POSIX errno (0 = success)
```

**Chunk Size:** `word_size + 4` bytes

**Result Field:**

The return value from the syscall, interpreted according to the specific syscall:
- File descriptors (open)
- Byte counts (read/write)
- Status codes (close, unlink)
- Timestamps (time, gettimeofday)

**Errno Field:**

Always 4 bytes (32-bit), regardless of `word_size`. Contains:
- `0` - Success, no error
- `>0` - POSIX errno value (ENOENT=2, EACCES=13, etc.)

The errno values are **host** errno values, not target-specific. For architectures with different errno mappings, picolibc's existing translation layer handles conversion.

**Example (successful write of 10 bytes, 32-bit system):**

```
'R' 'E' 'T' 'N'
0x08 0x00 0x00 0x00     // chunk_size = 8 (word_size + 4)
0x0A 0x00 0x00 0x00     // result = 10 bytes written
0x00 0x00 0x00 0x00     // errno = 0 (success)
```

**Example (error case - file not found, 32-bit system):**

```
'R' 'E' 'T' 'N'
0x08 0x00 0x00 0x00     // chunk_size = 8
0xFF 0xFF 0xFF 0xFF     // result = -1 (error)
0x02 0x00 0x00 0x00     // errno = 2 (ENOENT)
```

---

## Memory Layout

### Single-Region Model (Simplest)

A single contiguous memory region at a device-specific address:

```
+0x0000: RIFF header + SEMI form type (12 bytes)
+0x000C: CNFG chunk (12 bytes)
+0x0018: CALL chunk (variable size)
+0x00xx: RETN chunk (variable size, written by device)
```

**Operation:**
1. Client writes RIFF header + CNFG + CALL
2. Device detects write (e.g., write to trigger address)
3. Device parses chunks, executes syscall
4. Device overwrites CALL with RETN in the same region
5. Client reads RETN chunk

### Dual-Region Model (Concurrent)

Two separate regions for request and response:

```
Request Region (Client writes):
+0x0000: RIFF header + CNFG + CALL

Response Region (Device writes):
+0x0000: RIFF header + RETN
```

This allows pipelining and asynchronous operations.

### Ring Buffer Model (Future)

For multiple concurrent operations, use a ring buffer of RIFF chunks with read/write pointers. Not part of base specification.

### Trigger Mechanism

The device needs to know when a request is ready. Options:

1. **Polling:** Device continuously polls request region for changes
2. **Last-byte trigger:** Write to last byte of CALL chunk triggers processing
3. **Separate trigger register:** Special address that client writes to after CALL
4. **Interrupt:** Client raises hardware interrupt after writing CALL

Implementation-dependent; not specified by protocol.

---

## ARM Semihosting Compatibility

### Syscall Numbers

This protocol uses the **ARM Semihosting specification** syscall numbers:

| Number | Name | Description |
|--------|------|-------------|
| 0x01 | SYS_OPEN | Open a file |
| 0x02 | SYS_CLOSE | Close a file |
| 0x03 | SYS_WRITEC | Write a character to debug channel |
| 0x04 | SYS_WRITE0 | Write null-terminated string |
| 0x05 | SYS_WRITE | Write to file |
| 0x06 | SYS_READ | Read from file |
| 0x07 | SYS_READC | Read a character |
| 0x08 | SYS_ISERROR | Check error status |
| 0x09 | SYS_ISTTY | Check if file is a TTY |
| 0x0A | SYS_SEEK | Seek in file |
| 0x0C | SYS_FLEN | Get file length |
| 0x0D | SYS_TMPNAM | Get temporary filename |
| 0x0E | SYS_REMOVE | Delete file |
| 0x0F | SYS_RENAME | Rename file |
| 0x10 | SYS_CLOCK | Get clock ticks |
| 0x11 | SYS_TIME | Get calendar time |
| 0x12 | SYS_SYSTEM | Execute host command |
| 0x13 | SYS_ERRNO | Get errno value |
| 0x15 | SYS_GET_CMDLINE | Get command line |
| 0x16 | SYS_HEAPINFO | Get heap information |
| 0x18 | SYS_EXIT | Exit application |
| 0x20 | SYS_EXIT_EXTENDED | Exit with extended code |
| 0x30 | SYS_ELAPSED | Get elapsed time |
| 0x31 | SYS_TICKFREQ | Get tick frequency |

### Argument Passing Convention

All arguments are passed **indirectly** through a pointer to an array in guest memory. This matches the ARM standard exactly.

For example, `SYS_WRITE` expects three arguments:
1. File descriptor (word)
2. Buffer pointer (word, actually points to buffer)
3. Length (word)

In memory at `arg_ptr`:
```
[arg_ptr + 0*word_size]: fd
[arg_ptr + 1*word_size]: buffer_ptr
[arg_ptr + 2*word_size]: length
```

The device reads these three words from guest memory, then reads `length` bytes from `buffer_ptr`.

### String Arguments

Strings are **always pointers**. For example, `SYS_OPEN`:
1. Filename pointer (word)
2. Mode (word)
3. Filename length (word)

The device reads the pointer, then reads the specified number of bytes from that address to get the filename string.

### Return Values

Single return value in the `result` field of `RETN` chunk. For syscalls that return multiple values (rare), see [Future Extensions](#future-extensions).

---

## Implementation Guide

### Client-Side (picolibc)

To add RIFF semihosting support to picolibc, create a new machine-specific backend:

**File:** `semihost/machine/riff/riff_semihost.h`

```c
#ifndef _RIFF_SEMIHOST_H_
#define _RIFF_SEMIHOST_H_

#include <stdint.h>

// Configuration - set per platform
#ifndef SEMIHOST_RIFF_BASE
#define SEMIHOST_RIFF_BASE 0xF0000000UL  // Example address
#endif

// RIFF chunk helpers
static inline void write_fourcc(volatile uint8_t *ptr, const char *fourcc) {
    ptr[0] = fourcc[0];
    ptr[1] = fourcc[1];
    ptr[2] = fourcc[2];
    ptr[3] = fourcc[3];
}

static inline void write_u32_le(volatile uint8_t *ptr, uint32_t val) {
    ptr[0] = (val >> 0) & 0xFF;
    ptr[1] = (val >> 8) & 0xFF;
    ptr[2] = (val >> 16) & 0xFF;
    ptr[3] = (val >> 24) & 0xFF;
}

// Initialize semihosting (call once)
static inline void riff_semihost_init(void) {
    volatile uint8_t *base = (volatile uint8_t *)SEMIHOST_RIFF_BASE;

    // Write RIFF header
    write_fourcc(base + 0, "RIFF");
    write_u32_le(base + 4, 0x100);  // Placeholder size
    write_fourcc(base + 8, "SEMI");

    // Write CNFG chunk
    write_fourcc(base + 12, "CNFG");
    write_u32_le(base + 16, 4);  // Chunk size
    base[20] = sizeof(uintptr_t);  // word_size
    base[21] = sizeof(uintptr_t);  // ptr_size
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    base[22] = 0;  // Little endian
#else
    base[22] = 1;  // Big endian
#endif
    base[23] = 0;  // Reserved
}

// Perform semihosting call
static inline uintptr_t sys_semihost(uintptr_t op, uintptr_t param) {
    volatile uint8_t *base = (volatile uint8_t *)SEMIHOST_RIFF_BASE;

    // Write CALL chunk at offset 24
    write_fourcc(base + 24, "CALL");
    write_u32_le(base + 28, 4 + sizeof(uintptr_t));
    base[32] = (uint8_t)op;
    base[33] = 0;
    base[34] = 0;
    base[35] = 0;

    // Write arg_ptr (architecture-specific size)
    uintptr_t *arg_ptr_loc = (uintptr_t *)(base + 36);
    *arg_ptr_loc = param;

    // Trigger device (implementation-specific)
    // Could be: write to trigger register, last byte, etc.
    volatile uint32_t *trigger = (volatile uint32_t *)(base + 0x1000);
    *trigger = 1;

    // Wait for RETN chunk (polling, could be interrupt-driven)
    while (base[24] == 'C' && base[25] == 'A' &&
           base[26] == 'L' && base[27] == 'L') {
        // Busy wait for device to replace CALL with RETN
    }

    // Read result from RETN chunk
    uintptr_t result = *(uintptr_t *)(base + 32);
    uint32_t errno_val;
    errno_val = base[32 + sizeof(uintptr_t)] |
                (base[33 + sizeof(uintptr_t)] << 8) |
                (base[34 + sizeof(uintptr_t)] << 16) |
                (base[35 + sizeof(uintptr_t)] << 24);

    if (errno_val != 0) {
        errno = errno_val;
    }

    return result;
}

#endif // _RIFF_SEMIHOST_H_
```

**Integration:**

The `sys_semihost()` function above provides the same interface as other architectures, so all the common semihosting code in `semihost/common/` works unchanged.

---

### Device-Side (Emulator/Hardware)

Implement a memory-mapped device that:

1. **Monitors the memory region** for writes
2. **Parses RIFF chunks** when triggered
3. **Extracts configuration** from CNFG chunk
4. **Reads guest memory** according to declared word size
5. **Executes syscall** using host OS facilities
6. **Writes result** back as RETN chunk

**Pseudo-code:**

```c
typedef struct {
    uint8_t word_size;
    uint8_t ptr_size;
    uint8_t endianness;
    uint8_t *memory_region;
    size_t region_size;
} RiffSemihostDevice;

void riff_semihost_trigger(RiffSemihostDevice *dev) {
    // Parse CNFG chunk (assume offset 12)
    uint8_t *cnfg = dev->memory_region + 12;
    if (memcmp(cnfg, "CNFG", 4) != 0) {
        return; // Error
    }

    dev->word_size = cnfg[8];
    dev->ptr_size = cnfg[9];
    dev->endianness = cnfg[10];

    // Parse CALL chunk (assume offset 24)
    uint8_t *call = dev->memory_region + 24;
    if (memcmp(call, "CALL", 4) != 0) {
        return; // Error
    }

    uint8_t opcode = call[8];
    uint64_t arg_ptr = read_ptr(call + 12, dev->ptr_size, dev->endianness);

    // Read arguments from guest memory
    uint64_t args[4] = {0};
    for (int i = 0; i < 4; i++) {
        args[i] = read_guest_word(dev, arg_ptr + i * dev->word_size);
    }

    // Execute syscall
    uint64_t result;
    uint32_t errno_val;
    execute_syscall(opcode, args, dev, &result, &errno_val);

    // Write RETN chunk (replacing CALL)
    write_fourcc(call, "RETN");
    write_u32_le(call + 4, dev->word_size + 4);
    write_word(call + 8, result, dev->word_size, dev->endianness);
    write_u32_le(call + 8 + dev->word_size, errno_val);
}

uint64_t read_guest_word(RiffSemihostDevice *dev, uint64_t addr) {
    // Read word_size bytes from guest memory at addr
    // Apply endianness conversion
    // Return as 64-bit value (handles all sizes)
}

void execute_syscall(uint8_t opcode, uint64_t *args,
                     RiffSemihostDevice *dev,
                     uint64_t *result, uint32_t *errno_val) {
    switch (opcode) {
        case 0x01: // SYS_OPEN
            *result = host_open(dev, args);
            break;
        case 0x05: // SYS_WRITE
            *result = host_write(dev, args);
            break;
        // ... other syscalls
    }
    *errno_val = errno;  // Capture host errno
}

uint64_t host_write(RiffSemihostDevice *dev, uint64_t *args) {
    int fd = (int)args[0];
    uint64_t buf_ptr = args[1];
    size_t count = (size_t)args[2];

    // Read buffer from guest memory
    uint8_t *buf = malloc(count);
    for (size_t i = 0; i < count; i++) {
        buf[i] = read_guest_byte(dev, buf_ptr + i);
    }

    // Perform host write
    ssize_t written = write(fd, buf, count);
    free(buf);

    return (uint64_t)written;
}
```

---

## Use Cases

### 1. Bringing Up New Emulator

When developing a CPU emulator (QEMU, custom simulator, etc.):

**Without RIFF semihosting:**
- Need to implement full UART/console drivers
- No file I/O during early bring-up
- Difficult to debug boot process
- Must implement trap instruction handling

**With RIFF semihosting:**
1. Map a memory region for RIFF interface
2. Implement simple memory write monitor
3. Parse RIFF chunks and proxy to host OS
4. Instant console output, file access, debugging
5. Works before any other I/O is implemented

**Example:** Bringing up a new 12-bit word size architecture in QEMU:
- Set `word_size=2` (12 bits packed in 16-bit words)
- Emulator reads arguments as 16-bit values
- All picolibc semihosting "just works"

### 2. Prototyping Physical Devices

When designing new embedded hardware:

**Scenario:** FPGA with custom CPU, no UART yet

**Implementation:**
1. Add block RAM mapped at known address
2. Implement simple state machine in FPGA fabric:
   - Monitor writes to RAM region
   - Parse RIFF chunks
   - Forward to host via JTAG/SPI/etc.
3. Host-side daemon processes syscalls
4. Return results via same path

**Benefits:**
- Test software before hardware peripherals exist
- Debug boot code without serial port
- Access host filesystem for testing
- No debugger required

### 3. Cross-Architecture Testing

**Scenario:** Testing picolibc on rare architectures

Without real hardware or mature emulator, use RIFF semihosting:
1. Implement minimal CPU emulator (ALU, memory, control flow)
2. Add RIFF memory region
3. Run picolibc test suite
4. All I/O goes through semihosting

### 4. Heterogeneous Systems

**Scenario:** Multi-core system with mixed word sizes

- 32-bit ARM host processor
- 16-bit DSP coprocessor
- Shared memory region

DSP can use RIFF semihosting to communicate with ARM, which provides I/O services. Each declares its own word size in CNFG.

### 5. Educational Platforms

Teaching computer architecture:
- Students implement toy CPUs in simulators
- RIFF semihosting provides I/O without complexity
- Can run real C programs (via picolibc) on student designs
- Focus on CPU design, not peripheral implementation

---

## Complete Examples

### Example 1: 16-bit System Writing to Console

**System:** 16-bit CPU, little-endian, writing "Hello\n" to stdout

**Memory Layout:**

```
Address  | Content                           | Description
---------|-----------------------------------|-------------
0x0000   | 'R' 'I' 'F' 'F'                  | RIFF signature
0x0004   | 0x2C 0x00 0x00 0x00               | Size = 44 bytes
0x0008   | 'S' 'E' 'M' 'I'                  | Form type
         |                                   |
0x000C   | 'C' 'N' 'F' 'G'                  | Config chunk
0x0010   | 0x04 0x00 0x00 0x00               | Chunk size = 4
0x0014   | 0x02                              | word_size = 2
0x0015   | 0x02                              | ptr_size = 2
0x0016   | 0x00                              | endianness = LE
0x0017   | 0x00                              | reserved
         |                                   |
0x0018   | 'C' 'A' 'L' 'L'                  | Call chunk
0x001C   | 0x06 0x00 0x00 0x00               | Chunk size = 6
0x0020   | 0x05                              | opcode = SYS_WRITE
0x0021   | 0x00 0x00 0x00                    | reserved
0x0024   | 0x00 0x10                         | arg_ptr = 0x1000 (LE)
         |                                   |
--- Guest Memory at 0x1000 (arguments) ---
0x1000   | 0x01 0x00                         | fd = 1 (stdout)
0x1002   | 0x00 0x20                         | buf_ptr = 0x2000
0x1004   | 0x06 0x00                         | count = 6
         |                                   |
--- Guest Memory at 0x2000 (buffer) ---
0x2000   | 'H' 'e' 'l' 'l' 'o' '\n'         | String data
```

**Device Processing:**

1. Reads CNFG: word_size=2, ptr_size=2, LE
2. Reads CALL: opcode=0x05 (SYS_WRITE), arg_ptr=0x1000
3. Reads args from 0x1000:
   - arg[0] = 0x0001 (fd=1)
   - arg[1] = 0x2000 (buffer pointer)
   - arg[2] = 0x0006 (count=6)
4. Reads 6 bytes from 0x2000: "Hello\n"
5. Calls host: `write(1, "Hello\n", 6)`
6. Result: 6 bytes written

**Device Response:**

```
Address  | Content                           | Description
---------|-----------------------------------|-------------
0x0018   | 'R' 'E' 'T' 'N'                  | Return chunk (replaces CALL)
0x001C   | 0x06 0x00 0x00 0x00               | Chunk size = 6 (word_size + 4)
0x0020   | 0x06 0x00                         | result = 6 (bytes written)
0x0022   | 0x00 0x00 0x00 0x00               | errno = 0 (success)
```

---

### Example 2: 64-bit System Opening File

**System:** 64-bit CPU, little-endian, opening "/tmp/test.txt" for reading

**CNFG Setup:**

```
'C' 'N' 'F' 'G'
0x04 0x00 0x00 0x00     // size = 4
0x08                    // word_size = 8
0x08                    // ptr_size = 8
0x00                    // LE
0x00                    // reserved
```

**CALL Chunk:**

```
'C' 'A' 'L' 'L'
0x0C 0x00 0x00 0x00                         // size = 12 (4 + ptr_size=8)
0x01                                        // opcode = SYS_OPEN
0x00 0x00 0x00                              // reserved
0x00 0x30 0x00 0x00 0x00 0x00 0x00 0x00    // arg_ptr = 0x3000
```

**Arguments at 0x3000:**

```
[0x3000] = 0x0000000000004000    // filename_ptr = 0x4000
[0x3008] = 0x0000000000000000    // mode = 0 (read-only, matches SH_OPEN_R)
[0x3010] = 0x000000000000000E    // length = 14 ("/tmp/test.txt")
```

**String at 0x4000:**

```
'/''t''m''p''/''t''e''s''t''.''t''x''t''\0'
```

**Device Response (success - fd=3):**

```
'R' 'E' 'T' 'N'
0x0C 0x00 0x00 0x00                         // size = 12 (word_size=8 + 4)
0x03 0x00 0x00 0x00 0x00 0x00 0x00 0x00    // result = 3 (file descriptor)
0x00 0x00 0x00 0x00                         // errno = 0
```

**Device Response (error - file not found):**

```
'R' 'E' 'T' 'N'
0x0C 0x00 0x00 0x00                         // size = 12
0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF    // result = -1
0x02 0x00 0x00 0x00                         // errno = 2 (ENOENT)
```

---

### Example 3: 8-bit System with 16-bit Pointers

**System:** 8-bit CPU (like 6502), 16-bit address space, little-endian

**CNFG:**

```
'C' 'N' 'F' 'G'
0x04 0x00 0x00 0x00
0x01                    // word_size = 1 byte
0x02                    // ptr_size = 2 bytes
0x00                    // LE
0x00
```

**CALL (SYS_WRITE, 3 bytes to stdout):**

```
'C' 'A' 'L' 'L'
0x06 0x00 0x00 0x00     // size = 6 (4 + ptr_size=2)
0x05
0x00 0x00 0x00
0x00 0x08               // arg_ptr = 0x0800
```

**Arguments at 0x0800:**

```
[0x0800] = 0x01         // fd = 1 (stdout)
[0x0801] = 0x00         // buffer_ptr low byte
[0x0802] = 0x09         // buffer_ptr high byte (ptr = 0x0900)
[0x0803] = 0x03         // count = 3
```

Note: Each "word" is 1 byte, but pointers are 2 bytes, so the pointer takes 2 consecutive array elements.

**Device Response:**

```
'R' 'E' 'T' 'N'
0x05 0x00 0x00 0x00     // size = 5 (word_size=1 + 4)
0x03                    // result = 3 (bytes written)
0x00 0x00 0x00 0x00     // errno = 0
```

---

## Future Extensions

### Backward Compatibility

New chunk types can be added without breaking old implementations:
- Old clients ignore unknown chunks (skip via chunk_size)
- Old devices ignore unknown chunks
- Version negotiation via extended CNFG fields

### Potential Future Chunks

#### STRM - Streaming Data

For large transfers (reading big files):

```
'S' 'T' 'R' 'M'
[chunk_size]
call_id         [4 bytes]
sequence        [4 bytes]
flags           [2 bytes]  // 0x01=more_data, 0x02=end
data            [variable]
```

#### EVNT - Host Events

For host-initiated interrupts:

```
'E' 'V' 'N' 'T'
[chunk_size]
event_type      [2 bytes]  // 0x01=signal, 0x02=timer, etc.
payload         [variable]
```

#### ABRT - Abort Operation

Cancel pending async call:

```
'A' 'B' 'R' 'T'
[chunk_size]
call_id         [4 bytes]
reason          [2 bytes]
```

#### META - Capabilities

Query device features:

```
'M' 'E' 'T' 'A'
[chunk_size]
query_type      [2 bytes]
data            [variable]
```

### Multi-Value Returns

Some syscalls return multiple values (e.g., 64-bit time on 32-bit system). Options:

1. **Current approach:** Pack into single result word (works for most cases)
2. **Extended RETN:** Add optional extra return values
3. **Separate chunk:** New chunk type for multi-value returns

### Asynchronous Operations

For non-blocking I/O:

1. Add `call_id` field to CALL chunk
2. Device returns immediately with pending status
3. Client polls or receives EVNT when ready
4. Multiple CALL chunks can be in flight

---

## Reference Tables

### ARM Semihosting Syscall Numbers

| Opcode | Name | Arguments | Return |
|--------|------|-----------|--------|
| 0x01 | SYS_OPEN | (filename*, mode, length) | fd or -1 |
| 0x02 | SYS_CLOSE | (fd) | 0 or -1 |
| 0x03 | SYS_WRITEC | (char*) | - |
| 0x04 | SYS_WRITE0 | (string*) | - |
| 0x05 | SYS_WRITE | (fd, buf*, length) | bytes written |
| 0x06 | SYS_READ | (fd, buf*, length) | bytes read |
| 0x07 | SYS_READC | () | char |
| 0x08 | SYS_ISERROR | (status) | error code |
| 0x09 | SYS_ISTTY | (fd) | 1 if TTY, 0 if not |
| 0x0A | SYS_SEEK | (fd, pos) | 0 or -1 |
| 0x0C | SYS_FLEN | (fd) | length or -1 |
| 0x0D | SYS_TMPNAM | (buf*, id, length) | 0 or -1 |
| 0x0E | SYS_REMOVE | (filename*, length) | 0 or -1 |
| 0x0F | SYS_RENAME | (old*, old_len, new*, new_len) | 0 or -1 |
| 0x10 | SYS_CLOCK | () | centiseconds |
| 0x11 | SYS_TIME | () | seconds since epoch |
| 0x12 | SYS_SYSTEM | (cmd*, length) | exit code |
| 0x13 | SYS_ERRNO | () | errno value |
| 0x15 | SYS_GET_CMDLINE | (buf*, length) | 0 or -1 |
| 0x16 | SYS_HEAPINFO | (block*) | - |
| 0x18 | SYS_EXIT | (status) | no return |
| 0x20 | SYS_EXIT_EXTENDED | (exception, subcode) | no return |
| 0x30 | SYS_ELAPSED | () | 64-bit ticks |
| 0x31 | SYS_TICKFREQ | () | ticks per second |

### Chunk Type IDs

| FourCC | Name | Direction | Description |
|--------|------|-----------|-------------|
| CNFG | Configuration | Client→Device | Architecture parameters |
| CALL | Call | Client→Device | Syscall request |
| RETN | Return | Device→Client | Syscall result |
| STRM | Stream | Bidirectional | Streaming data (future) |
| EVNT | Event | Device→Client | Async events (future) |
| ABRT | Abort | Bidirectional | Cancel operation (future) |
| META | Metadata | Bidirectional | Capabilities (future) |

### Endianness Values

| Value | Name | Description |
|-------|------|-------------|
| 0x00 | Little Endian | LSB first (x86, ARM Cortex-M) |
| 0x01 | Big Endian | MSB first (SPARC, 68k, PPC) |
| 0x02 | PDP Endian | Middle-endian (PDP-11) |

### Open Mode Flags (SYS_OPEN)

These match the ARM semihosting specification:

| Value | Name | Description |
|-------|------|-------------|
| 0 | SH_OPEN_R | Read-only |
| 1 | SH_OPEN_RB | Read-only binary |
| 2 | SH_OPEN_R_PLUS | Read-write |
| 3 | SH_OPEN_R_PLUS_B | Read-write binary |
| 4 | SH_OPEN_W | Write, truncate/create |
| 5 | SH_OPEN_WB | Write binary, truncate/create |
| 6 | SH_OPEN_W_PLUS | Read-write, truncate/create |
| 7 | SH_OPEN_W_PLUS_B | Read-write binary, truncate/create |
| 8 | SH_OPEN_A | Append, create if needed |
| 9 | SH_OPEN_AB | Append binary, create if needed |
| 10 | SH_OPEN_A_PLUS | Read-append, create if needed |
| 11 | SH_OPEN_A_PLUS_B | Read-append binary, create if needed |

---

## Appendix: RIFF Parsing Example

Simple C code to parse RIFF chunks:

```c
typedef struct {
    char id[4];
    uint32_t size;
    uint8_t *data;
} RiffChunk;

int parse_riff(uint8_t *buffer, size_t buf_size,
               RiffChunk *chunks, int max_chunks) {
    if (buf_size < 12) return -1;
    if (memcmp(buffer, "RIFF", 4) != 0) return -1;

    uint32_t riff_size = read_u32_le(buffer + 4);
    if (memcmp(buffer + 8, "SEMI", 4) != 0) return -1;

    int chunk_count = 0;
    size_t offset = 12;

    while (offset + 8 <= buf_size && chunk_count < max_chunks) {
        RiffChunk *chunk = &chunks[chunk_count++];
        memcpy(chunk->id, buffer + offset, 4);
        chunk->size = read_u32_le(buffer + offset + 4);
        chunk->data = buffer + offset + 8;

        offset += 8 + chunk->size;
        if (chunk->size & 1) offset++;  // Pad to even
    }

    return chunk_count;
}

uint32_t read_u32_le(uint8_t *ptr) {
    return ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
}
```
