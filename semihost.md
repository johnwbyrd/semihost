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

This specification describes a **memory-mapped semihosting peripheral** - a hardware device, whether physical or virtual, that provides I/O services to embedded systems during development.  This allows a new system to immediately receive file system, timekeeping, and other services, without bringing up a bunch of device drivers on that new target.

This device implements, more or less transparently, all the functions of the [ARM semihosting interface](https://developer.arm.com/documentation/dui0282/b/semihosting/semihosting/what-is-semihosting-) which in turn provides support for opening, reading, writing, and closing files, as well as timekeeping services.  Having these services ready and working, is very handy when bringing up a compiler, debugger, or new library on a novel target.

The device uses RIFF (Resource Interchange File Format) as its communication protocol, enabling architecture-agnostic operation across any CPU from 8-bit to 128-bit.

Think of this as a discrete chip on the system bus, similar to:
- **UART (16550)** - provides serial I/O
- **RTC (DS1307)** - provides timekeeping

The device occupies a small memory-mapped register space (32 bytes) and communicates with the CPU through standard memory read/write operations.

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

The semihosting device presents 32 bytes of memory-mapped registers to the CPU.

### Device Register Map

The semihosting device presents 32 bytes of memory-mapped registers:

| Offset | Size | Name | Access | Description |
|--------|------|------|--------|-------------|
| 0x00 | 16 | RIFF_PTR | RW | Pointer to RIFF buffer in guest RAM |
| 0x10 | 1 | DOORBELL | W | Write any value to trigger request |
| 0x11 | 1 | IRQ_STATUS | R | Interrupt status flags |
| 0x12 | 1 | IRQ_ENABLE | RW | Interrupt enable mask |
| 0x13 | 1 | IRQ_ACK | W | Write 1s to clear interrupt bits |
| 0x14 | 1 | STATUS | R | Device status flags |
| 0x15-0x20 | 11 | RESERVED | - | Reserved for future use |

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
- Bit 0: RESPONSE_READY - Semihosting request completed
- Bit 1: ERROR - Error occurred during processing
- Bits 2-7: Reserved (read as 0)

**Operation:** Host sets bits when events occur. Guest reads to determine interrupt cause. Bits are cleared by writing to IRQ_ACK.

#### IRQ_ENABLE (0x12, 1 byte, Read/Write)

**Purpose:** Controls which interrupt conditions can assert the CPU's interrupt line.

**Bit definitions:**
- Bit 0: RESPONSE_READY_EN - Enable interrupt on completion
- Bit 1: ERROR_EN - Enable interrupt on error
- Bits 2-7: Reserved (write 0, read as 0)

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
- Bit 0: RESPONSE_READY - Same as IRQ_STATUS bit 0 (convenience)
- Bit 7: DEVICE_PRESENT - Always 1 (device exists and is functional)
- Bits 1-6: Reserved (read as 0)

**Operation:** Guest can poll STATUS register to wait for completion without enabling interrupts. Bit 0 provides same information as IRQ_STATUS for convenience.

### Memory Organization

**Key Principle:** The device registers and the RIFF buffer are **separate**.

**Device Registers** (32 bytes at device base address):
- RIFF_PTR, DOORBELL, IRQ_STATUS, etc.
- Fixed location in memory map
- Directly accessed by CPU

**RIFF Buffer** (variable size in guest RAM):
- Guest allocates wherever convenient
- Contains RIFF header + chunks
- Guest tells device location via RIFF_PTR
- Device reads/writes this buffer via memory access

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

**Behavior:** IRQ_OUT is asserted when the bitwise AND of IRQ_STATUS and IRQ_ENABLE is non-zero. When any enabled interrupt condition is active, IRQ_OUT is asserted. Connect to CPU's IRQ or NMI input as appropriate for the system.

**Typical connection:**
- Simple systems: Connect to CPU IRQ input
- Priority systems: Connect through interrupt controller
- Polling-only systems: Leave unconnected

### Timing Characteristics

#### Synchronous Operation (Polling Mode)

The device operates **synchronously** with deterministic timing:

1. Guest writes to DOORBELL (1 bus cycle)
2. Device detects write (combinational or 1 clock)
3. Device processes request (variable time, typically µs to ms)
4. Device writes RETN chunk to guest RAM (multiple bus cycles)
5. Device sets STATUS bit 0 (1 clock)
6. Guest reads STATUS to detect completion (1 bus cycle per poll)

**Processing time:** Depends on syscall type:
- `SYS_WRITEC`: Microseconds (write single character)
- `SYS_WRITE`: Milliseconds (write large buffer to disk)
- `SYS_OPEN`: Milliseconds (filesystem access)

**Guest blocking:** Guest typically polls STATUS in a tight loop, consuming CPU cycles during I/O.

#### Asynchronous Operation (Interrupt Mode)

For interrupt-driven operation:

1. Guest writes IRQ_ENABLE = 0x01 (enable RESPONSE_READY interrupt)
2. Guest writes to DOORBELL
3. Guest continues other work (or enters low-power mode)
4. Device processes request in background
5. Device sets IRQ_STATUS bit 0 and asserts IRQ_OUT
6. CPU takes interrupt, guest handler runs
7. Handler reads result from RIFF buffer
8. Handler writes IRQ_ACK = 0x01 to clear interrupt

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

**Basic structure:** The RIFF container begins with the four-character code 'RIFF', followed by a 4-byte size field, followed by the form type 'SEMI'. Within the container are chunks: CNFG (configuration data), CALL (syscall data), and RETN (return data, written by the device).

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

**Implementation note for big-endian guests:** Guest must swap multi-byte values in RIFF headers (sizes) but NOT in chunk data. Big-endian systems will need helper functions to write little-endian values for RIFF structure fields while using native byte order for data values within chunks.

### Chunk Nesting and Organization

**RIFF hierarchy for semihosting:** The RIFF container with form type 'SEMI' contains: CNFG (leaf chunk for configuration), CALL (container chunk for request, which may contain PARM leaf chunks for parameters and DATA leaf chunks for buffers/strings), and RETN or ERRO (container chunks for response, which may contain DATA leaf chunks for read data).

**Nesting rules:**
- Maximum nesting depth: 2 levels (RIFF → CALL/RETN → PARM/DATA)
- CALL and RETN are container chunks (may contain sub-chunks)
- PARM and DATA are leaf chunks (no sub-chunks allowed)
- CNFG is a leaf chunk
- Invalid: PARM inside PARM, DATA inside DATA, CALL inside CALL

**Chunk alignment:**
- All chunks should be naturally aligned
- RIFF buffer should start on a word-aligned address
- Per RIFF specification, odd-sized chunks are padded to even boundary

### CNFG Chunk - Configuration

**Purpose:** Declares guest CPU architecture parameters.

**Chunk ID:** 'CNFG' (ASCII codes 0x43 0x4E 0x46 0x47)

**Format:** The CNFG chunk contains a 4-byte chunk ID, a 4-byte little-endian chunk size (value 4), followed by four data bytes: word_size (bytes per word), ptr_size (bytes per pointer, may differ from word size), endianness (0=LE, 1=BE, 2=PDP), and one reserved byte (must be 0x00).

**Total chunk size:** 12 bytes (8 byte header + 4 byte data)

**Field definitions:**

**word_size:** Size in bytes of the natural integer type for the architecture. Represents the size of C `int` or equivalent default integer type.
- 6502 (LLVM-MOS): 2 bytes (16-bit int)
- ARM Cortex-M: 4 bytes (32-bit int)
- x86-64: 4 bytes (32-bit int, even on 64-bit platforms)
- AVR: 2 bytes (16-bit int)

**ptr_size:** Size in bytes of pointer types for the architecture. Represents the size of C pointer types (`void*`, `char*`). Equals the address bus width.
- 6502: 2 bytes (16-bit addressing)
- ARM Cortex-M: 4 bytes (32-bit addressing)
- x86-64: 8 bytes (64-bit addressing)
- AVR: 2 bytes (16-bit addressing)

**When word_size ≠ ptr_size:**
- Some DSPs have 16-bit data (word_size=2) but 24-bit code pointers (ptr_size=3)
- x86-64 has 32-bit int (word_size=4) but 64-bit pointers (ptr_size=8)
- This distinction allows the protocol to correctly size both scalar values and addresses

**Endianness values:**
- `0` - Little Endian (LSB first): x86, ARM Cortex-M, RISC-V (typically)
- `1` - Big Endian (MSB first): 68000, SPARC, PowerPC (classic mode), MIPS (BE mode)
- `2` - PDP Endian (middle-endian): PDP-11, VAX (historic)
- `3-255` - Reserved for future use

**Configuration caching:**
- CNFG chunk should be sent **once** at session start (first RIFF buffer after device initialization)
- Device caches word_size, ptr_size, and endianness for the entire session
- Subsequent RIFF buffers **omit** the CNFG chunk and start directly with CALL
- To change configuration, guest must reinitialize the device or send a new CNFG chunk
- `word_size` != `ptr_size` is valid (e.g., 16-bit words with 24-bit pointers on some DSPs)
- Host uses these cached values to correctly interpret all multi-byte values in PARM chunks

### CALL Chunk - Syscall Request Container

**Purpose:** Container chunk for a semihosting operation request.

**Chunk ID:** 'CALL' (ASCII codes 0x43 0x41 0x4C 0x4C)

**Format:** The CALL chunk contains a 4-byte chunk ID, a 4-byte little-endian chunk size (variable), followed by: opcode (1 byte, ARM semihosting syscall number), three reserved bytes (must be 0x00), and then a variable number of sub-chunks (PARM and DATA chunks for parameters).

**Opcode:** ARM semihosting syscall number (0x01-0x31, see reference table)

**Sub-chunks:** The CALL chunk contains nested PARM and DATA chunks representing the syscall parameters. Parameters appear in the order expected by the syscall.

### PARM Chunk - Parameter Value

**Purpose:** Represents a single scalar parameter (integer, pointer, etc.) within a CALL chunk.

**Chunk ID:** 'PARM' (ASCII codes 0x50 0x41 0x52 0x4D)

**Format:** The PARM chunk contains a 4-byte chunk ID, a 4-byte little-endian chunk size (4 + value_size), followed by: param_type (1 byte parameter type code), three reserved bytes (must be 0x00), and then the parameter value in guest endianness (variable size).

**Parameter types:**
- `0x01` - Word value (size = word_size from CNFG)
  - Used for: file descriptors, counts, modes, status codes, offsets
  - Represents scalar integer values in the guest's natural integer size
- `0x02` - Pointer value (size = ptr_size from CNFG)
  - Used for: memory addresses (when needed, though most use DATA chunks instead)
  - Sized to hold valid addresses in the guest's address space
- `0x03-0xFF` - Reserved for future use

**Value field:**
- Size depends on param_type and CNFG settings
- Endianness matches guest's declared endianness
- For type 0x01 (word): value is word_size bytes
- For type 0x02 (pointer): value is ptr_size bytes

**Note:** Parameters must appear in the order expected by the syscall.

### DATA Chunk - Binary Data or String

**Purpose:** Represents binary data, strings, or buffer contents within CALL or RETN chunks.

**Chunk ID:** 'DATA' (ASCII codes 0x44 0x41 0x54 0x41)

**Format:** The DATA chunk contains a 4-byte chunk ID, a 4-byte little-endian chunk size (4 + payload_length), followed by: data_type (1 byte data type code), three reserved bytes (must be 0x00), and then the payload (actual data bytes, variable length).

**Data types:**
- `0x01` - Binary data (arbitrary bytes)
- `0x02` - String (null-terminated ASCII/UTF-8)
- `0x03-0xFF` - Reserved for future use

**Payload:** Variable-length data. For strings (type 0x02), includes null terminator.

**Padding:** If payload length is odd, chunk is padded to even byte boundary per RIFF specification.

**Usage:**
- In CALL chunks: Contains data to write (SYS_WRITE), filenames (SYS_OPEN), command strings (SYS_SYSTEM)
- In RETN chunks: Contains data read back (SYS_READ, SYS_READC)

### RETN Chunk - Return Value and Data

**Purpose:** Device response containing syscall result and optional data.

**Chunk ID:** 'RETN' (ASCII codes 0x52 0x45 0x54 0x4E)

**Format:** The RETN chunk contains a 4-byte chunk ID, a 4-byte little-endian chunk size (variable), followed by: result (word_size bytes, syscall return value in guest endianness), errno (4 bytes, POSIX errno in little-endian, 0=success), and optional sub-chunks (DATA chunks for read operations).

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

**Sub-chunks:** For syscalls that return data (SYS_READ, SYS_READC), the RETN chunk contains a DATA sub-chunk with the actual bytes read.

**Device behavior:** Device **overwrites** the CALL chunk with RETN chunk in the same buffer location.

### ERRO Chunk - Error Response

**Purpose:** Device response when RIFF structure is malformed or request cannot be processed.

**Chunk ID:** 'ERRO' (ASCII codes 0x45 0x52 0x52 0x4F)

**Format:** The ERRO chunk contains a 4-byte chunk ID, a 4-byte little-endian chunk size (variable), followed by: error_code (2 bytes in little-endian), two reserved bytes (must be 0x00), and an optional ASCII error message (variable length).

**Error codes:**
- `0x01` - Invalid chunk structure
- `0x02` - Malformed RIFF format
- `0x03` - Missing CNFG chunk
- `0x04` - Unsupported opcode
- `0x05` - Invalid parameter count
- `0x06-0xFFFF` - Reserved for future use

**Device behavior:** Device writes ERRO chunk instead of RETN when it cannot parse or execute the request.

---

## Operation Modes

### Synchronous Mode (Polling)

**Default mode.** Guest blocks waiting for response by polling STATUS register.

**Operation:**
1. Guest builds RIFF buffer with CALL chunk containing PARM/DATA sub-chunks
2. Guest writes RIFF buffer address to RIFF_PTR register
3. Guest writes to DOORBELL register to trigger processing
4. Guest polls STATUS register until RESPONSE_READY bit is set
5. Guest reads RETN chunk from RIFF buffer (device overwrites CALL with RETN)

**CACHE COHERENCY WARNING**

If guest CPU has data cache, guest MUST:
1. Flush data cache after writing RIFF buffer (before DOORBELL write)
2. Ensure DOORBELL write completes (may need memory barrier)
3. Invalidate cache before reading RETN chunk from buffer

Failure to do so may result in stale data or corruption.

**Characteristics:**
- Simple to implement
- No interrupt handler needed
- Guest wastes CPU cycles during I/O
- Suitable for simple single-tasking programs

**Typical use:** Educational platforms, simple test programs, early firmware bring-up.

**Concurrent request handling:**

Guest MUST NOT write DOORBELL while STATUS.RESPONSE_READY is set. Device behavior is undefined if concurrent requests are issued. Device may ignore the second DOORBELL or abort the first request.

### Asynchronous Mode (Interrupt-Driven)

**Optional mode.** Guest can perform other work while waiting for completion interrupt.

**Operation:**
1. Guest enables RESPONSE_READY interrupt via IRQ_ENABLE register
2. Guest builds RIFF buffer and writes to RIFF_PTR
3. Guest writes to DOORBELL and continues other work
4. Device processes request and sets IRQ_STATUS.RESPONSE_READY
5. Device asserts interrupt line
6. Guest interrupt handler reads RETN chunk and acknowledges interrupt via IRQ_ACK

**Cache coherency requirements:** Same as synchronous mode - flush before DOORBELL, invalidate before reading RETN.

**Characteristics:**
- More complex (requires interrupt handler)
- Guest can multitask during I/O
- Enables true asynchronous operation
- Suitable for operating systems and complex applications

**Typical use:** OS development, preemptive multitasking, power-sensitive applications.

### Guest Memory Access by Device

**Critical detail:** The device accesses guest RAM to read/write RIFF buffers.

**Physical hardware:** Device is a bus master, can initiate DMA reads/writes to guest memory.

**Virtual hardware:** Emulator directly accesses guest memory structures (memory array, MMU translations, etc.).

**Implications:**
1. **Cache coherency:** Guest CPU MUST flush data cache after writing RIFF buffer and invalidate before reading RETN chunk (see warnings above)
2. **MMU/Virtual addressing:** See "Address Interpretation" subsection below
3. **Access permissions:** Device ignores memory protection (privileged access)

**Physical device with DMA operation:** Guest writes RIFF_PTR with the buffer address, then writes DOORBELL. The device bus master logic reads the RIFF header from guest RAM, reads the CNFG chunk (first time only), reads the CALL chunk header and recursively reads PARM and DATA sub-chunks, executes the syscall, writes the RETN chunk to guest RAM (replacing CALL), and for SYS_READ appends a DATA sub-chunk with read data. Finally, the device sets STATUS bit 0 (RESPONSE_READY).

### Address Interpretation

**The RIFF_PTR register and pointer values in PARM chunks contain guest memory addresses. How these are interpreted depends on the implementation:**

**Virtual devices (emulators):**
- May accept virtual addresses from guest
- Emulator translates using guest's MMU state
- Guest can use normal pointers from its address space
- Simplifies guest software (no address translation needed)

**Physical devices (FPGA/ASIC):**
- Require physical addresses in all pointer fields
- Device has no access to guest's MMU translation tables
- Guest responsibility:
  - Disable MMU entirely, OR
  - Provide physical addresses (translate in software), OR
  - Use identity-mapped memory regions for RIFF buffers
- Physical device configuration:
  - Manual endianness configuration (DIP switches, compile-time constants, configuration registers)
  - Cannot auto-detect guest byte order

**Recommendation:** Guest software should allocate RIFF buffers in physical memory or identity-mapped regions when possible for maximum compatibility with both virtual and physical devices.

---

## ARM Semihosting Compatibility

### Syscall Numbers

This protocol uses the **ARM Semihosting specification** syscall numbers for maximum compatibility with existing toolchains (GCC, Clang, picolibc, newlib).

| Opcode | Name | Arguments | Return Value |
|--------|------|-----------|--------------|
| 0x01 | SYS_OPEN | (filename, mode, length) | fd or -1 |
| 0x02 | SYS_CLOSE | (fd) | 0 or -1 |
| 0x03 | SYS_WRITEC | (char_ptr) | - |
| 0x04 | SYS_WRITE0 | (string) | - |
| 0x05 | SYS_WRITE | (fd, data, length) | bytes written or -1 |
| 0x06 | SYS_READ | (fd, length) | bytes read or -1 |
| 0x07 | SYS_READC | () | character (0-255) |
| 0x08 | SYS_ISERROR | (status) | error code or 0 |
| 0x09 | SYS_ISTTY | (fd) | 1 if TTY, 0 otherwise |
| 0x0A | SYS_SEEK | (fd, position) | 0 or -1 |
| 0x0C | SYS_FLEN | (fd) | file length or -1 |
| 0x0D | SYS_TMPNAM | (id, length) | temp filename or -1 |
| 0x0E | SYS_REMOVE | (filename, length) | 0 or -1 |
| 0x0F | SYS_RENAME | (old_name, old_len, new_name, new_len) | 0 or -1 |
| 0x10 | SYS_CLOCK | () | centiseconds since start |
| 0x11 | SYS_TIME | () | seconds since epoch |
| 0x12 | SYS_SYSTEM | (command, length) | exit code |
| 0x13 | SYS_ERRNO | () | last errno value |
| 0x15 | SYS_GET_CMDLINE | (length) | command line or -1 |
| 0x16 | SYS_HEAPINFO | () | heap info structure |
| 0x18 | SYS_EXIT | (status) | no return |
| 0x20 | SYS_EXIT_EXTENDED | (exception, subcode) | no return |
| 0x30 | SYS_ELAPSED | () | 64-bit tick count |
| 0x31 | SYS_TICKFREQ | () | ticks per second |

### Argument Encoding in RIFF Chunks

ARM semihosting syscalls are mapped to PARM and DATA chunks:

**Scalar arguments** (fd, mode, length, status, etc.) → PARM chunks (type 0x01 = word)

**String/buffer arguments** (filenames, data to write, commands) → DATA chunks

**Buffer output** (data read, command line) → DATA chunks in RETN

**Example: SYS_WRITE(fd=1, data="Hello\n", length=6)**

CALL chunk contains:
- PARM chunk: fd = 1
- PARM chunk: length = 6
- DATA chunk: payload = "Hello\n"

**Example: SYS_READ(fd=3, length=256)**

CALL chunk contains:
- PARM chunk: fd = 3
- PARM chunk: length = 256

RETN chunk contains:
- result = bytes_read
- errno = 0
- DATA chunk: payload = bytes read

**Example: SYS_OPEN(filename="/tmp/test.txt", mode=0, length=14)**

CALL chunk contains:
- DATA chunk: payload = "/tmp/test.txt\0"
- PARM chunk: mode = 0
- PARM chunk: length = 14

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

For large file transfers without copying entire buffer. The STRM chunk contains a chunk ID, chunk size, a call_id field (4 bytes, associates with CALL), sequence number (4 bytes, packet sequence), flags (2 bytes: 0x01=more, 0x02=end), and variable-length data.

**Use case:** Reading 100MB file in 4KB chunks.

#### EVNT - Host-Initiated Events

For host to signal guest asynchronously. The EVNT chunk contains a chunk ID, chunk size, an event_type field (2 bytes: 0x01=signal, 0x02=timer, 0x03=host_message), and a variable-length payload.

**Use case:** Host sends Ctrl-C signal to guest application.

#### ABRT - Abort Operation

Cancel pending async operation. The ABRT chunk contains a chunk ID, chunk size, a call_id field (4 bytes), and a reason code (2 bytes: 0x01=user, 0x02=timeout, 0x03=error).

**Use case:** Cancel slow file operation.

#### META - Device Capabilities

Query device features. The META chunk contains a chunk ID, chunk size, a query_type field (2 bytes: 0x01=version, 0x02=features, 0x03=limits), and query-specific data (variable length). The device responds with a META chunk containing a variable-length response with the requested capabilities.

**Use case:** Guest queries max buffer size, supported syscalls, protocol version.

### Extended Configuration

**CNFG chunk version 2:** Future versions of the CNFG chunk may include additional fields: protocol_version (1 byte: 0x01 for v1, 0x02 for v2), feature_flags (4 bytes, capability bitmap), and extensions (8 bytes reserved for future use).

**Feature flags bitmap:**
- Bit 0: ASYNC_OPS - Supports asynchronous operations
- Bit 1: STREAMING - Supports STRM chunks
- Bit 2: HOST_EVENTS - Supports EVNT chunks
- Bit 3: LARGE_FILES - Supports 64-bit file offsets
- Bit 4: EXTENDED_ERRNO - Supports extended error codes
- Bits 5-31: Reserved

### Asynchronous Operations

**Multi-request support:** Future versions may add a call_id field to the CALL chunk, allowing the guest to submit multiple CALL chunks with different call_ids. The device would process requests in background and return RETN chunks when ready, with the guest matching RETN to CALL via call_id.

**Buffer management:** For async operations, the guest would allocate multiple RIFF buffers, assign each buffer a unique call_id, track pending operations, and dispatch IRQ handlers based on call_id.

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
| CALL | Call (container) | Guest→Device | Variable | Syscall request with sub-chunks |
| PARM | Parameter | Guest→Device | 12+value_size | Scalar parameter (in CALL) |
| DATA | Data | Bidirectional | 12+payload_size | Binary/string data |
| RETN | Return (container) | Device→Guest | Variable | Syscall result with optional DATA |
| ERRO | Error | Device→Guest | Variable | Error response |
| EVNT | Event | Device→Guest | Variable | Async events (future) |
| ABRT | Abort | Bidirectional | Variable | Cancel operation (future) |
| META | Metadata | Bidirectional | Variable | Capabilities (future) |

### PARM Chunk Parameter Types

| Value | Name | Size | Description |
|-------|------|------|-------------|
| 0x01 | Word | word_size bytes | Integer/scalar value in guest endianness |
| 0x02 | Pointer | ptr_size bytes | Pointer value in guest endianness |
| 0x03-0xFF | Reserved | - | Reserved for future use |

### DATA Chunk Data Types

| Value | Name | Description |
|-------|------|-------------|
| 0x01 | Binary | Arbitrary binary data |
| 0x02 | String | Null-terminated ASCII/UTF-8 string |
| 0x03-0xFF | Reserved | Reserved for future use |

### ERRO Chunk Error Codes

| Value | Name | Description |
|-------|------|-------------|
| 0x01 | Invalid chunk structure | Malformed chunk headers or nesting |
| 0x02 | Malformed RIFF format | Invalid RIFF signature or structure |
| 0x03 | Missing CNFG chunk | CNFG required but not sent |
| 0x04 | Unsupported opcode | Syscall number not implemented |
| 0x05 | Invalid parameter count | Wrong number of PARM/DATA chunks |
| 0x06-0xFFFF | Reserved | Reserved for future use |

### Endianness Encoding

| Value | Name | Byte Order | Example CPUs |
|-------|------|------------|--------------|
| 0x00 | Little Endian | LSB first | x86, ARM, RISC-V |
| 0x01 | Big Endian | MSB first | 68000, SPARC, PowerPC |
| 0x02 | PDP Endian | Middle | PDP-11, VAX |