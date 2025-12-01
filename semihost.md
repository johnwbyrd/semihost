# RIFF-Based Semihosting Device Specification

**Version:** 2.0-draft
**Date:** 2025-11-26
**Status:** Experimental

---

## Table of Contents

1. [Overview](#overview)
2. [Hardware Architecture](#hardware-architecture)
3. [RIFF Protocol](#riff-protocol)
4. [Operation Modes](#operation-modes)
5. [Guest Software Integration](#guest-software-integration)
6. [Host Implementation](#host-implementation)
7. [Complete Examples](#complete-examples)
8. [ARM Semihosting Compatibility](#arm-semihosting-compatibility)
9. [Future Extensions](#future-extensions)
10. [Reference Tables](#reference-tables)

---

## Overview

### What Is This?

This specification describes a **memory-mapped semihosting peripheral** - a hardware device (physical or virtual) that provides I/O services to embedded systems during development. The device uses RIFF (Resource Interchange File Format) as its communication protocol, enabling completely architecture-agnostic operation across any CPU from 8-bit to 128-bit.

### Physical Device Concept

Think of this as a discrete chip on the system bus, similar to:
- **UART (16550)** - provides serial I/O
- **RTC (DS1307)** - provides timekeeping
- **Semihost Device** - provides file I/O, console, and host services

The device occupies a small memory-mapped register space (32 bytes) and communicates with the CPU through standard memory read/write operations.

### Virtual Implementation

The same device can be implemented virtually in:
- **Emulators** (QEMU, MAME, custom simulators)
- **Debuggers** (GDB, OpenOCD)
- **Hardware Simulators** (Verilator, ModelSim)

The specification is identical whether the device is silicon or software.

### Why RIFF?

Traditional semihosting uses trap instructions (BKPT, SVC, EBREAK, etc.) which:
- Require debugger support
- Are architecture-specific
- Don't work in all execution environments
- Complicate the CPU execution model

RIFF-based semihosting solves these problems by:
- Using **memory-mapped I/O** (standard load/store operations)
- Being **completely architecture-agnostic** (works on any CPU)
- Being **self-describing** through explicit configuration
- Being **extensible** without breaking compatibility

### Key Features

- **Universal**: Works with any word size (8-bit through 128-bit and beyond)
- **Standard**: Compatible with ARM semihosting syscall numbers
- **Simple**: Only 32 bytes of device registers
- **Efficient**: Guest manages all buffers in its own RAM
- **Flexible**: Synchronous (polling) or asynchronous (interrupt) operation
- **Robust**: Self-describing protocol with explicit endianness negotiation

---

## Hardware Architecture

### Device Register Map

The semihosting device presents 32 bytes of memory-mapped registers:

```
Offset  Size  Name         Access  Description
------  ----  -----------  ------  -------------------------------------
0x00    16    RIFF_PTR     RW      Pointer to RIFF buffer in guest RAM
0x10    1     DOORBELL     W       Write any value to trigger request
0x11    1     IRQ_STATUS   R       Interrupt status flags
0x12    1     IRQ_ENABLE   RW      Interrupt enable mask
0x13    1     IRQ_ACK      W       Write 1s to clear interrupt bits
0x14    1     STATUS       R       Device status flags
0x15    11    RESERVED     -       Reserved for future use
```

**Total device footprint: 32 bytes**

### Register Descriptions

#### RIFF_PTR (0x00-0x0F, 16 bytes, Read/Write)

**Purpose:** Holds the guest memory address of the RIFF communication buffer.

**Format:** Raw byte storage. Guest writes pointer in its native byte order using as many bytes as needed (2, 4, 8, or 16 bytes). Unused high bytes are ignored.

**Host behavior:** Host knows the guest CPU's address width and endianness from emulator/system configuration. Reads the appropriate number of bytes and interprets them correctly without guessing.

**Examples:**
- 16-bit CPU (6502): Writes 2 bytes in LE order
- 32-bit CPU (68000): Writes 4 bytes in BE order
- 64-bit CPU (x86-64): Writes 8 bytes in LE order
- 128-bit future CPU: Writes 16 bytes in native order

#### DOORBELL (0x10, 1 byte, Write-Only)

**Purpose:** Trigger register. Writing any value initiates request processing.

**Operation:**
1. Guest builds RIFF structure in its RAM
2. Guest writes pointer to RIFF_PTR
3. Guest writes any value (typically 0x01) to DOORBELL
4. Device detects write and begins processing

**Read behavior:** Reading returns undefined value (typically 0x00).

#### IRQ_STATUS (0x11, 1 byte, Read-Only)

**Purpose:** Indicates which interrupt conditions are currently active.

**Bit definitions:**
```
Bit 0: RESPONSE_READY - Semihosting request completed
Bit 1: ERROR          - Error occurred during processing
Bits 2-7: Reserved (read as 0)
```

**Operation:** Host sets bits when events occur. Guest reads to determine interrupt cause. Bits are cleared by writing to IRQ_ACK.

#### IRQ_ENABLE (0x12, 1 byte, Read/Write)

**Purpose:** Controls which interrupt conditions can assert the CPU's interrupt line.

**Bit definitions:**
```
Bit 0: RESPONSE_READY_EN - Enable interrupt on completion
Bit 1: ERROR_EN          - Enable interrupt on error
Bits 2-7: Reserved (write 0, read as 0)
```

**Default:** 0x00 (all interrupts disabled, polling mode)

**Operation:** When an interrupt condition occurs AND its corresponding enable bit is set, the device asserts the CPU's interrupt request line.

#### IRQ_ACK (0x13, 1 byte, Write-Only)

**Purpose:** Acknowledge and clear interrupt status bits.

**Operation:** Write 1s to clear corresponding bits in IRQ_STATUS. Writing 0s has no effect.

**Example:** Writing 0x01 clears RESPONSE_READY status bit and deasserts interrupt if no other enabled conditions are active.

**Read behavior:** Reading returns undefined value.

#### STATUS (0x14, 1 byte, Read-Only)

**Purpose:** General device status flags.

**Bit definitions:**
```
Bit 0: RESPONSE_READY - Same as IRQ_STATUS bit 0 (convenience)
Bit 7: DEVICE_PRESENT - Always 1 (device exists and is functional)
Bits 1-6: Reserved (read as 0)
```

**Operation:** Guest can poll STATUS register to wait for completion without enabling interrupts. Bit 0 provides same information as IRQ_STATUS for convenience.

### Memory Organization

**Key Principle:** The device registers and the RIFF buffer are **separate**.

```
Device Registers (32 bytes at device base address):
  - RIFF_PTR, DOORBELL, IRQ_STATUS, etc.
  - Fixed location in memory map
  - Directly accessed by CPU

RIFF Buffer (variable size in guest RAM):
  - Guest allocates wherever convenient
  - Contains RIFF header + chunks
  - Guest tells device location via RIFF_PTR
  - Device reads/writes this buffer via memory access
```

**Why separate?**
1. Device footprint stays minimal (32 bytes)
2. Guest has complete control over buffer allocation
3. Works on CPUs with tiny RAM (can use stack/data segment)
4. Scales to CPUs with huge RAM (can use large buffers)

### Physical Integration

#### Bus Interface

The device appears as a memory-mapped peripheral:

**Address Decoding:**
- Device responds to 32-byte aligned address range
- Typically placed in high memory (e.g., 0xFFFF0000 on 32-bit systems)
- Can be anywhere in address space based on system design

**Bus Signals (typical):**
- Address bus (CPU-dependent width)
- Data bus (CPU-dependent width, typically 8/16/32 bits)
- Read enable
- Write enable
- Chip select (from address decoder)

**Data Width Handling:**

The device accepts byte, word, or dword accesses:
- **Byte access (8-bit):** Natural for all registers
- **Word access (16-bit):** Aligned 16-bit reads/writes (e.g., RIFF_PTR)
- **Dword access (32-bit):** Aligned 32-bit reads/writes (e.g., RIFF_PTR)
- **Qword access (64-bit):** Aligned 64-bit reads/writes (e.g., RIFF_PTR)

Unaligned accesses may or may not be supported based on bus protocol.

#### Interrupt Output

**Signal:** IRQ_OUT (active high or low, implementation choice)

**Behavior:**
```
IRQ_OUT = (IRQ_STATUS & IRQ_ENABLE) != 0
```

When any enabled interrupt condition is active, IRQ_OUT is asserted. Connect to CPU's IRQ or NMI input as appropriate for the system.

**Typical connection:**
- Simple systems: Connect to CPU IRQ input
- Priority systems: Connect through interrupt controller
- Polling-only systems: Leave unconnected

### Timing Characteristics

#### Synchronous Operation (Polling Mode)

The device operates **synchronously** with deterministic timing:

```
1. Guest writes to DOORBELL (1 bus cycle)
2. Device detects write (combinational or 1 clock)
3. Device processes request (variable time, typically µs to ms)
4. Device writes RETN chunk to guest RAM (multiple bus cycles)
5. Device sets STATUS bit 0 (1 clock)
6. Guest reads STATUS to detect completion (1 bus cycle per poll)
```

**Processing time:** Depends on syscall type:
- `SYS_WRITEC`: Microseconds (write single character)
- `SYS_WRITE`: Milliseconds (write large buffer to disk)
- `SYS_OPEN`: Milliseconds (filesystem access)

**Guest blocking:** Guest typically polls STATUS in a tight loop, consuming CPU cycles during I/O.

#### Asynchronous Operation (Interrupt Mode)

For interrupt-driven operation:

```
1. Guest writes IRQ_ENABLE = 0x01 (enable RESPONSE_READY interrupt)
2. Guest writes to DOORBELL
3. Guest continues other work (or enters low-power mode)
4. Device processes request in background
5. Device sets IRQ_STATUS bit 0 and asserts IRQ_OUT
6. CPU takes interrupt, guest handler runs
7. Handler reads result from RIFF buffer
8. Handler writes IRQ_ACK = 0x01 to clear interrupt
```

**Latency:** Interrupt assertion to handler execution depends on CPU architecture and system state (interrupt masks, current instruction, etc.).

---

## RIFF Protocol

### Buffer Location and Ownership

The RIFF communication buffer is:
- **Allocated by guest** in its own RAM (stack, heap, static data)
- **Owned by guest** (device only accesses on request)
- **Pointed to** via RIFF_PTR register
- **Variable size** (guest allocates sufficient space)

**Recommended minimum size:** 256 bytes (handles most syscalls)
**Typical size:** 1024 bytes (comfortable for all operations)

### RIFF Structure Overview

RIFF (Resource Interchange File Format) is a tagged container format. The same format used by WAV, AVI, and many other standards.

**Basic structure:**
```
'RIFF' [size:4] 'SEMI'
  'CNFG' [chunk_size:4] [config_data]
  'CALL' [chunk_size:4] [syscall_data]
  'RETN' [chunk_size:4] [return_data]    // Device writes this
```

**RIFF compliance:**
- Chunk IDs: 4-byte ASCII codes ('RIFF', 'CNFG', etc.)
- Sizes: 32-bit **little-endian** (RIFF standard requirement)
- Padding: Chunks padded to even byte boundary if odd size
- Form type: 'SEMI' (semihosting)

### Endianness Handling

**Critical distinction:**

1. **RIFF structure** (headers, chunk IDs, sizes): **Always little-endian** per RIFF specification
2. **Data values** (syscall arguments, pointers, return values): **Guest's native endianness** as declared in CNFG chunk

**Why this works:**
- RIFF is a Microsoft/Intel creation (little-endian heritage)
- Keeping RIFF headers LE ensures compatibility and simplicity
- Guest declares its endianness in CNFG chunk
- Host interprets data values using declared endianness

**Implementation note for big-endian guests:**

Guest must swap multi-byte values in RIFF headers (sizes) but NOT in chunk data:

```c
// Big-endian CPU example
uint32_t size = 8;
write_u32_le(&riff_buffer[4], size);  // Swap for RIFF header

uint32_t arg0 = 0x12345678;
write_u32_be(&call_args[0], arg0);    // Native BE for chunk data
```

Helper functions for BE platforms:

```c
static inline void write_u32_le(void *ptr, uint32_t val) {
    uint8_t *p = (uint8_t *)ptr;
    p[0] = (val >> 0) & 0xFF;
    p[1] = (val >> 8) & 0xFF;
    p[2] = (val >> 16) & 0xFF;
    p[3] = (val >> 24) & 0xFF;
}

static inline void write_u32_be(void *ptr, uint32_t val) {
    uint8_t *p = (uint8_t *)ptr;
    p[0] = (val >> 24) & 0xFF;
    p[1] = (val >> 16) & 0xFF;
    p[2] = (val >> 8) & 0xFF;
    p[3] = (val >> 0) & 0xFF;
}
```

For little-endian platforms, both functions do the same thing, so the overhead is zero.

### CNFG Chunk - Configuration

**Purpose:** Declares guest CPU architecture parameters. Must be first chunk after RIFF header.

**Chunk ID:** `'CNFG'` (0x43 0x4E 0x46 0x47)

**Format:**
```
Offset  Size  Field        Description
------  ----  -----------  ------------------------------------------
+0x00   4     chunk_id     'CNFG'
+0x04   4     chunk_size   4 (little-endian)
+0x08   1     word_size    Bytes per word (1,2,4,8,16,...)
+0x09   1     ptr_size     Bytes per pointer (may differ from word)
+0x0A   1     endianness   0=LE, 1=BE, 2=PDP
+0x0B   1     reserved     Must be 0x00
```

**Total chunk size:** 12 bytes (8 byte header + 4 byte data)

**Endianness values:**
- `0` - Little Endian (LSB first): x86, ARM Cortex-M, RISC-V (typically)
- `1` - Big Endian (MSB first): 68000, SPARC, PowerPC (classic mode), MIPS (BE mode)
- `2` - PDP Endian (middle-endian): PDP-11, VAX (historic)
- `3-255` - Reserved for future use

**Notes:**
- Written once per session (or per RIFF buffer initialization)
- Device caches these values for interpreting subsequent chunks
- `word_size` != `ptr_size` is valid (e.g., 16-bit words with 24-bit pointers on some DSPs)
- Host uses these values to correctly interpret all multi-byte values in guest data

**Example (32-bit little-endian ARM):**
```
0x43 0x4E 0x46 0x47         // 'CNFG'
0x04 0x00 0x00 0x00         // chunk_size = 4 (LE)
0x04                        // word_size = 4 bytes
0x04                        // ptr_size = 4 bytes
0x00                        // endianness = 0 (LE)
0x00                        // reserved
```

**Example (16-bit big-endian 68000):**
```
0x43 0x4E 0x46 0x47         // 'CNFG'
0x04 0x00 0x00 0x00         // chunk_size = 4 (LE - note RIFF requirement!)
0x02                        // word_size = 2 bytes
0x04                        // ptr_size = 4 bytes (68000 has 32-bit pointers)
0x01                        // endianness = 1 (BE)
0x00                        // reserved
```

### CALL Chunk - Syscall Request

**Purpose:** Request a semihosting operation.

**Chunk ID:** `'CALL'` (0x43 0x41 0x4C 0x4C)

**Format:**
```
Offset  Size      Field      Description
------  --------  ---------  ------------------------------------------
+0x00   4         chunk_id   'CALL'
+0x04   4         size       4 + ptr_size (little-endian)
+0x08   1         opcode     ARM semihosting syscall number
+0x09   3         reserved   Must be 0x00
+0x0C   ptr_size  arg_ptr    Pointer to argument array in guest RAM
```

**Total chunk size:** 12 + ptr_size bytes

**Opcode:** ARM semihosting syscall number (0x01-0x31, see reference table)

**Argument pointer (arg_ptr):**
- Points to array in guest RAM
- Array contains `word_size` byte elements
- Element count depends on syscall (e.g., SYS_WRITE has 3 arguments)
- Written in guest's native endianness (as declared in CNFG)

**Example (32-bit LE system, SYS_WRITE syscall):**
```
0x43 0x41 0x4C 0x4C         // 'CALL'
0x08 0x00 0x00 0x00         // chunk_size = 8 (4 + ptr_size=4) (LE)
0x05                        // opcode = 0x05 (SYS_WRITE)
0x00 0x00 0x00              // reserved
0x00 0x10 0x00 0x00         // arg_ptr = 0x00001000 (LE)
```

At address 0x1000 in guest RAM (3 arguments × 4 bytes):
```
0x00001000: 0x01 0x00 0x00 0x00    // arg[0] = fd = 1 (stdout)
0x00001004: 0x00 0x20 0x00 0x00    // arg[1] = buffer_ptr = 0x2000
0x00001008: 0x0A 0x00 0x00 0x00    // arg[2] = count = 10 bytes
```

### RETN Chunk - Return Value

**Purpose:** Device response containing syscall result.

**Chunk ID:** `'RETN'` (0x52 0x45 0x54 0x4E)

**Format:**
```
Offset      Size       Field   Description
----------  ---------  ------  ------------------------------------------
+0x00       4          chunk_id   'RETN'
+0x04       4          size       word_size + 4 (little-endian)
+0x08       word_size  result     Syscall return value (guest endianness)
+0x08+word  4          errno      POSIX errno, 0=success (little-endian)
```

**Total chunk size:** 12 + word_size bytes

**Result field:**
- Size equals `word_size` from CNFG
- Endianness equals guest's declared endianness
- Interpretation depends on syscall:
  - File descriptor (SYS_OPEN)
  - Byte count (SYS_READ/SYS_WRITE)
  - Status code (SYS_CLOSE)
  - -1 typically indicates error

**Errno field:**
- Always 4 bytes (32-bit)
- Always little-endian (for consistency with RIFF)
- 0 = success, no error
- >0 = POSIX errno value (ENOENT=2, EACCES=13, etc.)

**Device behavior:** Device **overwrites** the CALL chunk with RETN chunk in the same buffer location.

**Example (successful write of 10 bytes, 32-bit LE):**
```
0x52 0x45 0x54 0x4E         // 'RETN' (replaced 'CALL')
0x08 0x00 0x00 0x00         // chunk_size = 8 (LE)
0x0A 0x00 0x00 0x00         // result = 10 bytes written (LE)
0x00 0x00 0x00 0x00         // errno = 0 (success) (LE)
```

**Example (error: file not found, 32-bit LE):**
```
0x52 0x45 0x54 0x4E         // 'RETN'
0x08 0x00 0x00 0x00         // chunk_size = 8 (LE)
0xFF 0xFF 0xFF 0xFF         // result = -1 (error) (LE)
0x02 0x00 0x00 0x00         // errno = 2 (ENOENT) (LE)
```

---

## Operation Modes

### Synchronous Mode (Polling)

**Default mode.** Guest blocks waiting for response.

**Guest code pattern:**
```c
// Setup (once at startup)
volatile uint8_t *semihost = (volatile uint8_t *)SEMIHOST_BASE;
uint8_t riff_buffer[256];

// Build RIFF request
build_riff_request(riff_buffer, SYS_WRITE, args);

// Write pointer to device (native byte order)
*(uintptr_t *)(semihost + RIFF_PTR_OFFSET) = (uintptr_t)riff_buffer;

// Ring doorbell
*(uint8_t *)(semihost + DOORBELL_OFFSET) = 0x01;

// Poll for completion
while (!(*(uint8_t *)(semihost + STATUS_OFFSET) & 0x01)) {
    // Busy wait
}

// Read result from riff_buffer
uintptr_t result = read_retn_result(riff_buffer);
```

**Characteristics:**
- Simple to implement
- No interrupt handler needed
- Guest wastes CPU cycles during I/O
- Suitable for simple single-tasking programs

**Typical use:** Educational platforms, simple test programs, early firmware bring-up.

### Asynchronous Mode (Interrupt-Driven)

**Optional mode.** Guest can perform other work while waiting.

**Guest code pattern:**
```c
// Setup (once at startup)
void semihost_init(void) {
    volatile uint8_t *semihost = (volatile uint8_t *)SEMIHOST_BASE;

    // Enable RESPONSE_READY interrupt
    *(uint8_t *)(semihost + IRQ_ENABLE_OFFSET) = 0x01;

    // Enable CPU interrupt handling
    enable_irq();
}

// IRQ handler
void irq_handler(void) {
    volatile uint8_t *semihost = (volatile uint8_t *)SEMIHOST_BASE;

    // Check if semihost interrupt
    if (*(uint8_t *)(semihost + IRQ_STATUS_OFFSET) & 0x01) {
        // Read result from riff_buffer
        uintptr_t result = read_retn_result(riff_buffer);

        // Signal completion to application
        semihost_request_complete(result);

        // Acknowledge interrupt
        *(uint8_t *)(semihost + IRQ_ACK_OFFSET) = 0x01;
    }
}

// Application code
void do_semihost_write(void) {
    // Build request
    build_riff_request(riff_buffer, SYS_WRITE, args);

    // Submit (returns immediately)
    *(uintptr_t *)(semihost + RIFF_PTR_OFFSET) = (uintptr_t)riff_buffer;
    *(uint8_t *)(semihost + DOORBELL_OFFSET) = 0x01;

    // Do other work here!
    perform_other_tasks();

    // Or wait for completion
    wait_for_semihost_completion();
}
```

**Characteristics:**
- More complex to implement (requires IRQ handler)
- Guest can multitask during I/O
- Enables true asynchronous operation
- Suitable for operating systems and complex applications

**Typical use:** OS development, preemptive multitasking, power-sensitive applications.

### Guest Memory Access by Device

**Critical detail:** The device accesses guest RAM to read/write RIFF buffers.

**Physical hardware:** Device is a bus master, can initiate DMA reads/writes to guest memory.

**Virtual hardware:** Emulator directly accesses guest memory structures (memory array, MMU translations, etc.).

**Implications:**
1. **Cache coherency:** If guest CPU has data cache, may need to flush before DOORBELL write
2. **MMU transparency:** Device sees physical addresses; if guest uses virtual addresses in RIFF_PTR, MMU must translate or guest must provide physical addresses
3. **Access permissions:** Device ignores memory protection (privileged access)

**Example (physical device with DMA):**
```
1. Guest writes RIFF_PTR = 0x00001000
2. Guest writes DOORBELL = 0x01
3. Device bus master logic:
   a. Reads 12 bytes from guest RAM at 0x00000000 (RIFF header)
   b. Reads 12 bytes from guest RAM at 0x0000000C (CNFG chunk)
   c. Reads 12+ bytes from guest RAM at 0x00000018 (CALL chunk)
   d. Extracts arg_ptr, reads arguments from that address
   e. Executes syscall
   f. Writes 12+ bytes to guest RAM at 0x00000018 (RETN chunk)
4. Device sets STATUS bit 0
```

---

## Guest Software Integration

### Helper Library (C)

**File:** `semihost_riff.h`

```c
#ifndef SEMIHOST_RIFF_H
#define SEMIHOST_RIFF_H

#include <stdint.h>

// Device base address - platform specific
#ifndef SEMIHOST_BASE
#define SEMIHOST_BASE 0xFFFF0000UL
#endif

// Register offsets
#define RIFF_PTR_OFFSET   0x00
#define DOORBELL_OFFSET   0x10
#define IRQ_STATUS_OFFSET 0x11
#define IRQ_ENABLE_OFFSET 0x12
#define IRQ_ACK_OFFSET    0x13
#define STATUS_OFFSET     0x14

// Status/IRQ bits
#define STATUS_RESPONSE_READY 0x01
#define STATUS_ERROR          0x02
#define STATUS_DEVICE_PRESENT 0x80

// ARM syscall numbers
#define SYS_OPEN       0x01
#define SYS_CLOSE      0x02
#define SYS_WRITEC     0x03
#define SYS_WRITE0     0x04
#define SYS_WRITE      0x05
#define SYS_READ       0x06
#define SYS_READC      0x07
#define SYS_ISERROR    0x08
#define SYS_ISTTY      0x09
#define SYS_SEEK       0x0A
#define SYS_FLEN       0x0C
#define SYS_TMPNAM     0x0D
#define SYS_REMOVE     0x0E
#define SYS_RENAME     0x0F
#define SYS_CLOCK      0x10
#define SYS_TIME       0x11
#define SYS_SYSTEM     0x12
#define SYS_ERRNO      0x13
#define SYS_GET_CMDLINE 0x15
#define SYS_HEAPINFO   0x16
#define SYS_EXIT       0x18
#define SYS_EXIT_EXTENDED 0x20
#define SYS_ELAPSED    0x30
#define SYS_TICKFREQ   0x31

// Helper: Write 32-bit little-endian
static inline void write_u32_le(void *ptr, uint32_t val) {
    uint8_t *p = (uint8_t *)ptr;
    p[0] = (val >> 0) & 0xFF;
    p[1] = (val >> 8) & 0xFF;
    p[2] = (val >> 16) & 0xFF;
    p[3] = (val >> 24) & 0xFF;
}

// Helper: Read 32-bit little-endian
static inline uint32_t read_u32_le(const void *ptr) {
    const uint8_t *p = (const uint8_t *)ptr;
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

// Initialize semihosting device
static inline void semihost_init(void) {
    volatile uint8_t *dev = (volatile uint8_t *)SEMIHOST_BASE;

    // Check device present
    if (!(dev[STATUS_OFFSET] & STATUS_DEVICE_PRESENT)) {
        return; // Device not available
    }

    // Interrupts disabled by default (polling mode)
    dev[IRQ_ENABLE_OFFSET] = 0x00;
}

// Build RIFF header + CNFG in buffer
static inline void semihost_init_buffer(void *buffer, size_t buf_size) {
    uint8_t *buf = (uint8_t *)buffer;

    // RIFF header
    buf[0] = 'R'; buf[1] = 'I'; buf[2] = 'F'; buf[3] = 'F';
    write_u32_le(buf + 4, buf_size - 8);  // Size (LE)
    buf[8] = 'S'; buf[9] = 'E'; buf[10] = 'M'; buf[11] = 'I';

    // CNFG chunk
    buf[12] = 'C'; buf[13] = 'N'; buf[14] = 'F'; buf[15] = 'G';
    write_u32_le(buf + 16, 4);  // Chunk size (LE)
    buf[20] = sizeof(uintptr_t);  // word_size
    buf[21] = sizeof(uintptr_t);  // ptr_size
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    buf[22] = 0;  // Little endian
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    buf[22] = 1;  // Big endian
#else
    buf[22] = 2;  // PDP endian
#endif
    buf[23] = 0;  // Reserved
}

// Perform semihosting call (blocking)
static inline uintptr_t semihost_call(uint8_t opcode, void *args) {
    static uint8_t riff_buffer[256];
    static int initialized = 0;

    volatile uint8_t *dev = (volatile uint8_t *)SEMIHOST_BASE;
    uint8_t *buf = riff_buffer;

    // Initialize buffer on first call
    if (!initialized) {
        semihost_init_buffer(buf, sizeof(riff_buffer));
        initialized = 1;
    }

    // Write CALL chunk at offset 24
    buf[24] = 'C'; buf[25] = 'A'; buf[26] = 'L'; buf[27] = 'L';
    write_u32_le(buf + 28, 4 + sizeof(uintptr_t));  // Chunk size (LE)
    buf[32] = opcode;
    buf[33] = buf[34] = buf[35] = 0;  // Reserved

    // Write arg_ptr (native byte order)
    *(uintptr_t *)(buf + 36) = (uintptr_t)args;

    // Write pointer to device (native byte order)
    *(uintptr_t *)(dev + RIFF_PTR_OFFSET) = (uintptr_t)buf;

    // Ring doorbell
    dev[DOORBELL_OFFSET] = 0x01;

    // Poll for completion
    while (!(dev[STATUS_OFFSET] & STATUS_RESPONSE_READY)) {
        // Busy wait
    }

    // Read result (RETN chunk at offset 24, replaces CALL)
    uintptr_t result = *(uintptr_t *)(buf + 32);
    uint32_t err = read_u32_le(buf + 32 + sizeof(uintptr_t));

    // Set errno if error occurred
    extern int errno;
    if (err != 0) {
        errno = err;
    }

    return result;
}

// High-level wrappers
static inline ssize_t semihost_write(int fd, const void *buf, size_t count) {
    uintptr_t args[3];
    args[0] = fd;
    args[1] = (uintptr_t)buf;
    args[2] = count;
    return (ssize_t)semihost_call(SYS_WRITE, args);
}

static inline ssize_t semihost_read(int fd, void *buf, size_t count) {
    uintptr_t args[3];
    args[0] = fd;
    args[1] = (uintptr_t)buf;
    args[2] = count;
    return (ssize_t)semihost_call(SYS_READ, args);
}

static inline int semihost_open(const char *filename, int mode, size_t len) {
    uintptr_t args[3];
    args[0] = (uintptr_t)filename;
    args[1] = mode;
    args[2] = len;
    return (int)semihost_call(SYS_OPEN, args);
}

static inline int semihost_close(int fd) {
    uintptr_t args[1];
    args[0] = fd;
    return (int)semihost_call(SYS_CLOSE, args);
}

#endif // SEMIHOST_RIFF_H
```

### Assembly Example (6502)

```asm
; Semihosting device for 6502
; Device base: $FE00

SEMIHOST_BASE = $FE00
RIFF_PTR      = SEMIHOST_BASE + $00
DOORBELL      = SEMIHOST_BASE + $10
STATUS        = SEMIHOST_BASE + $14

.data
riff_buffer:  .res 256
args:         .res 6

.code
; Write "Hello\n" to stdout
write_hello:
    ; Build arguments in memory
    lda #1              ; fd = 1 (stdout)
    sta args+0
    lda #<hello_str
    sta args+1
    lda #>hello_str
    sta args+2
    lda #6              ; count = 6
    sta args+3

    ; Build CALL chunk in RIFF buffer (assume already initialized)
    ; CALL chunk at offset 24
    lda #'C'
    sta riff_buffer+24
    lda #'A'
    sta riff_buffer+25
    lda #'L'
    sta riff_buffer+26
    lda #'L'
    sta riff_buffer+27

    ; Chunk size = 6 (4 + ptr_size=2) (LE)
    lda #6
    sta riff_buffer+28
    lda #0
    sta riff_buffer+29
    sta riff_buffer+30
    sta riff_buffer+31

    ; Opcode = SYS_WRITE (0x05)
    lda #$05
    sta riff_buffer+32
    lda #0
    sta riff_buffer+33
    sta riff_buffer+34
    sta riff_buffer+35

    ; arg_ptr (2 bytes, LE)
    lda #<args
    sta riff_buffer+36
    lda #>args
    sta riff_buffer+37

    ; Write RIFF_PTR (2 bytes, LE)
    lda #<riff_buffer
    sta RIFF_PTR+0
    lda #>riff_buffer
    sta RIFF_PTR+1

    ; Ring doorbell
    lda #1
    sta DOORBELL

    ; Poll for completion
wait_loop:
    lda STATUS
    and #$01            ; Check RESPONSE_READY
    beq wait_loop

    ; Read result from riff_buffer+32
    lda riff_buffer+32
    ; Result in A (low byte of bytes written)

    rts

.data
hello_str: .byte "Hello",$0A
```

---

## Host Implementation

### Virtual Device (Emulator)

**Pseudo-code for device emulation:**

```c
typedef struct {
    uint8_t riff_ptr[16];      // RIFF_PTR register storage
    uint8_t irq_status;        // IRQ_STATUS register
    uint8_t irq_enable;        // IRQ_ENABLE register
    uint8_t status;            // STATUS register

    // Cached CNFG values
    uint8_t word_size;
    uint8_t ptr_size;
    uint8_t endianness;

    // Emulator context
    void *guest_memory;
    size_t memory_size;
    int (*raise_irq)(void);
    int (*lower_irq)(void);
} SemihostDevice;

// Initialize device
void semihost_device_init(SemihostDevice *dev) {
    memset(dev, 0, sizeof(*dev));
    dev->status = 0x80;  // DEVICE_PRESENT bit
}

// Register write handler
void semihost_write_register(SemihostDevice *dev, uint32_t offset, uint8_t value) {
    switch (offset) {
        case 0x00 ... 0x0F:  // RIFF_PTR
            dev->riff_ptr[offset] = value;
            break;

        case 0x10:  // DOORBELL
            semihost_process_request(dev);
            break;

        case 0x12:  // IRQ_ENABLE
            dev->irq_enable = value;
            semihost_update_irq(dev);
            break;

        case 0x13:  // IRQ_ACK
            dev->irq_status &= ~value;  // Clear acknowledged bits
            semihost_update_irq(dev);
            break;
    }
}

// Register read handler
uint8_t semihost_read_register(SemihostDevice *dev, uint32_t offset) {
    switch (offset) {
        case 0x00 ... 0x0F:  // RIFF_PTR
            return dev->riff_ptr[offset];

        case 0x11:  // IRQ_STATUS
            return dev->irq_status;

        case 0x12:  // IRQ_ENABLE
            return dev->irq_enable;

        case 0x14:  // STATUS
            return dev->status;

        default:
            return 0x00;
    }
}

// Process semihosting request (called on DOORBELL write)
void semihost_process_request(SemihostDevice *dev) {
    // Decode RIFF_PTR based on emulated CPU characteristics
    // (emulator knows CPU address width and endianness)
    uint64_t riff_ptr = decode_riff_ptr(dev->riff_ptr);

    // Validate pointer
    if (riff_ptr >= dev->memory_size) {
        semihost_set_error(dev);
        return;
    }

    uint8_t *riff_buffer = (uint8_t *)dev->guest_memory + riff_ptr;

    // Parse RIFF header
    if (memcmp(riff_buffer, "RIFF", 4) != 0 ||
        memcmp(riff_buffer + 8, "SEMI", 4) != 0) {
        semihost_set_error(dev);
        return;
    }

    // Parse CNFG chunk (offset 12)
    if (memcmp(riff_buffer + 12, "CNFG", 4) != 0) {
        semihost_set_error(dev);
        return;
    }

    dev->word_size = riff_buffer[20];
    dev->ptr_size = riff_buffer[21];
    dev->endianness = riff_buffer[22];

    // Parse CALL chunk (offset 24)
    if (memcmp(riff_buffer + 24, "CALL", 4) != 0) {
        semihost_set_error(dev);
        return;
    }

    uint8_t opcode = riff_buffer[32];
    uint64_t arg_ptr = read_ptr(riff_buffer + 36, dev->ptr_size, dev->endianness);

    // Execute syscall
    uint64_t result;
    uint32_t errno_val;
    semihost_execute_syscall(dev, opcode, arg_ptr, &result, &errno_val);

    // Write RETN chunk (replacing CALL at offset 24)
    memcpy(riff_buffer + 24, "RETN", 4);
    write_u32_le(riff_buffer + 28, dev->word_size + 4);  // chunk_size
    write_value(riff_buffer + 32, result, dev->word_size, dev->endianness);
    write_u32_le(riff_buffer + 32 + dev->word_size, errno_val);

    // Set completion status
    dev->irq_status |= 0x01;  // RESPONSE_READY
    dev->status |= 0x01;      // RESPONSE_READY

    semihost_update_irq(dev);
}

// Update IRQ line state
void semihost_update_irq(SemihostDevice *dev) {
    if (dev->irq_status & dev->irq_enable) {
        dev->raise_irq();
    } else {
        dev->lower_irq();
    }
}

// Execute syscall
void semihost_execute_syscall(SemihostDevice *dev, uint8_t opcode,
                              uint64_t arg_ptr, uint64_t *result,
                              uint32_t *errno_val) {
    errno = 0;

    switch (opcode) {
        case 0x05: {  // SYS_WRITE
            uint64_t args[3];
            read_guest_args(dev, arg_ptr, args, 3);

            int fd = (int)args[0];
            uint64_t buf_ptr = args[1];
            size_t count = (size_t)args[2];

            // Read buffer from guest memory
            uint8_t *buf = malloc(count);
            read_guest_memory(dev, buf_ptr, buf, count);

            // Perform host write
            ssize_t written = write(fd, buf, count);
            free(buf);

            *result = (uint64_t)written;
            *errno_val = errno;
            break;
        }

        // ... other syscalls ...

        default:
            *result = (uint64_t)-1;
            *errno_val = ENOSYS;
            break;
    }
}
```

### Physical Device (FPGA/ASIC)

**Implementation notes for hardware:**

1. **State Machine:**
   ```
   IDLE -> WAIT_DOORBELL -> READ_RIFF -> PARSE_CNFG ->
   PARSE_CALL -> EXECUTE_SYSCALL -> WRITE_RETN -> SET_STATUS -> IDLE
   ```

2. **Bus Master for DMA:**
   - Implement bus master logic to read/write guest RAM
   - Handle arbitration with CPU
   - Support burst transfers for efficiency

3. **Syscall Execution:**
   - Forward to host via UART, SPI, JTAG, etc.
   - Host daemon processes syscalls
   - Return results to FPGA
   - FPGA writes RETN chunk via DMA

4. **Interrupt Logic:**
   - Simple combinational: `IRQ_OUT = |(IRQ_STATUS & IRQ_ENABLE)`
   - Register output to avoid glitches

**Example Verilog (simplified):**

```verilog
module semihost_device (
    input wire clk,
    input wire rst,

    // CPU bus interface
    input wire [4:0] addr,
    input wire [7:0] wdata,
    output reg [7:0] rdata,
    input wire wr_en,
    input wire rd_en,

    // DMA bus master interface
    output reg [31:0] dma_addr,
    output reg [7:0] dma_wdata,
    input wire [7:0] dma_rdata,
    output reg dma_wr,
    output reg dma_rd,

    // Interrupt output
    output wire irq_out
);

// Registers
reg [127:0] riff_ptr;
reg [7:0] irq_status;
reg [7:0] irq_enable;
reg [7:0] status;

// State machine
typedef enum {
    IDLE,
    PROCESS_REQUEST
} state_t;
state_t state;

// Register writes
always @(posedge clk) begin
    if (rst) begin
        riff_ptr <= 0;
        irq_status <= 0;
        irq_enable <= 0;
        status <= 8'h80;  // DEVICE_PRESENT
        state <= IDLE;
    end else if (wr_en) begin
        case (addr)
            5'h00: riff_ptr[7:0]     <= wdata;
            5'h01: riff_ptr[15:8]    <= wdata;
            5'h02: riff_ptr[23:16]   <= wdata;
            5'h03: riff_ptr[31:24]   <= wdata;
            // ... more pointer bytes ...

            5'h10: state <= PROCESS_REQUEST;  // DOORBELL

            5'h12: irq_enable <= wdata;

            5'h13: irq_status <= irq_status & ~wdata;  // ACK
        endcase
    end
end

// Register reads
always @(*) begin
    case (addr)
        5'h00: rdata = riff_ptr[7:0];
        5'h01: rdata = riff_ptr[15:8];
        // ... more pointer bytes ...
        5'h11: rdata = irq_status;
        5'h12: rdata = irq_enable;
        5'h14: rdata = status;
        default: rdata = 8'h00;
    endcase
end

// IRQ output
assign irq_out = |(irq_status & irq_enable);

// Request processing (simplified - real implementation more complex)
always @(posedge clk) begin
    case (state)
        IDLE: begin
            // Wait for doorbell
        end

        PROCESS_REQUEST: begin
            // DMA read RIFF buffer
            // Parse CNFG, CALL
            // Forward to host interface
            // ... (implementation specific) ...

            // Eventually:
            irq_status[0] <= 1'b1;  // RESPONSE_READY
            status[0] <= 1'b1;
            state <= IDLE;
        end
    endcase
end

endmodule
```

---

## Complete Examples

### Example 1: 16-bit System Writing "Hello\n"

**System:** 16-bit CPU, little-endian, word_size=2, ptr_size=2

**RIFF Buffer Layout:**

```
Offset  Content                      Description
------  ---------------------------  -----------
0x0000  'R' 'I' 'F' 'F'             RIFF signature
0x0004  0x2E 0x00 0x00 0x00         Size = 46 bytes (LE)
0x0008  'S' 'E' 'M' 'I'             Form type

0x000C  'C' 'N' 'F' 'G'             Config chunk
0x0010  0x04 0x00 0x00 0x00         Chunk size = 4 (LE)
0x0014  0x02                        word_size = 2
0x0015  0x02                        ptr_size = 2
0x0016  0x00                        endianness = LE
0x0017  0x00                        reserved

0x0018  'C' 'A' 'L' 'L'             Call chunk
0x001C  0x06 0x00 0x00 0x00         Chunk size = 6 (LE)
0x0020  0x05                        opcode = SYS_WRITE (0x05)
0x0021  0x00 0x00 0x00              reserved
0x0024  0x00 0x10                   arg_ptr = 0x1000 (LE)
```

**Arguments at 0x1000 (3 words × 2 bytes):**

```
0x1000  0x01 0x00       arg[0] = fd = 1 (stdout) (LE)
0x1002  0x00 0x20       arg[1] = buffer_ptr = 0x2000 (LE)
0x1004  0x06 0x00       arg[2] = count = 6 bytes (LE)
```

**Buffer at 0x2000:**

```
0x2000  'H' 'e' 'l' 'l' 'o' '\n'
```

**Device Response (RETN chunk replaces CALL):**

```
0x0018  'R' 'E' 'T' 'N'             Return chunk
0x001C  0x06 0x00 0x00 0x00         Chunk size = 6 (word_size=2 + 4) (LE)
0x0020  0x06 0x00                   result = 6 bytes written (LE)
0x0022  0x00 0x00 0x00 0x00         errno = 0 (success) (LE)
```

### Example 2: 32-bit Big-Endian System Opening File

**System:** 32-bit 68000, big-endian, word_size=4, ptr_size=4

**CNFG Chunk:**

```
0x000C  'C' 'N' 'F' 'G'
0x0010  0x04 0x00 0x00 0x00         Chunk size = 4 (LE - RIFF requirement!)
0x0014  0x04                        word_size = 4
0x0015  0x04                        ptr_size = 4
0x0016  0x01                        endianness = BE
0x0017  0x00                        reserved
```

**CALL Chunk (SYS_OPEN for "/tmp/test.txt"):**

```
0x0018  'C' 'A' 'L' 'L'
0x001C  0x08 0x00 0x00 0x00         Chunk size = 8 (LE)
0x0020  0x01                        opcode = SYS_OPEN (0x01)
0x0021  0x00 0x00 0x00              reserved
0x0024  0x00 0x00 0x30 0x00         arg_ptr = 0x00003000 (BE - note!)
```

**Arguments at 0x3000 (BE values!):**

```
0x3000  0x00 0x00 0x40 0x00   arg[0] = filename_ptr = 0x4000 (BE)
0x3004  0x00 0x00 0x00 0x00   arg[1] = mode = 0 (read-only) (BE)
0x3008  0x00 0x00 0x00 0x0E   arg[2] = length = 14 (BE)
```

**String at 0x4000:**

```
0x4000  '/''t''m''p''/''t''e''s''t''.''t''x''t' 0x00 0x00
```

**RETN Chunk (Success - fd=3):**

```
0x0018  'R' 'E' 'T' 'N'
0x001C  0x08 0x00 0x00 0x00         Chunk size = 8 (LE)
0x0020  0x00 0x00 0x00 0x03         result = 3 (fd) (BE)
0x0024  0x00 0x00 0x00 0x00         errno = 0 (LE - always!)
```

**Note:** The `arg_ptr` value (0x00003000) is written in BE format because it's in the CALL chunk data. However, the chunk size (0x08000000) is LE because it's part of the RIFF structure.

### Example 3: 8-bit System with 16-bit Pointers

**System:** 6502 (8-bit), word_size=1, ptr_size=2

**CNFG:**

```
0x0014  0x01    word_size = 1
0x0015  0x02    ptr_size = 2
0x0016  0x00    endianness = LE
```

**CALL (SYS_WRITE, 3 bytes):**

```
0x0020  0x05                opcode = SYS_WRITE
0x0024  0x00 0x08           arg_ptr = 0x0800 (LE, 2 bytes)
```

**Arguments at 0x0800 (1 byte words!):**

```
0x0800  0x01        arg[0] = fd = 1 (1 byte!)
0x0801  0x00        arg[1] low = buffer_ptr low byte
0x0802  0x09        arg[1] high = buffer_ptr high byte (ptr = 0x0900)
0x0803  0x03        arg[2] = count = 3 (1 byte!)
```

**Note:** Arguments are 1 byte each (word_size=1), but pointers are 2 bytes (ptr_size=2). The pointer occupies two consecutive array elements.

**RETN:**

```
0x0020  0x03                result = 3 bytes written (1 byte!)
0x0021  0x00 0x00 0x00 0x00 errno = 0 (always 4 bytes LE)
```

---

## ARM Semihosting Compatibility

### Syscall Numbers

This protocol uses the **ARM Semihosting specification** syscall numbers for maximum compatibility with existing toolchains (GCC, Clang, picolibc, newlib).

| Opcode | Name | Arguments | Return Value |
|--------|------|-----------|--------------|
| 0x01 | SYS_OPEN | (filename_ptr, mode, length) | fd or -1 |
| 0x02 | SYS_CLOSE | (fd) | 0 or -1 |
| 0x03 | SYS_WRITEC | (char_ptr) | - |
| 0x04 | SYS_WRITE0 | (string_ptr) | - |
| 0x05 | SYS_WRITE | (fd, buffer_ptr, length) | bytes written or -1 |
| 0x06 | SYS_READ | (fd, buffer_ptr, length) | bytes read or -1 |
| 0x07 | SYS_READC | () | character (0-255) |
| 0x08 | SYS_ISERROR | (status) | error code or 0 |
| 0x09 | SYS_ISTTY | (fd) | 1 if TTY, 0 otherwise |
| 0x0A | SYS_SEEK | (fd, position) | 0 or -1 |
| 0x0C | SYS_FLEN | (fd) | file length or -1 |
| 0x0D | SYS_TMPNAM | (buffer_ptr, id, length) | 0 or -1 |
| 0x0E | SYS_REMOVE | (filename_ptr, length) | 0 or -1 |
| 0x0F | SYS_RENAME | (old_ptr, old_len, new_ptr, new_len) | 0 or -1 |
| 0x10 | SYS_CLOCK | () | centiseconds since start |
| 0x11 | SYS_TIME | () | seconds since epoch |
| 0x12 | SYS_SYSTEM | (command_ptr, length) | exit code |
| 0x13 | SYS_ERRNO | () | last errno value |
| 0x15 | SYS_GET_CMDLINE | (buffer_ptr, length) | 0 or -1 |
| 0x16 | SYS_HEAPINFO | (block_ptr) | - |
| 0x18 | SYS_EXIT | (status) | no return |
| 0x20 | SYS_EXIT_EXTENDED | (exception, subcode) | no return |
| 0x30 | SYS_ELAPSED | () | 64-bit tick count |
| 0x31 | SYS_TICKFREQ | () | ticks per second |

### Argument Passing Convention

All arguments passed **indirectly** via pointer to array in guest memory (matches ARM standard).

**Example: SYS_WRITE**

ARM standard: 3 arguments
1. File descriptor (word)
2. Buffer pointer (word/pointer)
3. Length (word)

In RIFF protocol:
```
arg_ptr points to array in guest RAM:
  [arg_ptr + 0*word_size] = fd
  [arg_ptr + 1*word_size] = buffer_ptr
  [arg_ptr + 2*word_size] = length
```

Device reads these values, then reads `length` bytes from `buffer_ptr`.

### Open Mode Flags (SYS_OPEN)

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

## Future Extensions

### Design for Extensibility

RIFF's chunk-based structure enables backward-compatible extensions:

- **Unknown chunks:** Old implementations skip via chunk_size
- **New chunk types:** Don't break existing implementations
- **Version negotiation:** Via extended CNFG fields or new META chunk
- **Optional features:** Advertised via capability bits

### Potential Future Chunks

#### STRM - Streaming Data

For large file transfers without copying entire buffer:

```
'S' 'T' 'R' 'M'
[chunk_size]
  call_id     [4]     // Associates with CALL
  sequence    [4]     // Packet sequence number
  flags       [2]     // 0x01=more, 0x02=end
  data        [var]   // Actual data
```

**Use case:** Reading 100MB file in 4KB chunks.

#### EVNT - Host-Initiated Events

For host to signal guest asynchronously:

```
'E' 'V' 'N' 'T'
[chunk_size]
  event_type  [2]     // 0x01=signal, 0x02=timer, 0x03=host_message
  payload     [var]
```

**Use case:** Host sends Ctrl-C signal to guest application.

#### ABRT - Abort Operation

Cancel pending async operation:

```
'A' 'B' 'R' 'T'
[chunk_size]
  call_id     [4]
  reason      [2]     // 0x01=user, 0x02=timeout, 0x03=error
```

**Use case:** Cancel slow file operation.

#### META - Device Capabilities

Query device features:

```
'M' 'E' 'T' 'A'
[chunk_size]
  query_type  [2]     // 0x01=version, 0x02=features, 0x03=limits
  data        [var]   // Query-specific
```

**Response from device:**

```
'M' 'E' 'T' 'A'
[chunk_size]
  response    [var]   // Device fills in capabilities
```

**Use case:** Guest queries max buffer size, supported syscalls, protocol version.

### Extended Configuration

**CNFG chunk version 2:**

```
+0x00  1  word_size
+0x01  1  ptr_size
+0x02  1  endianness
+0x03  1  protocol_version   // 0x01 for v1, 0x02 for v2, etc.
+0x04  4  feature_flags      // Capability bitmap
+0x08  8  extensions         // Reserved for future use
```

**Feature flags:**
```
Bit 0: ASYNC_OPS       - Supports asynchronous operations
Bit 1: STREAMING       - Supports STRM chunks
Bit 2: HOST_EVENTS     - Supports EVNT chunks
Bit 3: LARGE_FILES     - Supports 64-bit file offsets
Bit 4: EXTENDED_ERRNO  - Supports extended error codes
Bits 5-31: Reserved
```

### Asynchronous Operations

**Multi-request support:**

1. Add `call_id` field to CALL chunk
2. Guest can submit multiple CALL chunks (different call_ids)
3. Device processes in background, returns RETN chunks when ready
4. Guest matches RETN to CALL via call_id

**Buffer management:**

- Guest allocates multiple RIFF buffers
- Each buffer gets unique call_id
- Guest tracks pending operations
- IRQ handler dispatches based on call_id

---

## Reference Tables

### Register Map Summary

| Offset | Size | Name | Type | Description |
|--------|------|------|------|-------------|
| 0x00 | 16 | RIFF_PTR | RW | Guest RAM pointer to RIFF buffer |
| 0x10 | 1 | DOORBELL | W | Write to trigger request |
| 0x11 | 1 | IRQ_STATUS | R | Interrupt status bits |
| 0x12 | 1 | IRQ_ENABLE | RW | Interrupt enable mask |
| 0x13 | 1 | IRQ_ACK | W | Clear interrupt bits |
| 0x14 | 1 | STATUS | R | Device status |
| 0x15 | 11 | RESERVED | - | Future use |

### Status Bit Definitions

| Bit | Name | R/W | Description |
|-----|------|-----|-------------|
| 0 | RESPONSE_READY | R | Request completed |
| 1 | ERROR | R | Error occurred |
| 7 | DEVICE_PRESENT | R | Always 1 (device exists) |

### RIFF Chunk Types

| FourCC | Name | Direction | Size | Description |
|--------|------|-----------|------|-------------|
| CNFG | Configuration | Guest→Device | 12 bytes | Architecture parameters |
| CALL | Call | Guest→Device | 12+ptr_size | Syscall request |
| RETN | Return | Device→Guest | 12+word_size | Syscall result |
| STRM | Stream | Bidirectional | Variable | Streaming data (future) |
| EVNT | Event | Device→Guest | Variable | Async events (future) |
| ABRT | Abort | Bidirectional | Variable | Cancel operation (future) |
| META | Metadata | Bidirectional | Variable | Capabilities (future) |

### Endianness Encoding

| Value | Name | Byte Order | Example CPUs |
|-------|------|------------|--------------|
| 0x00 | Little Endian | LSB first | x86, ARM, RISC-V |
| 0x01 | Big Endian | MSB first | 68000, SPARC, PowerPC |
| 0x02 | PDP Endian | Middle | PDP-11, VAX |

---

## Appendix: Design Rationale

### Why Separate Device Registers and RIFF Buffer?

**Alternative:** Device could contain the RIFF buffer (like classic UARTs with internal FIFOs).

**Why not:**
1. **Scalability:** 8-bit CPU with 2KB RAM needs tiny buffer; 64-bit CPU with 16GB RAM can use huge buffer
2. **Flexibility:** Guest chooses buffer location (stack, heap, static)
3. **Simplicity:** Device stays minimal (32 bytes), easy to implement in FPGA
4. **Consistency:** Same device works from 6502 to modern ARM

### Why RIFF Header in Little-Endian?

**RIFF is a Microsoft/Intel standard** - always little-endian by specification. Maintaining this:
1. **Compatibility:** Standard RIFF parsers work
2. **Simplicity:** One canonical format
3. **Tooling:** Can use RIFF editors, debuggers on captures
4. **Precedent:** WAV, AVI, WebP all use LE RIFF

**Cost for BE CPUs:** Must swap chunk sizes. Minimal code (~4 instructions per chunk).

### Why 16-Byte RIFF_PTR?

**Future-proofing for 128-bit addressing:**
1. Research CPUs exploring 128-bit address spaces
2. Cryptographic processors with wide word sizes
3. Cost is negligible (just storage), benefit is long-term viability
4. Unused bytes simply ignored

### Why errno Always 4 Bytes LE?

**Consistency over flexibility:**
1. POSIX errno values fit in 32 bits (0-255 typical range)
2. Always LE matches RIFF structure
3. Simplifies host implementation (no per-guest errno encoding)
4. Size cost negligible (4 bytes vs. word_size)

---

**End of Specification**
