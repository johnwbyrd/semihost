RIFF-Based Semihosting Device Specification
============================================

Version 0.1.0

.. contents:: Table of Contents
   :local:
   :depth: 2

Overview
--------

What Is This?
^^^^^^^^^^^^^

This specification describes a **memory-mapped semihosting peripheral** --
a hardware device, whether physical or virtual, that provides I/O services
to embedded systems during development. This allows a new system to
immediately receive file system, timekeeping, and other services without
bringing up device drivers on that new target.

This device implements, more or less transparently, all the functions of
the `ARM semihosting interface
<https://developer.arm.com/documentation/dui0282/b/semihosting/semihosting/what-is-semihosting->`_
which provides support for opening, reading, writing, and closing files,
as well as timekeeping services. Having these services ready and working
is very handy when bringing up a compiler, debugger, or new library on a
novel target.

The device uses RIFF (Resource Interchange File Format) as its communication
protocol, enabling architecture-agnostic operation across any CPU from
8-bit to 128-bit.

Think of this as a discrete chip on the system bus, similar to:

- **UART (16550)** -- provides serial I/O
- **RTC (DS1307)** -- provides timekeeping

The device occupies a small memory-mapped register space (32 bytes) and
communicates with the CPU through standard memory read/write operations.

Traditional semihosting uses trap instructions (BKPT, SVC, EBREAK, etc.)
which:

- Require debugger support
- Are architecture-specific
- Don't work in all execution environments
- Complicate the CPU execution model

RIFF-based semihosting solves these problems by:

- Using **memory-mapped I/O** (standard load/store operations)
- Being **completely architecture-agnostic** (works on any CPU)
- Being **self-describing** through explicit configuration
- Being **extensible** without breaking compatibility

Key Features
^^^^^^^^^^^^

- **Universal**: Works with any integer size (8-bit through 128-bit and beyond)
- **Standard**: Compatible with ARM semihosting syscall numbers
- **Simple**: Only 32 bytes of device registers
- **Efficient**: Guest manages all buffers in its own RAM
- **Flexible**: Synchronous operation with optional timer interrupts
- **Robust**: Self-describing protocol with explicit endianness negotiation

Hardware Architecture
---------------------

The semihosting device presents 32 bytes of memory-mapped registers to the CPU.

Device Register Map
^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 10 10 15 10 55

   * - Offset
     - Size
     - Name
     - Access
     - Description
   * - 0x00
     - 8
     - SIGNATURE
     - R
     - ASCII "SEMIHOST" (device identification)
   * - 0x08
     - 16
     - RIFF_PTR
     - RW
     - Pointer to RIFF buffer in guest RAM
   * - 0x18
     - 1
     - DOORBELL
     - W
     - Write any value to trigger request
   * - 0x19
     - 1
     - STATUS
     - RW
     - Interrupt pending (write 0 to clear)
   * - 0x1A-0x1F
     - 6
     - RESERVED
     - \-
     - Reserved for future use

Register Descriptions
^^^^^^^^^^^^^^^^^^^^^

SIGNATURE (0x00-0x07, 8 bytes, Read-Only)
"""""""""""""""""""""""""""""""""""""""""

**Purpose:** Device identification. Contains ASCII string "SEMIHOST" to
allow guests to verify device presence.

**Format:** 8 bytes containing the ASCII characters 'S', 'E', 'M', 'I',
'H', 'O', 'S', 'T' (0x53, 0x45, 0x4D, 0x49, 0x48, 0x4F, 0x53, 0x54).

**Usage:** Guest software can read these 8 bytes at the device base
address to confirm a semihosting device is present before attempting
any operations. This is more reliable than checking a single status bit.

**Write behavior:** Writes are ignored.

RIFF_PTR (0x08-0x17, 16 bytes, Read/Write)
""""""""""""""""""""""""""""""""""""""""""

**Purpose:** Holds the guest memory address of the RIFF communication buffer.

**Format:** Raw byte storage. Guest writes pointer in its native byte order
using as many bytes as needed (2, 4, 8, or 16 bytes). Unused high bytes
are ignored.

**Host behavior:** Host knows the guest CPU's address width and endianness
from emulator/system configuration. Reads the appropriate number of bytes
and interprets them correctly without guessing.

**Address interpretation:**

- **Virtual devices (emulators):** May accept virtual addresses and
  translate using guest's MMU
- **Physical devices (FPGA/ASIC):** Require physical addresses only
  (guest must disable MMU or use identity-mapped memory)

**Examples:**

- 16-bit CPU (6502): Writes 2 bytes in LE order
- 32-bit CPU (68000): Writes 4 bytes in BE order
- 64-bit CPU (x86-64): Writes 8 bytes in LE order
- 128-bit future CPU: Writes 16 bytes in native order

DOORBELL (0x18, 1 byte, Write-Only)
"""""""""""""""""""""""""""""""""""

**Purpose:** Trigger register. Writing any value initiates request processing.

**Operation:**

1. Guest builds RIFF structure in its RAM
2. Guest writes pointer to RIFF_PTR
3. Guest writes any value (typically 0x01) to DOORBELL
4. Device detects write and begins processing

**Read behavior:** Reading returns undefined value (typically 0x00).

STATUS (0x19, 1 byte, Read/Write)
"""""""""""""""""""""""""""""""""

**Purpose:** Indicates pending interrupt source. Write 0 to acknowledge.

**Values:**

- ``0``: No interrupt pending
- ``1``: Timer tick occurred
- ``2+``: Reserved for future interrupt sources

**Operation:**

- Device sets STATUS to non-zero when an interrupt fires (latched)
- Device asserts CPU's interrupt line (ASSERT_LINE)
- Guest reads STATUS to determine interrupt source
- Guest writes 0 to acknowledge: clears STATUS and deasserts interrupt line
- Timer continues running; next tick will set STATUS and assert IRQ again

**Important:** Writing 0 to STATUS only acknowledges the current interrupt.
It does NOT stop the timer. To stop the timer entirely, call
``SYS_TIMER_CONFIG(0)`` via the RIFF protocol.

**Guest ISR flow:**

1. Read STATUS to determine interrupt source
2. Handle the interrupt based on value
3. Write 0 to STATUS to acknowledge and deassert IRQ
4. Return from ISR

**Device detection:** Use the SIGNATURE register at offset 0x00 to detect
device presence. Read the 8-byte "SEMIHOST" signature.

Memory Organization
^^^^^^^^^^^^^^^^^^^

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

Physical Integration
^^^^^^^^^^^^^^^^^^^^

Bus Interface
"""""""""""""

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

Interrupt Output
""""""""""""""""

**Signal:** IRQ_OUT (directly connected to CPU's interrupt input)

**Behavior:** IRQ_OUT is asserted when STATUS is non-zero. The device
asserts the interrupt line when a timer tick occurs. The guest
acknowledges the interrupt by writing 0 to the STATUS register, which
deasserts IRQ_OUT.

**Connection:** Connect to CPU's IRQ input. The timer interrupt is
configured via the ``SYS_TIMER_CONFIG`` syscall through the RIFF protocol.

Timing Characteristics
^^^^^^^^^^^^^^^^^^^^^^

Synchronous Semihosting Operation
"""""""""""""""""""""""""""""""""

Semihosting requests are processed **synchronously**:

1. Guest writes RIFF buffer address to RIFF_PTR
2. Guest writes to DOORBELL (1 bus cycle)
3. Device processes request immediately
4. Device writes RETN chunk to guest RAM
5. DOORBELL write returns - response is ready

**Key insight:** When the DOORBELL write completes, the response is
already in the RIFF buffer. No polling is needed.

**Processing time:** Depends on syscall type:

- ``SYS_WRITEC``: Microseconds (write single character)
- ``SYS_WRITE``: Milliseconds (write large buffer to disk)
- ``SYS_OPEN``: Milliseconds (filesystem access)

Timer Interrupts
""""""""""""""""

The device supports a configurable periodic timer via ``SYS_TIMER_CONFIG``:

1. Guest calls ``SYS_TIMER_CONFIG(rate_hz)`` to start timer
2. Timer fires at the specified rate
3. Device sets STATUS = 1 and asserts IRQ_OUT
4. Guest ISR reads STATUS, handles tick, writes 0 to acknowledge
5. Device deasserts IRQ_OUT
6. Timer continues firing until ``SYS_TIMER_CONFIG(0)`` is called

**Note:** Timer interrupts are independent of semihosting requests.
Semihosting remains synchronous - the timer provides a separate
mechanism for periodic interrupts useful for OS development.

Cache Coherency Requirements
""""""""""""""""""""""""""""

**CRITICAL:** If the guest CPU has data cache, proper cache management
is required for correct operation.

**Guest responsibilities:**

1. **Before triggering request:** Flush data cache after writing RIFF
   buffer (before DOORBELL write)
2. **Memory ordering:** Ensure DOORBELL write completes (may need
   memory barrier)
3. **Before reading response:** Invalidate cache before reading RETN
   chunk from buffer

**Why this matters:**

- Device accesses guest RAM directly (DMA or emulator memory access)
- Cached writes may not be visible to device without flush
- Device writes may not be visible to CPU without invalidate
- Failure to manage cache may result in stale data or corruption

**Applies to:** Both synchronous (polling) and asynchronous
(interrupt-driven) operation modes.

RIFF Protocol
-------------

Buffer Location and Ownership
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The RIFF communication buffer is:

- **Allocated by guest** in its own RAM (stack, heap, static data)
- **Owned by guest** (device only accesses on request)
- **Pointed to** via RIFF_PTR register
- **Variable size** (guest allocates sufficient space)

**Recommended minimum size:** 256 bytes (handles most syscalls)

**Typical size:** 1024 bytes (comfortable for all operations)

RIFF Structure Overview
^^^^^^^^^^^^^^^^^^^^^^^

RIFF (Resource Interchange File Format) is a tagged container format.
The same format used by WAV, AVI, and many other standards.

**Basic structure:** The RIFF container begins with the four-character
code 'RIFF', followed by a 4-byte size field, followed by the form type
'SEMI'. Within the container are chunks: CNFG (configuration data),
CALL (syscall data), RETN (pre-allocated by guest for return data),
and ERRO (pre-allocated by guest for error responses).

**RIFF compliance:**

- Chunk IDs: 4-byte ASCII codes ('RIFF', 'CNFG', etc.)
- Sizes: 32-bit **little-endian** (RIFF standard requirement)
- **Chunk size field:** Contains the size of the chunk data only,
  **excluding** the 8-byte chunk header (4-byte ID + 4-byte size)
- Padding: Chunks padded to even byte boundary if odd size
  (padding byte not included in chunk_size field)
- Form type: 'SEMI' (semihosting)

Endianness Handling
^^^^^^^^^^^^^^^^^^^

**Critical distinction:**

1. **RIFF structure** (headers, chunk IDs, sizes): **Always little-endian**
   per RIFF specification
2. **Data values** (syscall arguments, pointers, return values):
   **Guest's native endianness** as declared in CNFG chunk

**Why this works:**

- RIFF is a Microsoft/Intel creation (little-endian heritage)
- Keeping RIFF headers LE ensures compatibility and simplicity
- Guest declares its endianness in CNFG chunk
- Host interprets data values using declared endianness

**Implementation note for big-endian guests:** Guest must swap multi-byte
values in RIFF headers (sizes) but NOT in chunk data. Big-endian systems
will need helper functions to write little-endian values for RIFF structure
fields while using native byte order for data values within chunks.

Chunk Nesting and Organization
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**RIFF hierarchy for semihosting:** The RIFF container with form type
'SEMI' contains: CNFG (leaf chunk for configuration), CALL (container
chunk for request, which may contain PARM leaf chunks for parameters
and DATA leaf chunks for buffers/strings), RETN (container chunk
pre-allocated by guest for response, which may contain DATA leaf chunks
for read data), and ERRO (leaf chunk pre-allocated by guest for error
responses).

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

**Parser requirements:**

- Parsers MUST NOT assume any particular chunk order within the RIFF container
- Parsers MUST locate chunks by iterating through the RIFF structure using
  chunk IDs and sizes
- Parsers MUST NOT read or write at hardcoded offsets -- all access must be
  based on parsed chunk locations
- Parsers MUST NOT write any response data until the entire request has been
  successfully parsed and validated
- If parsing fails or required chunks (RETN, ERRO) are missing, the parser
  MUST NOT write to guest memory
- Unknown chunk types MUST be skipped (using their size field), not rejected --
  this enables forward compatibility

CNFG Chunk -- Configuration
^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Purpose:** Declares guest CPU architecture parameters.

**Chunk ID:** 'CNFG' (ASCII codes 0x43 0x4E 0x46 0x47)

**Format:** The CNFG chunk contains a 4-byte chunk ID, a 4-byte little-endian
chunk size (value 4), followed by four data bytes: int_size (bytes per
integer), ptr_size (bytes per pointer, may differ from int size), endianness
(0=LE, 1=BE, 2=PDP), and one reserved byte (must be 0x00).

**Total chunk size:** 12 bytes (8 byte header + 4 byte data)

**Field definitions:**

**int_size:** Size in bytes of the natural integer type for the architecture.
Represents the size of C ``int`` or equivalent default integer type.

- 6502 (LLVM-MOS): 2 bytes (16-bit int)
- ARM Cortex-M: 4 bytes (32-bit int)
- x86-64: 4 bytes (32-bit int, even on 64-bit platforms)
- AVR: 2 bytes (16-bit int)

**ptr_size:** Size in bytes of pointer types for the architecture.
Represents the size of C pointer types (``void*``, ``char*``). Equals the
address bus width.

- 6502: 2 bytes (16-bit addressing)
- ARM Cortex-M: 4 bytes (32-bit addressing)
- x86-64: 8 bytes (64-bit addressing)
- AVR: 2 bytes (16-bit addressing)

**When int_size ≠ ptr_size:**

- Some DSPs have 16-bit data (int_size=2) but 24-bit code pointers (ptr_size=3)
- x86-64 has 32-bit int (int_size=4) but 64-bit pointers (ptr_size=8)
- This distinction allows the protocol to correctly size both scalar values
  and addresses

**Endianness values:**

- ``0`` - Little Endian (LSB first): x86, ARM Cortex-M, RISC-V (typically)
- ``1`` - Big Endian (MSB first): 68000, SPARC, PowerPC (classic mode),
  MIPS (BE mode)
- ``2`` - PDP Endian (middle-endian): PDP-11, VAX (historic)
- ``3-255`` - Reserved for future use

**Configuration caching:**

- CNFG chunk should be sent **once** at session start (first RIFF buffer
  after device initialization)
- Device caches int_size, ptr_size, and endianness for the entire session
- Subsequent RIFF buffers **omit** the CNFG chunk and start directly with CALL
- To change configuration, guest must reinitialize the device or send a new
  CNFG chunk
- ``int_size`` != ``ptr_size`` is valid (e.g., 16-bit integers with 24-bit
  pointers on some DSPs)
- Host uses these cached values to correctly interpret all multi-byte values
  in PARM chunks

CALL Chunk -- Syscall Request Container
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Purpose:** Container chunk for a semihosting operation request.

**Chunk ID:** 'CALL' (ASCII codes 0x43 0x41 0x4C 0x4C)

**Format:** The CALL chunk contains a 4-byte chunk ID, a 4-byte little-endian
chunk size (variable), followed by: opcode (1 byte, ARM semihosting syscall
number), three reserved bytes (must be 0x00), and then a variable number of
sub-chunks (PARM and DATA chunks for parameters).

**Opcode:** ARM semihosting syscall number (0x01-0x31, see reference table)
File by file, I'd like you to r
**Sub-chunks:** The CALL chunk contains nested PARM and DATA chunks
representing the syscall parameters. Parameters appear in the order
expected by the syscall.

PARM Chunk -- Parameter Value
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Purpose:** Represents a single scalar parameter (integer, pointer, etc.)
within a CALL chunk.

**Chunk ID:** 'PARM' (ASCII codes 0x50 0x41 0x52 0x4D)

**Format:** The PARM chunk contains a 4-byte chunk ID, a 4-byte little-endian
chunk size (4 + value_size), followed by: param_type (1 byte parameter type
code), three reserved bytes (must be 0x00), and then the parameter value in
guest endianness (variable size).

**Parameter types:**

- ``0x01`` - Integer value (size = int_size from CNFG)

  - Used for: file descriptors, counts, modes, status codes, offsets
  - Represents scalar integer values in the guest's natural integer size

- ``0x02`` - Pointer value (size = ptr_size from CNFG)

  - Used for: memory addresses (when needed, though most use DATA chunks instead)
  - Sized to hold valid addresses in the guest's address space

- ``0x03-0xFF`` - Reserved for future use

**Value field:**

- Size depends on param_type and CNFG settings
- Endianness matches guest's declared endianness
- For type 0x01 (integer): value is int_size bytes
- For type 0x02 (pointer): value is ptr_size bytes

**Note:** Parameters must appear in the order expected by the syscall.

DATA Chunk -- Binary Data or String
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Purpose:** Represents binary data, strings, or buffer contents within
CALL or RETN chunks.

**Chunk ID:** 'DATA' (ASCII codes 0x44 0x41 0x54 0x41)

**Format:** The DATA chunk contains a 4-byte chunk ID, a 4-byte little-endian
chunk size (4 + payload_length), followed by: data_type (1 byte data type
code), three reserved bytes (must be 0x00), and then the payload (actual
data bytes, variable length).

**Data types:**

- ``0x01`` - Binary data (arbitrary bytes)
- ``0x02`` - String (null-terminated ASCII/UTF-8)
- ``0x03-0xFF`` - Reserved for future use

**Payload:** Variable-length data. For strings (type 0x02), includes null
terminator.

**Padding:** If payload length is odd, chunk is padded to even byte boundary
per RIFF specification.

**Usage:**

- In CALL chunks: Contains data to write (SYS_WRITE), filenames (SYS_OPEN),
  command strings (SYS_SYSTEM)
- In RETN chunks: Contains data read back (SYS_READ, SYS_READC)

RETN Chunk -- Return Value and Data
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Purpose:** Pre-allocated container for device response containing syscall
result and optional data.

**Chunk ID:** 'RETN' (ASCII codes 0x52 0x45 0x54 0x4E)

**Format:** The RETN chunk contains a 4-byte chunk ID, a 4-byte little-endian
chunk size (set by guest at allocation time), followed by space for: result
(int_size bytes, syscall return value in guest endianness), errno (4 bytes,
POSIX errno in little-endian, 0=success), and optional sub-chunks (DATA
chunks for read operations).

**Guest allocation:** The guest **must** pre-allocate the RETN chunk with
sufficient space for the expected response:

- Minimum size: ``int_size + 4`` bytes (result + errno) for syscalls with
  no data return
- For SYS_READ: ``int_size + 4 + 12 + requested_bytes`` (result + errno +
  DATA chunk header + read data)
- For other data-returning syscalls: sized according to maximum expected
  response

**Result field:**

- Size equals ``int_size`` from CNFG
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

**Sub-chunks:** For syscalls that return data (SYS_READ, SYS_READC), the
device writes a DATA sub-chunk within RETN containing the actual bytes read.
The DATA chunk size reflects the actual bytes returned, which may be less
than the allocated space (e.g., on EOF).

**Device behavior:** Device writes response data **within** the pre-allocated
RETN chunk bounds. The device never modifies the RIFF structure -- only the
contents of RETN. If the response would exceed the allocated RETN size, the
device writes an error to the ERRO chunk instead.

ERRO Chunk -- Error Response
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Purpose:** Pre-allocated container for device error response when RIFF
structure is malformed or request cannot be processed.

**Chunk ID:** 'ERRO' (ASCII codes 0x45 0x52 0x52 0x4F)

**Format:** The ERRO chunk contains a 4-byte chunk ID, a 4-byte little-endian
chunk size (set by guest at allocation time), followed by space for:
error_code (2 bytes in little-endian), two reserved bytes (must be 0x00),
and an optional ASCII error message (variable length).

**Guest allocation:** The guest **must** pre-allocate the ERRO chunk with
sufficient space for error responses:

- Minimum size: 4 bytes (error_code + reserved)
- Recommended size: 64 bytes (allows for error message)

**Error codes:**

- ``0x01`` - Invalid chunk structure (incorrect nesting, wrong chunk sizes,
  or unrecognized chunk types)
- ``0x02`` - Malformed RIFF format (missing 'RIFF' signature, missing 'SEMI'
  form type, or size field errors)
- ``0x03`` - Missing CNFG chunk (first request after device initialization
  must include CNFG)
- ``0x04`` - Unsupported opcode (syscall number not implemented by this device)
- ``0x05`` - Invalid parameter count (wrong number of PARM or DATA chunks
  for the specified syscall)
- ``0x06`` - Missing RETN chunk (guest must pre-allocate RETN)
- ``0x07`` - Missing ERRO chunk (guest must pre-allocate ERRO)
- ``0x08`` - RETN too small (pre-allocated RETN cannot hold response)
- ``0x09-0xFFFF`` - Reserved for future use

**Device behavior:** Device writes error data **within** the pre-allocated
ERRO chunk bounds when it cannot parse or execute the request. The device
never modifies the RIFF structure -- only the contents of ERRO. If ERRO
itself is missing or too small, the device cannot report the error.

Operation Mode
--------------

Synchronous Semihosting
^^^^^^^^^^^^^^^^^^^^^^^

Semihosting operates **synchronously**. When the DOORBELL write completes,
the response is already in the RIFF buffer.

**Operation:**

1. Guest builds RIFF buffer with CALL chunk (containing PARM/DATA sub-chunks),
   pre-allocated RETN chunk, and pre-allocated ERRO chunk
2. Guest writes RIFF buffer address to RIFF_PTR register
3. Guest writes to DOORBELL register to trigger processing
4. Response is ready immediately - no polling needed
5. Guest reads response from RETN chunk (or ERRO chunk if error occurred)

**Note:** See `Cache Coherency Requirements`_ in Hardware Architecture section
for critical cache management details.

**Characteristics:**

- Simple to implement
- No polling or interrupt handler needed for semihosting
- Suitable for all use cases from simple tests to OS development

**Concurrent request handling:**

Guest MUST NOT write DOORBELL while a previous request is being processed.
Device behavior is undefined if concurrent requests are issued.

Guest Memory Access by Device
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Critical detail:** The device accesses guest RAM to read/write RIFF buffers.

**Physical hardware:** Device is a bus master, can initiate DMA reads/writes
to guest memory.

**Virtual hardware:** Emulator directly accesses guest memory structures
(memory array, MMU translations, etc.).

**Implications:**

1. **Cache coherency:** Guest CPU MUST flush data cache after writing RIFF
   buffer and invalidate before reading RETN chunk (see warnings above)
2. **MMU/Virtual addressing:** See `Address Interpretation`_ subsection below
3. **Access permissions:** Device ignores memory protection (privileged access)

**Physical device with DMA operation:** Guest writes RIFF_PTR with the buffer
address, then writes DOORBELL. The device bus master logic reads the RIFF
header from guest RAM, reads the CNFG chunk (first time only), reads the
CALL chunk header and recursively reads PARM and DATA sub-chunks, executes
the syscall, writes the RETN chunk to guest RAM (replacing CALL), and for
SYS_READ appends a DATA sub-chunk with read data. Finally, the device sets
STATUS bit 0 (RESPONSE_READY).

Address Interpretation
^^^^^^^^^^^^^^^^^^^^^^

The RIFF_PTR register and pointer values in PARM chunks contain guest memory
addresses. How these are interpreted depends on the implementation:

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

  - Manual endianness configuration (DIP switches, compile-time constants,
    configuration registers)
  - Cannot auto-detect guest byte order

**Recommendation:** Guest software should allocate RIFF buffers in physical
memory or identity-mapped regions when possible for maximum compatibility
with both virtual and physical devices.

ARM Semihosting Compatibility
-----------------------------

Syscall Numbers
^^^^^^^^^^^^^^^

This protocol uses the **ARM Semihosting specification** syscall numbers
for maximum compatibility with existing toolchains (GCC, Clang, picolibc,
newlib).

.. list-table::
   :header-rows: 1
   :widths: 10 20 35 35

   * - Opcode
     - Name
     - Arguments
     - Return Value
   * - 0x01
     - SYS_OPEN
     - (filename, mode, length)
     - fd or -1
   * - 0x02
     - SYS_CLOSE
     - (fd)
     - 0 or -1
   * - 0x03
     - SYS_WRITEC
     - (char_ptr)
     - \-
   * - 0x04
     - SYS_WRITE0
     - (string)
     - \-
   * - 0x05
     - SYS_WRITE
     - (fd, data, length)
     - bytes NOT written (0 = success)
   * - 0x06
     - SYS_READ
     - (fd, length)
     - bytes NOT read (0 = complete)
   * - 0x07
     - SYS_READC
     - ()
     - character (0-255)
   * - 0x08
     - SYS_ISERROR
     - (status)
     - error code or 0
   * - 0x09
     - SYS_ISTTY
     - (fd)
     - 1 if TTY, 0 otherwise
   * - 0x0A
     - SYS_SEEK
     - (fd, position)
     - 0 or -1
   * - 0x0C
     - SYS_FLEN
     - (fd)
     - file length or -1
   * - 0x0D
     - SYS_TMPNAM
     - (id, length)
     - temp filename or -1
   * - 0x0E
     - SYS_REMOVE
     - (filename, length)
     - 0 or -1
   * - 0x0F
     - SYS_RENAME
     - (old_name, old_len, new_name, new_len)
     - 0 or -1
   * - 0x10
     - SYS_CLOCK
     - ()
     - centiseconds since start
   * - 0x11
     - SYS_TIME
     - ()
     - seconds since epoch
   * - 0x12
     - SYS_SYSTEM
     - (command, length)
     - exit code
   * - 0x13
     - SYS_ERRNO
     - ()
     - last errno value
   * - 0x15
     - SYS_GET_CMDLINE
     - (length)
     - command line or -1
   * - 0x16
     - SYS_HEAPINFO
     - ()
     - 0, PARM×4 (heap_base, heap_limit, stack_base, stack_limit)
   * - 0x18
     - SYS_EXIT
     - (status)
     - no return
   * - 0x20
     - SYS_EXIT_EXTENDED
     - (exception, subcode)
     - no return
   * - 0x30
     - SYS_ELAPSED
     - ()
     - tick count or DATA(8 bytes)
   * - 0x31
     - SYS_TICKFREQ
     - ()
     - ticks per second
   * - 0x32
     - SYS_TIMER_CONFIG
     - (rate_hz)
     - 0 or error code

**Note:** SYS_ELAPSED and SYS_HEAPINFO have special return value encoding.
See `Special Return Value Encoding`_ for details.

SYS_TIMER_CONFIG (0x32)
^^^^^^^^^^^^^^^^^^^^^^^

Configure a periodic timer interrupt.

**CALL chunk:**

- PARM chunk: rate_hz (uint32) - Timer frequency in Hz

**RETN chunk:**

- result: 0 on success, negative on error
- errno: 0 or error code

**Behavior:**

- When rate > 0: Start periodic timer at specified frequency. On each tick,
  device sets STATUS = 1 and asserts CPU IRQ line.
- When rate = 0: Stop timer, no more interrupts.
- Calling again with a different rate changes the frequency.

**Return values:**

- 0: Timer configured successfully
- -1 with errno = ENOTSUP: CPU does not support interrupts
- -1 with errno = EINVAL: Rate not achievable (too fast or too slow)

**Common rates:**

- 50 Hz: Retro/8-bit systems, PAL frame sync
- 60 Hz: NTSC frame sync
- 100 Hz: Embedded systems, older Linux (HZ=100)
- 1000 Hz: Modern Linux, RTOS (HZ=1000)

**Note:** Software implementations can be permissive and accept any reasonable
rate. Physical implementations (FPGA/ASIC) may return errors for frequencies
they cannot generate from their clock source.

Argument Encoding in RIFF Chunks
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

ARM semihosting syscalls are mapped to PARM and DATA chunks:

**Scalar arguments** (fd, mode, length, status, etc.) → PARM chunks
(type 0x01 = integer)

**String/buffer arguments** (filenames, data to write, commands) → DATA chunks

**Buffer output** (data read, command line) → DATA chunks in RETN

**Example: SYS_WRITE(fd=0x01, data="Hello\\n", length=0x06)**

CALL chunk contains:

- PARM chunk: fd = 0x01
- DATA chunk: payload = "Hello\\n"
- PARM chunk: length = 0x06

**Example: SYS_READ(fd=0x03, length=0x100)**

CALL chunk contains:

- PARM chunk: fd = 0x03
- PARM chunk: length = 0x100

RETN chunk contains:

- result = bytes_read
- errno = 0
- DATA chunk: payload = bytes read

**Example: SYS_OPEN(filename="/tmp/test.txt", mode=0x00, length=0x0E)**

CALL chunk contains:

- DATA chunk: payload = "/tmp/test.txt\\0"
- PARM chunk: mode = 0x00
- PARM chunk: length = 0x0E

Special Return Value Encoding
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Some syscalls return values that may exceed the guest's natural integer size.
These use DATA chunks in the RETN response.

SYS_ELAPSED (0x30) -- 64-bit Tick Count
"""""""""""""""""""""""""""""""""""""""

**Purpose:** Returns elapsed tick count since an arbitrary epoch.

**CALL chunk:** No PARM or DATA sub-chunks required.

**RETN chunk:**

- If ``int_size >= 8``: Result field contains the 64-bit tick count directly
- If ``int_size < 8``: Result field is 0, and a DATA sub-chunk contains
  the 64-bit value

**DATA chunk format (when int_size < 8):**

- data_type: 0x01 (binary)
- payload: 8 bytes, **little-endian** (regardless of guest endianness)
- Byte 0-3: Low 32 bits of tick count
- Byte 4-7: High 32 bits of tick count

**Rationale:** 64-bit tick counts cannot fit in 16-bit or 32-bit result
fields. Using a DATA chunk with fixed little-endian encoding ensures
consistent interpretation across all architectures.

**Example for 16-bit guest (int_size=2):**

::

   RETN chunk:
     result = 0x0000 (2 bytes, guest endianness)
     errno = 0x00000000 (4 bytes, little-endian)
     DATA sub-chunk:
       type = 0x01 (binary)
       payload = 0x78 0x56 0x34 0x12 0xEF 0xCD 0xAB 0x00
                 (represents tick count 0x00ABCDEF12345678)

SYS_HEAPINFO (0x16) -- Memory Layout Information
"""""""""""""""""""""""""""""""""""""""""""""""""

**Purpose:** Returns heap and stack boundary addresses.

**CALL chunk:** No PARM or DATA sub-chunks required.

**RETN chunk:**

- Result field: 0 on success, -1 on error
- Four PARM sub-chunks (type 0x02 = pointer), in order:

.. list-table::
   :header-rows: 1
   :widths: 10 20 70

   * - Order
     - Field
     - Description
   * - 1
     - heap_base
     - Start of heap region
   * - 2
     - heap_limit
     - End of heap region (exclusive)
   * - 3
     - stack_base
     - Bottom of stack (lowest address)
   * - 4
     - stack_limit
     - Top of stack (highest address)

**Rationale:** Using individual PARM chunks for each pointer (rather than
a binary blob) maintains the self-describing nature of the RIFF protocol.
Each pointer is explicitly typed and sized according to the CNFG ptr_size.

**Example for 32-bit guest (ptr_size=4, little-endian):**

::

   RETN chunk:
     result = 0x00000000 (4 bytes, little-endian)
     errno = 0x00000000 (4 bytes, little-endian)
     PARM sub-chunk:
       type = 0x02 (pointer)
       value = 0x00 0x10 0x00 0x20   (heap_base = 0x20001000)
     PARM sub-chunk:
       type = 0x02 (pointer)
       value = 0x00 0x00 0x01 0x20   (heap_limit = 0x20010000)
     PARM sub-chunk:
       type = 0x02 (pointer)
       value = 0x00 0x00 0x02 0x20   (stack_base = 0x20020000)
     PARM sub-chunk:
       type = 0x02 (pointer)
       value = 0x00 0xF0 0x02 0x20   (stack_limit = 0x2002F000)

Open Mode Flags (SYS_OPEN)
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 10 25 65

   * - Value
     - Name
     - Description
   * - 0
     - SH_OPEN_R
     - Read-only
   * - 1
     - SH_OPEN_RB
     - Read-only binary
   * - 2
     - SH_OPEN_R_PLUS
     - Read-write
   * - 3
     - SH_OPEN_R_PLUS_B
     - Read-write binary
   * - 4
     - SH_OPEN_W
     - Write, truncate/create
   * - 5
     - SH_OPEN_WB
     - Write binary, truncate/create
   * - 6
     - SH_OPEN_W_PLUS
     - Read-write, truncate/create
   * - 7
     - SH_OPEN_W_PLUS_B
     - Read-write binary, truncate/create
   * - 8
     - SH_OPEN_A
     - Append, create if needed
   * - 9
     - SH_OPEN_AB
     - Append binary, create if needed
   * - 10
     - SH_OPEN_A_PLUS
     - Read-append, create if needed
   * - 11
     - SH_OPEN_A_PLUS_B
     - Read-append binary, create if needed

Future Extensions
-----------------

Design for Extensibility
^^^^^^^^^^^^^^^^^^^^^^^^

RIFF's chunk-based structure enables backward-compatible extensions:

- **Unknown chunks:** Old implementations skip via chunk_size
- **New chunk types:** Don't break existing implementations
- **Version negotiation:** Via extended CNFG fields or new META chunk
- **Optional features:** Advertised via capability bits

Potential Future Chunks
^^^^^^^^^^^^^^^^^^^^^^^

STRM -- Streaming Data
""""""""""""""""""""""

For large file transfers without copying entire buffer. The STRM chunk
contains a chunk ID, chunk size, a call_id field (4 bytes, associates with
CALL), sequence number (4 bytes, packet sequence), flags (2 bytes: 0x01=more,
0x02=end), and variable-length data.

**Use case:** Reading 100MB file in 4KB chunks.

EVNT -- Host-Initiated Events
"""""""""""""""""""""""""""""

For host to signal guest asynchronously. The EVNT chunk contains a chunk ID,
chunk size, an event_type field (2 bytes: 0x01=signal, 0x02=timer,
0x03=host_message), and a variable-length payload.

**Use case:** Host sends Ctrl-C signal to guest application.

ABRT -- Abort Operation
"""""""""""""""""""""""

Cancel pending async operation. The ABRT chunk contains a chunk ID, chunk
size, a call_id field (4 bytes), and a reason code (2 bytes: 0x01=user,
0x02=timeout, 0x03=error).

**Use case:** Cancel slow file operation.

META -- Device Capabilities
"""""""""""""""""""""""""""

Query device features. The META chunk contains a chunk ID, chunk size, a
query_type field (2 bytes: 0x01=version, 0x02=features, 0x03=limits), and
query-specific data (variable length). The device responds with a META chunk
containing a variable-length response with the requested capabilities.

**Use case:** Guest queries max buffer size, supported syscalls, protocol
version.

Extended Configuration
^^^^^^^^^^^^^^^^^^^^^^

**CNFG chunk version 2:** Future versions of the CNFG chunk may include
additional fields: protocol_version (1 byte: 0x01 for v1, 0x02 for v2),
feature_flags (4 bytes, capability bitmap), and extensions (8 bytes
reserved for future use).

**Feature flags bitmap:**

- Bit 0: ASYNC_OPS - Supports asynchronous operations
- Bit 1: STREAMING - Supports STRM chunks
- Bit 2: HOST_EVENTS - Supports EVNT chunks
- Bit 3: LARGE_FILES - Supports 64-bit file offsets
- Bit 4: EXTENDED_ERRNO - Supports extended error codes
- Bits 5-31: Reserved

Asynchronous Operations
^^^^^^^^^^^^^^^^^^^^^^^

**Multi-request support:** Future versions may add a call_id field to the
CALL chunk, allowing the guest to submit multiple CALL chunks with different
call_ids. The device would process requests in background and return RETN
chunks when ready, with the guest matching RETN to CALL via call_id.

**Buffer management:** For async operations, the guest would allocate multiple
RIFF buffers, assign each buffer a unique call_id, track pending operations,
and dispatch IRQ handlers based on call_id.

Reference Tables
----------------

Register Map Summary
^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 10 10 15 10 55

   * - Offset
     - Size
     - Name
     - Type
     - Description
   * - 0x00
     - 8
     - SIGNATURE
     - R
     - ASCII "SEMIHOST" (device identification)
   * - 0x08
     - 16
     - RIFF_PTR
     - RW
     - Guest RAM pointer to RIFF buffer
   * - 0x18
     - 1
     - DOORBELL
     - W
     - Write to trigger request
   * - 0x19
     - 1
     - STATUS
     - RW
     - Interrupt pending (write 0 to clear)
   * - 0x1A
     - 6
     - RESERVED
     - \-
     - Future use

STATUS Register Values
^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 10 25 65

   * - Value
     - Name
     - Description
   * - 0
     - ZBC_STATUS_NONE
     - No interrupt pending
   * - 1
     - ZBC_STATUS_TIMER
     - Timer tick occurred
   * - 2+
     - (reserved)
     - Reserved for future interrupt sources

RIFF Chunk Types
^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 10 15 15 15 45

   * - FourCC
     - Name
     - Direction
     - Size
     - Description
   * - CNFG
     - Configuration
     - Guest→Device
     - 12 bytes
     - Architecture parameters
   * - CALL
     - Call (container)
     - Guest→Device
     - Variable
     - Syscall request with sub-chunks
   * - PARM
     - Parameter
     - Guest→Device
     - 12+value_size
     - Scalar parameter (in CALL)
   * - DATA
     - Data
     - Bidirectional
     - 12+payload_size
     - Binary/string data
   * - RETN
     - Return (container)
     - Guest (alloc) ← Device (fill)
     - Variable
     - Pre-allocated by guest, filled by device
   * - ERRO
     - Error
     - Guest (alloc) ← Device (fill)
     - Variable
     - Pre-allocated by guest, filled on error
   * - EVNT
     - Event
     - Device→Guest
     - Variable
     - Async events (future)
   * - ABRT
     - Abort
     - Bidirectional
     - Variable
     - Cancel operation (future)
   * - META
     - Metadata
     - Bidirectional
     - Variable
     - Capabilities (future)

PARM Chunk Parameter Types
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 15 20 20 45

   * - Value
     - Name
     - Size
     - Description
   * - 0x01
     - Integer
     - int_size bytes
     - Integer/scalar value in guest endianness
   * - 0x02
     - Pointer
     - ptr_size bytes
     - Pointer value in guest endianness
   * - 0x03-0xFF
     - Reserved
     - \-
     - Reserved for future use

DATA Chunk Data Types
^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 15 20 65

   * - Value
     - Name
     - Description
   * - 0x01
     - Binary
     - Arbitrary binary data
   * - 0x02
     - String
     - Null-terminated ASCII/UTF-8 string
   * - 0x03-0xFF
     - Reserved
     - Reserved for future use

ERRO Chunk Error Codes
^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 15 30 55

   * - Value
     - Name
     - Description
   * - 0x01
     - Invalid chunk structure
     - Malformed chunk headers or nesting
   * - 0x02
     - Malformed RIFF format
     - Invalid RIFF signature or structure
   * - 0x03
     - Missing CNFG chunk
     - CNFG required but not sent
   * - 0x04
     - Unsupported opcode
     - Syscall number not implemented
   * - 0x05
     - Invalid parameter count
     - Wrong number of PARM/DATA chunks
   * - 0x06
     - Missing RETN chunk
     - Guest must pre-allocate RETN
   * - 0x07
     - Missing ERRO chunk
     - Guest must pre-allocate ERRO
   * - 0x08
     - RETN too small
     - Pre-allocated RETN cannot hold response
   * - 0x09-0xFFFF
     - Reserved
     - Reserved for future use

Endianness Encoding
^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 10 20 20 50

   * - Value
     - Name
     - Byte Order
     - Example CPUs
   * - 0x00
     - Little Endian
     - LSB first
     - x86, ARM, RISC-V
   * - 0x01
     - Big Endian
     - MSB first
     - 68000, SPARC, PowerPC
   * - 0x02
     - PDP Endian
     - Middle
     - PDP-11, VAX
