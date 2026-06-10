# Zero Board Computer Wiki - Complete Page Outline

This document shows the detailed content outline for all 48 wiki pages.

---

## Section 1: Foundation (5 pages)

### 1. Main Page
**Purpose**: Landing page and navigation hub

**Content**:
- **Header**: "Welcome to Zero Board Computer"
- **What is ZBC?** (2-3 paragraphs)
  - Platform-agnostic system specification
  - Minimal hardware for compiler/debugger/library bring-up
  - Works on any CPU architecture
- **Key Features** (bullet list)
  - CPU-agnostic design
  - Minimal device drivers needed
  - Full libc support via semihosting
  - Standardized, deterministic environment
- **Getting Started** (quick links)
  - Link to "What is Zero Board Computer?"
  - Link to "Getting Started"
  - Link to "Design Goals and Use Cases"
- **Documentation Sections** (navigation boxes)
  - ZBC System Architecture
  - Semihosting Protocol
  - Implementation Examples
  - User Documentation
  - Reference Materials
  - Developer Documentation
- **News/Updates** (placeholder section)

**Source**: Original content
**Categories**: None (main page)

---

### 2. What is Zero Board Computer?
**Purpose**: Comprehensive introduction to ZBC

**Content**:
- **Overview**
  - ZBC is a hardware/system specification
  - Not tied to MAME or any specific implementation
  - Can be built in hardware (FPGA/ASIC) or software (emulators)
- **The Problem**
  - Bringing up compilers, debuggers on new CPU architectures is hard
  - Requires complex OS and driver infrastructure
  - Testing across many architectures is inconsistent
- **The Solution**
  - Minimal, standardized hardware specification
  - Only two device drivers needed: display + semihosting
  - Immediate access to full libc (file I/O, timing, console)
- **System Components**
  - CPU (any architecture)
  - RAM (sized based on address space)
  - MC6847 Text Display (32x16 characters)
  - Semihosting Peripheral (memory-mapped I/O)
- **Use Cases**
  - Compiler bring-up and testing
  - Debugger development
  - Library porting (libc, runtime)
  - CPU emulator validation
  - Educational platforms
- **Design Philosophy**
  - Simplicity: minimal hardware
  - Universality: works on any CPU
  - Determinism: predictable, repeatable environment
  - Standardization: consistent across implementations

**Source**: zbc.rst sections 1.1, 1.2, 1.3 + original content
**Categories**: Overview

---

### 3. Design Goals and Use Cases
**Purpose**: Explain why ZBC exists and what it's for

**Content**:
- **Primary Goals**
  - Provide consistent test environment across CPU architectures
  - Minimize driver complexity for new platforms
  - Enable immediate access to host services
  - Support automation and tooling
- **Compiler Bring-Up**
  - Test code generation for new targets
  - Validate optimizer behavior
  - Debug runtime library issues
  - No OS or complex drivers required
- **Debugger Development**
  - Test debugger features on various architectures
  - Controlled environment for debugging debuggers
  - Consistent behavior across platforms
- **Library Porting**
  - Test libc implementations
  - Validate runtime behavior
  - File I/O, memory, timing all available via semihosting
- **CPU Emulator Validation**
  - MAME's use case: test all CPU emulations
  - Standardized test harness
  - Automated testing across hundreds of CPUs
- **Education**
  - Learn assembly/systems programming
  - Simple enough for students
  - Immediate feedback via display and I/O
- **Comparison with Alternatives**
  - vs. Full OS (Linux, bare metal): Too complex, hard to debug
  - vs. QEMU system mode: Architecture-specific, heavy
  - vs. Hardware boards: Expensive, inconsistent, limited availability
  - ZBC: Minimal, universal, free, deterministic

**Source**: zbc.rst section 1.2 + original content
**Categories**: Overview

---

### 4. Getting Started
**Purpose**: First steps for using ZBC

**Content**:
- **Prerequisites**
  - MAME installation (reference implementation)
  - OR other ZBC-compatible emulator/hardware
  - Toolchain for target CPU (gcc, clang, llvm-mos, etc.)
- **Quick Start with MAME**
  - Install MAME
  - List available ZBC systems: `mame -listfull zbc*`
  - Run a ZBC system: `mame zbcz80`
  - Observe boot screen with system info
- **Loading Your First Program**
  - Create simple binary (e.g., infinite loop)
  - Load via quickload: `mame zbcz80 -quik program.bin`
  - Watch execution on display
- **Understanding the Boot Screen**
  - CPU name and type
  - Load address (where program starts)
  - Available RAM size
  - Semihosting buffer address
  - Video RAM address
- **Next Steps**
  - Read [[System Overview]] to understand architecture
  - Learn about [[Memory Layout and Addressing]]
  - Explore [[Semihosting Overview]] for I/O capabilities
  - See [[Writing Programs for ZBC]] for development

**Source**: zbc.rst section 8.1 + original content
**Categories**: Overview, User Guides

---

### 5. Key Concepts
**Purpose**: Essential terminology and concepts

**Content**:
- **Platform-Agnostic Design**
  - ZBC specification is independent of implementation
  - Same interface on hardware or emulator
  - Same behavior across all CPUs
- **Dynamic Memory Layout**
  - Memory map calculated based on CPU address width
  - Scales from 8-bit to 128-bit (and beyond)
  - Formula: reserved_start = 2^addr_bits - 2^(addr_bits/2)
  - Peripherals placed at high memory
- **Template-Based Architecture** (MAME implementation)
  - Single C++ template instantiated for each CPU
  - CPU-specific code via template specialization
  - Eliminates code duplication
- **Quickload Mechanism**
  - Load raw binary programs into memory
  - No file format, headers, or loaders needed
  - Program execution begins at load address
  - Load address calculated automatically or specified
- **Semihosting**
  - Memory-mapped peripheral providing host services
  - File I/O, console I/O, timing, system calls
  - Based on ARM semihosting syscall numbers
  - RIFF protocol for architecture-agnostic operation
- **RIFF Protocol**
  - Tagged container format (like WAV, AVI)
  - Self-describing: guest declares its architecture
  - Works with any word size, endianness
  - Chunks: CNFG (config), CALL (request), RETN (response)
- **MC6847 VDG**
  - Text display controller (32x16 characters)
  - Video RAM at top of address space
  - Simple memory-mapped interface
  - Console driver with scrolling, word wrap

**Source**: zbc.rst sections 1.3, 2.2, 2.4 + semihost.md section 1
**Categories**: Overview

---

## Section 2: ZBC System Architecture (6 pages)

### 6. System Overview
**Purpose**: High-level architecture of ZBC

**Content**:
- **Hardware Components**
  - CPU: Any architecture (8-bit to 128-bit)
  - RAM: Sized based on address space
  - MC6847 VDG: Text display (512 bytes VRAM)
  - Semihosting Device: I/O peripheral (32-byte registers)
- **Memory Organization**
  - Low memory: Vectors, stack, program code
  - Main RAM: Available for programs
  - High memory: Semihosting buffer, VRAM, reserved region
  - Dynamic layout based on address space width
- **Bus Architecture**
  - Standard memory-mapped I/O
  - CPU reads/writes to device registers
  - Device accesses guest RAM for semihosting buffers
- **Interrupt System**
  - Optional VSync interrupt from MC6847
  - Configurable via JP1 jumper: Disabled, IRQ, or NMI
  - Optional semihosting completion interrupt
- **Boot Sequence**
  1. CPU reset
  2. CPU-specific initialization (reset vectors, exception tables)
  3. Display boot screen with system info
  4. Jump to load address (or idle loop if no program)
  5. Program execution begins
- **Design Variations**
  - Core specification: CPU + RAM + Display + Semihosting
  - Optional: Additional devices can be added
  - Minimum compliance: Must support core components

**Source**: zbc.rst sections 2.1, 2.2
**Categories**: Architecture

---

### 7. Memory Layout and Addressing
**Purpose**: Detailed memory organization

**Content**:
- **Dynamic Layout Algorithm**
  - Formula: `reserved_start = 2^addr_bits - 2^(addr_bits/2)`
  - VRAM address: `reserved_start - 512`
  - Semihosting address: `reserved_start - 1536`
  - Available RAM: `load_addr` to `semihost_addr - 1`
- **16-bit CPU Example (Z80, 6502)**
  - Address space: 64KB
  - Reserved start: 0xFF00 (256 bytes)
  - VRAM: 0xFE00-0xFEFF (512 bytes)
  - Semihosting: 0xFC00-0xFDFF (1024 bytes)
  - Available RAM: 0x0200-0xFBFF (63,488 bytes)
  - Memory map diagram (text)
- **32-bit CPU Example (68000, ARM, i386)**
  - Address space: 4GB
  - Reserved start: 0xFFFF0000 (64KB)
  - VRAM: 0xFFFFFE00-0xFFFFFFFF (512 bytes)
  - Semihosting: 0xFFFFFA00-0xFFFFFDFF (1024 bytes)
  - Available RAM: 0x00000200-0xFFFFF9FF (~4GB)
  - Memory map diagram (text)
- **64-bit CPU Example**
  - Address space: 16 exabytes
  - Reserved start: 0xFFFFFFFF00000000 (4GB)
  - VRAM: 0xFFFFFFFFFFFFFE00-0xFFFFFFFFFFFFFFFF
  - Semihosting: 0xFFFFFFFFFFFFFA00-0xFFFFFFFFFFFFFDFF
  - Available RAM: massive
- **Address Space Considerations**
  - CPUs with < 16-bit addressing: Special handling needed
  - CPUs with > 64-bit addressing: Formula still applies
  - Harvard architectures: Code and data spaces separate
  - MMU/Virtual addressing: See [[Operation Modes]]
- **Customization**
  - LOAD_ADDR: Default 0x0200, can override
  - VRAM_ADDR: Default calculated, can override
  - CPU_SPEED: Configurable clock rate

**Source**: zbc.rst section 2.4
**Categories**: Architecture, Reference

---

### 8. Video Display (MC6847)
**Purpose**: Text display subsystem

**Content**:
- **MC6847 VDG Overview**
  - Motorola MC6847 Video Display Generator
  - Text mode: 32 columns × 16 rows
  - Character set: ASCII alphanumerics and symbols
  - Used in TRS-80 Color Computer, Dragon 32/64
- **Video RAM Organization**
  - 512 bytes total (32 × 16 = 512 characters)
  - Memory-mapped at high address (see [[Memory Layout and Addressing]])
  - Direct write: `videoram[y * 32 + x] = character`
  - Row 0 at offset 0, row 15 at offset 480
- **Character Encoding**
  - Standard ASCII values (0x20-0x7F printable)
  - Control codes (0x00-0x1F) may display as graphics
  - No color support (monochrome text)
- **Display Timing**
  - Refresh rate: ~62Hz (PAL timing used by ZBC)
  - VSync signal available for timing/interrupts
  - See [[Interrupt System (JP1 Jumper)]]
- **MC6847Console Driver** (MAME implementation)
  - Software text console layer
  - Functions: putchar, puts, clear, goto_xy
  - Vertical scrolling
  - Word wrapping
  - Line centering
  - Implemented in m6847drv.cpp/h
- **Boot Screen Display**
  - Automatically shown on system startup
  - Shows: CPU name, load address, RAM size, addresses
  - Cleared when program writes to VRAM
- **Programming the Display**
  - Direct memory access: Write to VRAM address
  - Example (16-bit CPU): `char *vram = (char *)0xFE00; vram[0] = 'A';`
  - Row calculation: `offset = row * 32 + column`
  - Scrolling: Copy rows 1-15 to 0-14, clear row 15

**Source**: zbc.rst sections 2.2, 2.4 + MAME source (m6847drv.cpp)
**Categories**: Architecture

---

### 9. Interrupt System (JP1 Jumper)
**Purpose**: VSync interrupt configuration

**Content**:
- **JP1 Jumper Overview**
  - 3-position configuration jumper
  - Routes MC6847 VSync signal to CPU interrupt lines
  - Software-configurable in emulators, physical jumper in hardware
- **Position 1-2: Disabled (Default)**
  - VSync not connected to CPU
  - No periodic interrupts
  - Simplest mode for basic programs
  - Boot screen uses this mode
- **Position 2-3: IRQ (Maskable Interrupt)**
  - VSync drives CPU's IRQ line
  - ~60Hz (NTSC) or ~62Hz (PAL) periodic interrupt
  - CPU can mask/disable via interrupt flag
  - Use case: OS development, cooperative multitasking
  - Requires IRQ handler in software
- **Position 3-4: NMI (Non-Maskable Interrupt)**
  - VSync drives CPU's NMI line
  - Cannot be masked by software
  - Fires unconditionally every frame
  - Use case: Hard real-time systems, preemptive multitasking
  - **Must** have proper NMI handler or system crashes
- **Configuration in MAME**
  - Press TAB → Machine Configuration → JP1: VSync Interrupt
  - Via config file: [zbcz80] section, value 0/1/2
  - Command line: (if supported)
- **Programming Considerations**
  - **Install interrupt handlers** at CPU-specific vectors:
    - Z80 NMI: 0x0066
    - 6502 IRQ: 0xFFFE-0xFFFF
    - 6502 NMI: 0xFFFA-0xFFFB
    - (others: see CPU documentation)
  - **Return from interrupt**: RETI, RETN, RTI (CPU-specific)
  - **Acknowledge interrupts**: If required by CPU architecture
  - **Without handler**: Repeated stack pushes → stack overflow → crash
- **VSync Timing**
  - PAL mode: 312 scanlines/frame, 62.5 Hz
  - Based on 4.433619 MHz crystal
  - Field sync (FS) pulse at frame start
  - Connected via callback to interrupt lines
- **Historical Context**
  - TRS-80 CoCo, Dragon 32/64: MC6847 FS → PIA → CPU IRQ
  - Provided system timer without separate timer chip
  - ZBC: Jumper allows flexibility for different use cases

**Source**: zbc.rst section 2.3
**Categories**: Architecture

---

### 10. CPU Support and Initialization
**Purpose**: CPU-specific boot code and template system

**Content**:
- **Template-Based Architecture** (MAME)
  - Single `zbc_state<CPU_TYPE>` template class
  - Instantiated for each CPU type
  - Compile-time parameters: LOAD_ADDR, CPU_SPEED, VRAM_ADDR
  - Eliminates code duplication across CPUs
- **DEFINE_ZBC Macro** (MAME)
  - Syntax: `DEFINE_ZBC(cpu_class, cpu_type, short_name, display_name, ...)`
  - Example: `DEFINE_ZBC(m6502_device, M6502, m6502, "MOS Technology 6502")`
  - Generates: Derived class, ROM definition, MAME registration
  - Optional parameters override defaults
- **CPU-Specific Initialization**
  - Problem: Different CPUs need different boot code
  - Solution: Template specialization of `init_cpu_for_idle()`
  - Default: No initialization (boot with uninitialized memory)
- **6502 Family Specialization**
  - Reset vector at 0xFFFC-0xFFFD
  - Points to load address (default 0x0200)
  - Idle loop: `JMP $0200` (0x4C 0x00 0x02)
  - Allows system to boot without program loaded
- **Z80 Family Specialization**
  - Boot at address 0x0000
  - Idle loop: `JP load_addr` at 0x0000
  - NMI handler at 0x0066: `RETN` (0xED 0x45)
  - Handles VSync NMI without crashing
- **68000 Family Specialization**
  - Vector table at 0x0000
  - Initial SSP (stack pointer) at 0x0000-0x0003
  - Initial PC (program counter) at 0x0004-0x0007
  - Points to load address
- **Adding New Specializations**
  - Identify CPU boot requirements (reset vectors, exception tables)
  - Add template specialization in zbc.cpp
  - Write idle loop at load address
  - Set up required exception handlers
  - Test with `mame zbcXXXX -validate`
- **CPUs Without Specialization**
  - Boot with uninitialized memory
  - May not execute without loaded program
  - May execute from address 0 (undefined behavior)
  - Check boot screen for memory addresses

**Source**: zbc.rst sections 2.2, 2.5, 6
**Categories**: Architecture, Developer Documentation

---

### 11. Quickload System
**Purpose**: Binary program loading mechanism

**Content**:
- **What is Quickload?**
  - MAME feature for loading raw binary files
  - No file format, headers, or metadata required
  - Binary loaded directly into memory at specified address
  - Execution begins immediately
- **Usage**
  - Command: `mame zbcXXXX -quik program.bin`
  - Alternative: `-quickload program.bin`
  - File can be any size (up to available RAM)
- **Load Address Calculation** (MAME ZBC)
  - Default: Auto-calculated based on address space
  - Formula: `2^(1 + addr_bits/2)`
  - Examples:
    - 16-bit CPU: 2^9 = 0x0200 (512)
    - 24-bit CPU: 2^13 = 0x2000 (8192)
    - 32-bit CPU: 2^17 = 0x20000 (131,072)
  - Can override via LOAD_ADDR template parameter
- **Loading Process**
  1. MAME initializes ZBC system
  2. CPU-specific initialization runs (reset vectors, etc.)
  3. Boot screen displayed
  4. Quickload device detects file
  5. Binary read from host filesystem
  6. Written to guest RAM at load address
  7. VRAM switched to read-write (was read-only for boot screen)
  8. Execution continues (CPU already running)
- **VRAM Handling**
  - Without quickload: VRAM is read-only (protects boot screen)
  - With quickload: VRAM becomes read-write (program can use display)
  - Automatic switching on quickload
- **Program Requirements**
  - Must be compiled/assembled for correct load address
  - Must not exceed available RAM
  - Must handle CPU state (registers, stack, interrupts)
  - Should use position-independent code if possible
- **Creating Loadable Binaries**
  - Compile/assemble with correct load address
  - Extract binary section from ELF/object file
  - Tools: `objcopy -O binary`, linker scripts
  - No headers or metadata in final file
- **Limitations**
  - Single binary load only (no dynamic loading)
  - No relocation (must run at load address)
  - No memory protection
  - No file metadata (size, checksums, etc.)

**Source**: zbc.rst sections 2.4, 8.1, 8.2
**Categories**: Architecture, User Guides

---

## Section 3: Semihosting Protocol (9 pages)

### 12. Semihosting Overview
**Purpose**: Introduction to RIFF-based semihosting

**Content**:
- **What is Semihosting?**
  - Mechanism for embedded/emulated code to access host services
  - File I/O, console I/O, timing, system calls
  - Used during development before drivers are available
  - Originated with ARM debugging tools
- **Traditional Semihosting Problems**
  - Uses trap instructions (BKPT, SVC, EBREAK, etc.)
  - Requires debugger support
  - Architecture-specific implementations
  - Doesn't work in all execution environments
  - Complicates CPU execution model
- **RIFF-Based Semihosting Solution**
  - Memory-mapped I/O device (standard load/store)
  - Completely architecture-agnostic
  - Works on any CPU (8-bit to 128-bit+)
  - No trap instructions or debugger required
  - Self-describing protocol
  - Extensible without breaking compatibility
- **How It Works**
  1. Guest allocates RIFF buffer in RAM
  2. Guest builds RIFF structure describing syscall
  3. Guest writes buffer address to device register
  4. Guest triggers request via doorbell register
  5. Device processes request (accesses host)
  6. Device writes response back to buffer
  7. Guest reads results from buffer
- **Key Benefits**
  - **Universal**: Any word size, any endianness, any CPU
  - **Simple**: Only 32 bytes of device registers
  - **Efficient**: Guest manages buffers (no copying)
  - **Standard**: ARM semihosting syscall numbers
  - **Flexible**: Synchronous polling or asynchronous interrupts
- **ARM Semihosting Compatibility**
  - Uses ARM semihosting syscall numbers
  - SYS_OPEN, SYS_READ, SYS_WRITE, SYS_CLOSE, etc.
  - Argument encoding adapted to RIFF format
  - Compatible with gcc, clang, newlib, picolibc
- **Use Cases**
  - Compiler testing: printf() works immediately
  - File I/O: Read test data, write results
  - Timing: Benchmarks, timeouts
  - System calls: Execute host commands
  - Debugging: Output trace information

**Source**: semihost.md section 1
**Categories**: Semihosting

---

### 13. Device Registers
**Purpose**: Complete hardware register reference

**Content**:
- **Register Map Summary**

| Offset | Size | Name | Access | Description |
|--------|------|------|--------|-------------|
| 0x00 | 16 | RIFF_PTR | RW | Pointer to RIFF buffer in guest RAM |
| 0x10 | 1 | DOORBELL | W | Write any value to trigger request |
| 0x11 | 1 | IRQ_STATUS | R | Interrupt status flags |
| 0x12 | 1 | IRQ_ENABLE | RW | Interrupt enable mask |
| 0x13 | 1 | IRQ_ACK | W | Write 1s to clear interrupt bits |
| 0x14 | 1 | STATUS | R | Device status flags |
| 0x15 | 11 | RESERVED | - | Reserved for future use |

- **RIFF_PTR (0x00-0x0F, 16 bytes, Read/Write)**
  - Holds guest memory address of RIFF communication buffer
  - Format: Raw byte storage in guest's native byte order
  - Guest writes pointer using as many bytes as needed (2, 4, 8, or 16)
  - Unused high bytes ignored
  - Host reads appropriate number of bytes based on CPU address width
  - Examples:
    - 6502 (16-bit): Writes 2 bytes, LE order
    - 68000 (32-bit): Writes 4 bytes, BE order
    - x86-64 (64-bit): Writes 8 bytes, LE order
  - Address interpretation:
    - Virtual devices: May accept virtual addresses
    - Physical devices: Require physical addresses (see [[Operation Modes]])

- **DOORBELL (0x10, 1 byte, Write-Only)**
  - Trigger register to initiate request processing
  - Write any value (typically 0x01)
  - Device detects write and begins processing RIFF buffer
  - Read returns undefined (typically 0x00)

- **IRQ_STATUS (0x11, 1 byte, Read-Only)**
  - Indicates active interrupt conditions
  - Bit 0: RESPONSE_READY - Request completed
  - Bit 1: ERROR - Error occurred during processing
  - Bits 2-7: Reserved (read as 0)
  - Cleared by writing to IRQ_ACK

- **IRQ_ENABLE (0x12, 1 byte, Read/Write)**
  - Controls which interrupt conditions assert CPU IRQ
  - Bit 0: RESPONSE_READY_EN - Enable interrupt on completion
  - Bit 1: ERROR_EN - Enable interrupt on error
  - Bits 2-7: Reserved (write 0, read as 0)
  - Default: 0x00 (all interrupts disabled, polling mode)

- **IRQ_ACK (0x13, 1 byte, Write-Only)**
  - Acknowledge and clear interrupt status bits
  - Write 1s to clear corresponding IRQ_STATUS bits
  - Writing 0s has no effect
  - Example: Write 0x01 clears RESPONSE_READY bit
  - Read returns undefined

- **STATUS (0x14, 1 byte, Read-Only)**
  - General device status flags
  - Bit 0: RESPONSE_READY - Same as IRQ_STATUS bit 0
  - Bit 7: DEVICE_PRESENT - Always 1 (device exists)
  - Bits 1-6: Reserved (read as 0)
  - Polling: Check bit 0 for completion

- **RESERVED (0x15-0x1F, 11 bytes)**
  - Reserved for future use
  - Read returns undefined
  - Write has no effect

- **Bus Interface**
  - Standard memory-mapped peripheral
  - Address decoding: 32-byte aligned address range
  - Data width: Supports byte, word, dword, qword accesses
  - Unaligned accesses: Implementation-dependent

- **Interrupt Output**
  - Signal: IRQ_OUT (active high or low, implementation choice)
  - Asserted when: (IRQ_STATUS & IRQ_ENABLE) != 0
  - Connection: CPU IRQ/NMI input or interrupt controller

**Source**: semihost.md section 2
**Categories**: Semihosting, Reference

---

### 14. RIFF Protocol Fundamentals
**Purpose**: RIFF structure, endianness, chunks

**Content**:
- **What is RIFF?**
  - Resource Interchange File Format
  - Tagged container format (like WAV, AVI files)
  - Chunks have IDs, sizes, and data
  - Self-describing and extensible

- **Buffer Location**
  - RIFF buffer allocated by guest in its own RAM
  - Guest chooses location (stack, heap, static data)
  - Guest tells device location via RIFF_PTR register
  - Device accesses buffer via memory (DMA or emulator)
  - Recommended size: 256-1024 bytes

- **Basic RIFF Structure**
  ```
  RIFF container:
    'RIFF' (4 bytes) - Signature
    size (4 bytes, LE) - Total size minus 8
    'SEMI' (4 bytes) - Form type (semihosting)
    chunks... (CNFG, CALL, etc.)
  ```

- **Chunk Format**
  ```
  Chunk:
    ID (4 bytes) - ASCII fourCC ('CNFG', 'CALL', etc.)
    size (4 bytes, LE) - Data size (excludes 8-byte header)
    data (size bytes) - Chunk-specific data
    [pad byte] - If size is odd, pad to even boundary
  ```

- **Endianness Handling**
  - **RIFF structure** (headers, sizes): **Always little-endian**
  - **Data values** (syscall args, return values): **Guest's native endianness**
  - Guest declares endianness in CNFG chunk
  - Host interprets data using declared endianness
  - Big-endian guests: Must swap bytes in RIFF headers, not in data

- **Chunk Types**
  - **CNFG**: Configuration (guest declares architecture)
  - **CALL**: Syscall request container (has sub-chunks)
  - **PARM**: Parameter value (scalar: integer, pointer)
  - **DATA**: Binary data or string
  - **RETN**: Return value and data (device response)
  - **ERRO**: Error response (malformed request)
  - Future: STRM, EVNT, ABRT, META (see [[Protocol Extensions]])

- **Chunk Nesting Rules**
  - Maximum depth: 2 levels
  - RIFF → CALL/RETN → PARM/DATA
  - CALL and RETN are containers (can have sub-chunks)
  - PARM and DATA are leaf chunks (no sub-chunks)
  - CNFG is leaf chunk
  - Invalid: PARM inside PARM, DATA inside DATA

- **Chunk Alignment**
  - All chunks naturally aligned
  - RIFF buffer should be word-aligned
  - Odd-sized chunks padded to even boundary per RIFF spec
  - Padding not included in chunk size field

- **Request/Response Flow**
  1. Guest writes CNFG chunk (first time only)
  2. Guest writes CALL chunk with PARM/DATA sub-chunks
  3. Guest triggers via DOORBELL
  4. Device processes request
  5. Device overwrites CALL with RETN or ERRO chunk
  6. Guest reads response from same buffer location

**Source**: semihost.md section 3
**Categories**: Semihosting

---

### 15. CNFG Chunk - Configuration
**Purpose**: Architecture declaration

**Content**:
- **Purpose**
  - Guest declares CPU architecture parameters
  - Sent once at session start (first request)
  - Device caches configuration for session
  - Subsequent requests omit CNFG

- **Chunk Format**
  ```
  'CNFG' (4 bytes) - Chunk ID
  0x04 (4 bytes, LE) - Chunk size (4 bytes of data)
  int_size (1 byte) - Size of integer type
  ptr_size (1 byte) - Size of pointer type
  endianness (1 byte) - Byte order
  reserved (1 byte) - Must be 0x00
  ```

- **int_size Field**
  - Size in bytes of natural integer type
  - C `int` or equivalent
  - Examples:
    - 6502 (LLVM-MOS): 2 bytes (16-bit int)
    - ARM Cortex-M: 4 bytes (32-bit int)
    - x86-64: 4 bytes (32-bit int, even on 64-bit)
    - AVR: 2 bytes (16-bit int)

- **ptr_size Field**
  - Size in bytes of pointer types
  - C `void*`, `char*`, etc.
  - Equals address bus width
  - Examples:
    - 6502: 2 bytes (16-bit addressing)
    - ARM Cortex-M: 4 bytes (32-bit addressing)
    - x86-64: 8 bytes (64-bit addressing)
    - AVR: 2 bytes (16-bit addressing)

- **When int_size ≠ ptr_size**
  - Some DSPs: 16-bit int, 24-bit pointers
  - x86-64: 32-bit int, 64-bit pointers
  - Protocol sizes both correctly
  - PARM type 0x01 uses int_size
  - PARM type 0x02 uses ptr_size

- **endianness Field**
  - 0x00: Little Endian (LSB first) - x86, ARM, RISC-V
  - 0x01: Big Endian (MSB first) - 68000, SPARC, PowerPC
  - 0x02: PDP Endian (middle-endian) - PDP-11, VAX
  - 0x03-0xFF: Reserved

- **Configuration Caching**
  - Device caches values after first CNFG
  - Subsequent requests omit CNFG (start with CALL)
  - To change configuration: Reinitialize device or send new CNFG
  - Device uses cached values to interpret all multi-byte values in PARM chunks

- **Example CNFG Chunks**
  - **6502**: int_size=2, ptr_size=2, endianness=0 (LE)
  - **68000**: int_size=4, ptr_size=4, endianness=1 (BE)
  - **x86-64**: int_size=4, ptr_size=8, endianness=0 (LE)

**Source**: semihost.md section 3 (CNFG Chunk)
**Categories**: Semihosting, Reference

---

### 16. CALL and PARM Chunks - Requests
**Purpose**: Syscall request format

**Content**:
- **CALL Chunk - Container**
  - Purpose: Container for syscall request
  - Chunk ID: 'CALL'
  - Format:
    ```
    'CALL' (4 bytes) - Chunk ID
    size (4 bytes, LE) - Variable size
    opcode (1 byte) - ARM semihosting syscall number
    reserved (3 bytes) - Must be 0x00
    sub-chunks... (PARM and DATA chunks)
    ```
  - Opcode: 0x01-0x31 (see [[Syscall Reference]])
  - Sub-chunks: PARM (parameters) and DATA (buffers/strings)
  - Order: Parameters must appear in syscall-expected order

- **PARM Chunk - Parameter Value**
  - Purpose: Scalar parameter (integer, pointer)
  - Chunk ID: 'PARM'
  - Format:
    ```
    'PARM' (4 bytes) - Chunk ID
    size (4 bytes, LE) - 4 + value_size
    param_type (1 byte) - Type code
    reserved (3 bytes) - Must be 0x00
    value (variable) - Parameter value in guest endianness
    ```
  - Parameter Types:
    - 0x01: Integer (size = int_size from CNFG)
    - 0x02: Pointer (size = ptr_size from CNFG)
    - 0x03-0xFF: Reserved
  - Value field:
    - Size depends on type and CNFG
    - Endianness matches guest's declared endianness
    - Type 0x01: int_size bytes
    - Type 0x02: ptr_size bytes

- **Parameter Usage**
  - File descriptors: PARM type 0x01
  - Counts, lengths: PARM type 0x01
  - Modes, flags: PARM type 0x01
  - Status codes: PARM type 0x01
  - Offsets: PARM type 0x01
  - Memory addresses: PARM type 0x02 (or DATA chunk)

- **Example: SYS_WRITE(fd=1, data="Hello\n", length=6)**
  ```
  CALL:
    opcode=0x05 (SYS_WRITE)
    PARM: type=0x01, value=0x01 (fd)
    DATA: type=0x02, payload="Hello\n"
    PARM: type=0x01, value=0x06 (length)
  ```

- **Example: SYS_OPEN(filename="/tmp/test.txt", mode=0, length=14)**
  ```
  CALL:
    opcode=0x01 (SYS_OPEN)
    DATA: type=0x02, payload="/tmp/test.txt\0"
    PARM: type=0x01, value=0x00 (mode)
    PARM: type=0x01, value=0x0E (length=14)
  ```

- **Example: SYS_CLOSE(fd=3)**
  ```
  CALL:
    opcode=0x02 (SYS_CLOSE)
    PARM: type=0x01, value=0x03 (fd)
  ```

- **Building CALL Chunks**
  1. Allocate RIFF buffer (sufficient size)
  2. Write RIFF header (signature, size, 'SEMI')
  3. Write CALL header (ID, size, opcode, reserved)
  4. For each parameter:
     - Scalar: Write PARM chunk (type, value)
     - String/buffer: Write DATA chunk (type, payload)
  5. Calculate and update all size fields
  6. Pad odd-sized chunks to even boundary

**Source**: semihost.md section 3 (CALL, PARM chunks)
**Categories**: Semihosting, Reference

---

### 17. DATA Chunk - Buffers and Strings
**Purpose**: Binary data and string transfer

**Content**:
- **DATA Chunk Format**
  ```
  'DATA' (4 bytes) - Chunk ID
  size (4 bytes, LE) - 4 + payload_length
  data_type (1 byte) - Type code
  reserved (3 bytes) - Must be 0x00
  payload (variable) - Actual data bytes
  [pad byte] - If payload_length is odd
  ```

- **Data Types**
  - 0x01: Binary data (arbitrary bytes)
  - 0x02: String (null-terminated ASCII/UTF-8)
  - 0x03-0xFF: Reserved

- **Payload Field**
  - Variable length data
  - Strings (type 0x02): Include null terminator in payload
  - Binary (type 0x01): Raw bytes, any content
  - Padding: If payload_length odd, pad to even boundary
  - Padding byte not included in size field

- **Usage in CALL Chunks**
  - Filenames: type 0x02 (string)
  - Data to write: type 0x01 or 0x02
  - Commands: type 0x02 (string)
  - Binary buffers: type 0x01

- **Usage in RETN Chunks**
  - Data read back: type 0x01
  - Character read: type 0x01 (single byte)
  - Command line: type 0x02 (string)
  - Temp filename: type 0x02 (string)

- **Example: String "Hello\n"**
  ```
  'DATA' - Chunk ID
  0x0B 0x00 0x00 0x00 - Size (11 = 4 header + 7 payload)
  0x02 - Type (string)
  0x00 0x00 0x00 - Reserved
  'H' 'e' 'l' 'l' 'o' '\n' '\0' - Payload (7 bytes)
  0x00 - Padding byte (payload is odd)
  ```

- **Example: Binary data {0x01, 0x02, 0x03, 0x04}**
  ```
  'DATA' - Chunk ID
  0x08 0x00 0x00 0x00 - Size (8 = 4 header + 4 payload)
  0x01 - Type (binary)
  0x00 0x00 0x00 - Reserved
  0x01 0x02 0x03 0x04 - Payload (4 bytes, even, no padding)
  ```

- **Example: SYS_READ Response**
  - Device reads 256 bytes from file
  - RETN chunk contains:
    - result = 256 (bytes read)
    - errno = 0 (success)
    - DATA chunk: type=0x01, payload=256 bytes read

- **Size Calculation**
  - DATA chunk size = 4 (header) + payload_length
  - Total chunk size = 8 (ID+size) + 4 (header) + payload_length + padding

- **Memory Efficiency**
  - Guest allocates buffers in its own RAM
  - Device reads/writes directly (no copying to device)
  - Buffers can be stack, heap, or static data

**Source**: semihost.md section 3 (DATA chunk)
**Categories**: Semihosting, Reference

---

### 18. RETN and ERRO Chunks - Responses
**Purpose**: Device response format

**Content**:
- **RETN Chunk - Success Response**
  - Purpose: Syscall result with optional data
  - Chunk ID: 'RETN'
  - Format:
    ```
    'RETN' (4 bytes) - Chunk ID
    size (4 bytes, LE) - Variable size
    result (int_size bytes) - Return value in guest endianness
    errno (4 bytes, LE) - POSIX errno (0=success)
    sub-chunks... (Optional DATA chunks)
    ```
  - Result field:
    - Size = int_size from CNFG
    - Endianness = guest's declared endianness
    - Interpretation depends on syscall:
      - File descriptor (SYS_OPEN)
      - Byte count (SYS_READ, SYS_WRITE)
      - Status code (SYS_CLOSE)
      - Character (SYS_READC)
      - Seconds/ticks (SYS_TIME, SYS_CLOCK)
      - -1 typically indicates error
  - Errno field:
    - Always 4 bytes (32-bit)
    - Always little-endian (for consistency)
    - 0 = success, no error
    - >0 = POSIX errno value (ENOENT=2, EACCES=13, etc.)
  - Sub-chunks:
    - DATA chunks for read operations
    - SYS_READ: DATA with bytes read
    - SYS_READC: DATA with single character
    - SYS_GET_CMDLINE: DATA with command line string

- **ERRO Chunk - Error Response**
  - Purpose: Malformed request or processing error
  - Chunk ID: 'ERRO'
  - Format:
    ```
    'ERRO' (4 bytes) - Chunk ID
    size (4 bytes, LE) - Variable size
    error_code (2 bytes, LE) - Error code
    reserved (2 bytes) - Must be 0x00
    message (variable, optional) - ASCII error message
    ```
  - Error Codes:
    - 0x01: Invalid chunk structure (incorrect nesting, wrong sizes, unrecognized chunks)
    - 0x02: Malformed RIFF format (missing signature, wrong form type, size errors)
    - 0x03: Missing CNFG chunk (first request must include CNFG)
    - 0x04: Unsupported opcode (syscall not implemented)
    - 0x05: Invalid parameter count (wrong number of PARM/DATA chunks)
    - 0x06-0xFFFF: Reserved
  - Message field:
    - Optional human-readable error description
    - ASCII text (not null-terminated unless desired)
    - Helps debugging malformed requests

- **Device Behavior**
  - Device **overwrites** CALL chunk with RETN or ERRO
  - Same buffer location reused
  - Guest must read response before making next request

- **Example: SYS_OPEN Success**
  ```
  RETN:
    result = 0x03 (file descriptor 3)
    errno = 0x00000000 (success)
  ```

- **Example: SYS_OPEN Failure**
  ```
  RETN:
    result = 0xFFFFFFFF (-1, error)
    errno = 0x02000000 (ENOENT=2, file not found, LE)
  ```

- **Example: SYS_READ Success**
  ```
  RETN:
    result = 0x0100 (256 bytes read)
    errno = 0x00000000 (success)
    DATA: type=0x01, payload=256 bytes
  ```

- **Example: Invalid Chunk Structure**
  ```
  ERRO:
    error_code = 0x0001 (invalid chunk structure)
    message = "PARM chunk missing required fields"
  ```

- **Handling Responses**
  1. Poll STATUS or wait for interrupt
  2. Read result from int_size bytes
  3. Read errno from 4 bytes (LE)
  4. If result == -1, check errno for error details
  5. If sub-chunks present, parse DATA chunks
  6. Clear STATUS/IRQ before next request

**Source**: semihost.md section 3 (RETN, ERRO chunks)
**Categories**: Semihosting, Reference

---

### 19. Operation Modes
**Purpose**: Polling vs interrupts, cache coherency

**Content**:
- **Synchronous Mode (Polling)**
  - Default mode for simple operation
  - Guest blocks waiting for response
  - No interrupt handler needed
  - Operation:
    1. Guest builds RIFF buffer with CALL chunk
    2. Guest writes buffer address to RIFF_PTR
    3. Guest writes to DOORBELL to trigger
    4. Guest polls STATUS register until RESPONSE_READY bit set
    5. Guest reads RETN chunk from buffer
  - Characteristics:
    - Simple to implement
    - Guest wastes CPU cycles during I/O
    - Suitable for single-tasking programs
  - Typical use: Test programs, firmware bring-up, education

- **Asynchronous Mode (Interrupt-Driven)**
  - Optional mode for advanced operation
  - Guest can multitask during I/O
  - Requires interrupt handler
  - Operation:
    1. Guest enables RESPONSE_READY interrupt via IRQ_ENABLE
    2. Guest builds RIFF buffer and writes to RIFF_PTR
    3. Guest writes to DOORBELL and continues other work
    4. Device processes request
    5. Device sets IRQ_STATUS.RESPONSE_READY and asserts IRQ_OUT
    6. CPU takes interrupt, guest handler runs
    7. Handler reads RETN chunk
    8. Handler writes IRQ_ACK = 0x01 to clear interrupt
  - Characteristics:
    - More complex (requires interrupt handler)
    - Guest can perform other work during I/O
    - Enables true asynchronous operation
  - Typical use: OS development, multitasking, power management

- **Cache Coherency Requirements** (CRITICAL)
  - **Problem**: Device accesses guest RAM directly
  - **Risk**: Cached writes not visible to device, device writes not visible to CPU
  - **Guest responsibilities**:
    1. **Before DOORBELL**: Flush data cache after writing RIFF buffer
    2. **Memory ordering**: Ensure DOORBELL write completes (memory barrier)
    3. **Before reading RETN**: Invalidate cache before reading response
  - **Why this matters**:
    - Device uses DMA or emulator memory access
    - CPU cache may have stale data
    - Failure results in stale data or corruption
  - **Applies to**: Both synchronous and asynchronous modes

- **Concurrent Request Handling**
  - Guest MUST NOT write DOORBELL while STATUS.RESPONSE_READY is set
  - Device behavior is undefined for concurrent requests
  - Device may ignore second DOORBELL or abort first request
  - Wait for completion before next request

- **Address Interpretation**
  - **Virtual devices (emulators)**:
    - May accept virtual addresses from guest
    - Emulator translates using guest's MMU state
    - Guest can use normal pointers
    - Simplifies guest software
  - **Physical devices (FPGA/ASIC)**:
    - Require physical addresses in all pointer fields
    - Device has no access to guest's MMU
    - Guest responsibility:
      - Disable MMU entirely, OR
      - Provide physical addresses (translate in software), OR
      - Use identity-mapped memory regions for RIFF buffers
  - **Recommendation**: Allocate RIFF buffers in physical/identity-mapped memory for compatibility

- **Error Handling**
  - Check IRQ_STATUS.ERROR bit
  - Read ERRO chunk for error details
  - Log error message if present
  - Retry or abort as appropriate

- **Performance Considerations**
  - Synchronous: Polling wastes CPU cycles
  - Asynchronous: Interrupt overhead, handler latency
  - Buffer size: Larger buffers reduce requests
  - Request batching: Combine operations when possible

**Source**: semihost.md sections 4 (Operation Modes), 2 (Cache Coherency)
**Categories**: Semihosting

---

### 20. Syscall Reference
**Purpose**: Complete ARM semihosting compatibility

**Content**:
- **Syscall Number Mapping**
  - Uses ARM semihosting specification syscall numbers
  - Compatible with gcc, clang, newlib, picolibc
  - Arguments encoded as PARM and DATA chunks
  - See [[CALL and PARM Chunks]] for encoding details

- **Syscall Table**

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

- **Open Mode Flags (SYS_OPEN)**

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

- **File Descriptors**
  - 0: stdin (standard input)
  - 1: stdout (standard output)
  - 2: stderr (standard error)
  - 3+: File handles from SYS_OPEN

- **Common Patterns**
  - **Print string**: SYS_WRITE0 or SYS_WRITE(1, string, length)
  - **Print character**: SYS_WRITEC
  - **Read file**: SYS_OPEN → SYS_READ → SYS_CLOSE
  - **Write file**: SYS_OPEN → SYS_WRITE → SYS_CLOSE
  - **Get time**: SYS_TIME or SYS_CLOCK
  - **Exit cleanly**: SYS_EXIT(0)

- **Error Handling**
  - Return value -1 typically indicates error
  - Check errno in RETN chunk for details
  - POSIX errno values (see [[Error Codes and Errno Values]])

- **Implementation Notes**
  - Not all syscalls may be implemented by all devices
  - Unsupported opcodes return ERRO chunk with error code 0x04
  - SYS_HEAPINFO: See ARM semihosting spec for structure details
  - SYS_SYSTEM: May be disabled for security (returns -1)

**Source**: semihost.md section 8 (ARM Semihosting Compatibility)
**Categories**: Semihosting, Reference

---

## Section 4: Implementation Examples (8 pages)

### 21. MAME Implementation Overview
**Purpose**: ZBC reference implementation in MAME

**Content**:
- **MAME as Reference Implementation**
  - Platform-agnostic specification, MAME is one implementation
  - Demonstrates ZBC concepts in production emulator
  - Not the only way to implement ZBC
  - Serves as working example and test platform
- **Architecture Overview**
  - Template-based C++ driver system
  - Single `zbc_state<CPU_TYPE>` template class
  - Instantiated for each CPU type
  - CPU-specific code via template specialization
  - Eliminates duplication across hundreds of CPUs
- **Source File Organization**
  - `src/mame/zbc/zbc.cpp` - Main template implementation
  - `src/mame/zbc/zbcgen.hpp` - Generated CPU headers (auto-generated)
  - `src/mame/zbc/zbcgen.ipp` - Generated DEFINE_ZBC calls (auto-generated)
  - `src/mame/zbc/zbc_status.csv` - CPU status database
  - `src/mame/zbc/m6847drv.cpp/h` - MC6847 console driver
  - `scripts/build/zbcgen.py` - Code generation tool
- **Driver Registration**
  - `src/mame/mame.lst` - Driver list (updated by zbcgen.py)
  - Each working CPU gets entry: `zbcm6502`, `zbcz80`, etc.
  - Only CPUs with status=working included
- **Key Features**
  - Automated CPU discovery via `-listcpu`
  - Template instantiation for each CPU
  - CPU-specific reset vector setup
  - Quickload support for binary programs
  - MC6847 text display with console driver
  - Semihosting plugin integration
- **Design Goals**
  - Comprehensive CPU coverage (hundreds of architectures)
  - Automated quality control
  - Minimal manual maintenance
  - Test harness for MAME CPU emulations
- **Integration with MAME Plugins**
  - Semihosting plugin (`plugins/semihost/`)
  - Provides host I/O services
  - Memory-mapped device at configurable address
  - See [[Using Semihosting Services]] for usage

**Source**: zbc.rst sections 1, 2, 5
**Categories**: MAME Implementation

---

### 22. zbcgen Tool
**Purpose**: Automated CPU discovery and code generation

**Content**:
- **Purpose and Motivation**
  - Automate discovery of all MAME CPU devices
  - Generate ZBC driver code automatically
  - Track CPU status (working, broken, disabled)
  - Reduce manual maintenance burden
  - Serve as canonical list of MAME CPU support
- **Command-Line Interface**
  - `--scan-mame <cpus.txt>` - Initial CPU discovery
  - `--mark-broken-compile <build.log>` - Mark compilation failures
  - `--mark-broken-validate <validate.log>` - Mark validation failures
  - `--build` - Regenerate output files from current state
- **Workflow Overview**
  1. Run `mame -listcpu` to enumerate CPUs
  2. Run `zbcgen.py --scan-mame` to create/update CSV
  3. Run `zbcgen.py --build` to generate code
  4. Build MAME, capture errors
  5. Run `zbcgen.py --mark-broken-compile` to mark failures
  6. Iterate until clean build
  7. Run `mame -validate`, capture output
  8. Run `zbcgen.py --mark-broken-validate` to mark issues
  9. Rebuild with only working CPUs
- **Initial Generation (--scan-mame)**
  - Reads `mame -listcpu` output
  - Parses CPU metadata: shortname, type constant, class name, full name
  - Infers header file using device type mapping
  - Creates initial `zbc_status.csv` with all discovered CPUs
  - Sets initial status: `working` if header found, `broken_header` if not
- **Device Type Mapping**
  - Loads all CPU header files (`src/devices/cpu/**/*.h`)
  - Scans for `DECLARE_DEVICE_TYPE` declarations
  - Builds mapping: device type constant → header file path
  - Example: `M6502 → cpu/m6502/m6502.h`
  - More reliable than pattern-matching heuristics
- **Marking Broken Compilation (--mark-broken-compile)**
  - Parses build log for compilation errors
  - Recognizes GCC/Clang/MSVC error formats
  - Extracts line numbers and error messages
  - Looks up which `DEFINE_ZBC` call failed
  - Marks CPU as `broken_compile` in CSV with error message
- **Marking Broken Validation (--mark-broken-validate)**
  - Parses `mame -validate` output
  - Identifies validation errors by driver name
  - Extracts error messages
  - Marks CPU as `broken_validate` in CSV with details
- **Regenerating Output (--build)**
  - Reads current `zbc_status.csv`
  - Generates `zbcgen.hpp` with `#include` directives (only working CPUs)
  - Generates `zbcgen.ipp` with `DEFINE_ZBC()` calls (only working CPUs)
  - Updates `src/mame/mame.lst` with driver list
  - Sorts alphabetically by shortname
- **Output File Formats**
  - **zbcgen.hpp**: Alphabetical CPU header includes
  - **zbcgen.ipp**: DEFINE_ZBC macro calls (included by zbc.cpp)
  - **mame.lst**: Driver registration (@source:zbc/zbc.cpp section)
- **Iterative Workflow**
  - Build → Mark failures → Rebuild → Validate → Mark failures → Final build
  - Typically 2-3 iterations to clean state
  - CSV tracks status across iterations

**Source**: zbc.rst section 4
**Categories**: MAME Implementation, Developer Documentation

---

### 23. CPU Status Database
**Purpose**: zbc_status.csv schema and status values

**Content**:
- **Purpose**
  - Single source of truth for CPU status
  - Version-controlled knowledge base
  - Tracks which CPUs work, which are broken, why
  - Enables automated code generation
  - Historical record of CPU testing
- **CSV Schema**
  ```
  shortname,type_constant,class_name,fullname,status,header_file,notes
  ```
- **Field Definitions**
  - **shortname**: Short identifier (e.g., "m6502", "z80")
  - **type_constant**: Device type macro (e.g., "M6502", "Z80")
  - **class_name**: C++ device class (e.g., "m6502_device", "z80_device")
  - **fullname**: Human-readable name (e.g., "MOS Technology 6502")
  - **status**: Current state (see status values below)
  - **header_file**: Include path (e.g., "cpu/m6502/m6502.h")
  - **notes**: Freeform text (error messages, reasons for status)
- **Status Values**
  - `working`: CPU compiles, validates, and runs correctly
  - `broken_compile`: Compilation fails (syntax errors, missing dependencies)
  - `broken_validate`: Compiles but fails `mame -validate` checks
  - `broken_header`: Header file not found or doesn't declare device type
  - `broken_drc`: DRC (dynamic recompilation) causes infinite loop or crash
  - `disabled`: Manually disabled (special requirements, conflicts, name length)
  - `not_cpu`: Device is not a CPU (DMA controller, graphics, etc.)
  - `needs_internal_rom`: Requires internal ROM/RAM regions (PICs, 8051, AVR)
  - `needs_dependent_device`: Requires additional devices (PS2 VU, Emotion Engine)
  - `unknown`: Not yet tested (initial state)
- **Example Entries**
  ```csv
  m6502,M6502,m6502_device,"MOS Technology 6502",working,cpu/m6502/m6502.h,
  z80,Z80,z80_device,"Zilog Z80",working,cpu/z80/z80.h,
  ppc403ga,PPC403GA,ppc403ga_device,"IBM PowerPC 403GA",broken_drc,cpu/powerpc/ppc.h,"DRC infinite loop"
  pic16c54,PIC16C54,pic16c54_device,"Microchip PIC16C54",needs_internal_rom,cpu/pic16c5x/pic16c5x.h,"Requires internal ROM"
  ```
- **Maintenance**
  - Updated by zbcgen.py tool
  - Version controlled (git)
  - Manual edits allowed (e.g., marking CPUs disabled)
  - Can add notes for future reference
- **Statistics** (example)
  - Total CPUs discovered: ~600
  - Status working: ~300
  - Status broken_compile: ~100
  - Status broken_validate: ~50
  - Status needs_internal_rom: ~80
  - Status not_cpu: ~30
  - Other: ~40
- **Usage**
  - Read by zbcgen.py --build
  - Determines which CPUs included in generation
  - Only `status=working` CPUs generate code
  - Broken/disabled CPUs excluded from build

**Source**: zbc.rst section 3
**Categories**: MAME Implementation, Reference

---

### 24. Build System Integration
**Purpose**: mame.lst updates and compilation workflow

**Content**:
- **mame.lst File**
  - Master driver registration list
  - Tells MAME which drivers to compile
  - Format: driver names under `@source:` sections
  - ZBC section: `@source:zbc/zbc.cpp`
- **zbcgen.py mame.lst Updates**
  - Automatically updates ZBC section
  - Locates `@source:zbc/zbc.cpp` in mame.lst
  - Replaces driver list with working CPUs from CSV
  - Sorts alphabetically by shortname
  - Format: One driver per line (e.g., `zbcm6502`, `zbcz80`)
- **Example mame.lst Section**
  ```
  @source:zbc/zbc.cpp
  zbcarm7
  zbci386
  zbcm6502
  zbcm68000
  zbcz80
  ```
- **Compilation Workflow**
  1. Run `zbcgen.py --build` to update mame.lst
  2. Run `make` to build MAME
  3. Only listed drivers compiled
  4. Broken/disabled CPUs excluded automatically
- **Integration with MAME Build**
  - mame.lst drives build system
  - Build system generates driver registration code
  - Links all listed drivers into final MAME binary
- **Manual Overrides**
  - Can manually edit mame.lst to exclude CPUs
  - zbcgen.py --build will overwrite changes
  - Better to mark disabled in zbc_status.csv
- **Build Targets** (potential future)
  - `make zbcgen-scan`: Run CPU discovery
  - `make zbcgen-build`: Regenerate code
  - `make zbc`: Build only ZBC drivers
  - Not currently implemented, manual workflow
- **Continuous Integration**
  - Could automate zbcgen workflow in CI
  - Build → mark failures → rebuild → validate
  - Track CPU status changes over time
  - Not currently implemented

**Source**: zbc.rst section 5
**Categories**: MAME Implementation, Developer Documentation

---

### 25. Adding CPU Support in MAME
**Purpose**: How to extend MAME's ZBC implementation

**Content**:
- **When to Add Specialization**
  - CPU boots with uninitialized memory
  - CPU doesn't execute without loaded program
  - Need reset vectors or exception handlers
  - Want idle loop for better boot experience
- **Template Specialization Approach**
  - Specialize `init_cpu_for_idle<CPU_TYPE, LOAD_ADDR>()`
  - Function called during `machine_start()`
  - Writes initialization code to guest memory
  - CPU-specific: reset vectors, exception tables, idle loop
- **Specialization Template**
  ```cpp
  template <>
  void zbc_state<my_cpu_device, LOAD_ADDR>::init_cpu_for_idle(address_space &space) {
      // Set up reset vector
      // Write idle loop at load address
      // Configure exception handlers
  }
  ```
- **Example: 6502 Specialization**
  - Reset vector at 0xFFFC-0xFFFD
  - Points to load address (default 0x0200)
  - Idle loop at 0x0200: `JMP $0200` (0x4C 0x00 0x02)
  - Allows boot without program loaded
- **Example: Z80 Specialization**
  - Boot at address 0x0000
  - JP to load address at 0x0000
  - NMI handler at 0x0066: `RETN` (0xED 0x45)
  - Prevents crash from VSync NMI
- **Example: 68000 Specialization**
  - Vector table at 0x0000
  - Initial SSP (stack pointer): 0x0000-0x0003
  - Initial PC (program counter): 0x0004-0x0007
  - Points to load address
- **Development Process**
  1. Identify CPU boot requirements
  2. Research CPU architecture manual
  3. Determine reset vector addresses
  4. Write template specialization in zbc.cpp
  5. Test with `mame zbcXXXX`
  6. Verify boot screen displays correctly
  7. Test with quickload program
  8. Run `mame zbcXXXX -validate`
- **Testing**
  - Boot without program: Should show boot screen
  - Load program: Should execute correctly
  - VSync interrupt: Should not crash (if JP1 enabled)
  - Validate: Should pass with no errors
- **Common Pitfalls**
  - Wrong endianness for multi-byte values
  - Incorrect reset vector address
  - Missing exception handlers
  - Stack pointer not initialized
  - Idle loop overwrites important memory
- **When Not to Specialize**
  - CPU works fine without specialization
  - Always loading program via quickload
  - CPU has complex boot requirements
  - Better handled in loaded program

**Source**: zbc.rst sections 6, 2.2
**Categories**: MAME Implementation, Developer Documentation

---

### 26. Hardware Implementation Guide
**Purpose**: Building ZBC in FPGA/ASIC

**Content**:
- **Overview**
  - ZBC can be built in physical hardware
  - FPGA prototype or full ASIC design
  - Platform-agnostic specification applies
  - Reference: MAME implementation and this documentation
- **Required Components**
  - **CPU core**: Any architecture (8-bit to 128-bit+)
    - Soft core (RISC-V, 6502, Z80, etc.)
    - Hard IP (ARM, x86, etc.)
  - **RAM**: Sized based on address space
    - 16-bit CPU: 64KB SRAM/DRAM
    - 32-bit CPU: Up to 4GB DRAM
  - **MC6847 VDG**: Text display controller
    - Original MC6847 chip, or
    - FPGA implementation, or
    - Modern VGA controller emulating MC6847
  - **Semihosting Device**: Custom peripheral
    - 32-byte register space
    - DMA controller for guest RAM access
    - Host interface (UART, USB, PCIe, etc.)
- **Memory Map Design**
  - Follow dynamic layout algorithm (see [[Memory Layout and Addressing]])
  - Low memory: CPU vectors, stack, program
  - Main RAM: Bulk of address space
  - High memory: Semihosting registers, VRAM, reserved
- **Semihosting Device Implementation**
  - Register interface (see [[Device Registers]])
  - RIFF_PTR: 128-bit register (use what CPU needs)
  - DOORBELL: Write-triggered state machine
  - IRQ_STATUS, IRQ_ENABLE, IRQ_ACK: Interrupt logic
  - STATUS: Ready flag
  - DMA controller to read/write guest RAM
  - Host interface to execute syscalls
- **MC6847 Implementation**
  - Character ROM (ASCII font)
  - 512-byte VRAM at high address
  - 32x16 text mode
  - VSync signal generation (~60-62 Hz)
  - Connect VSync to JP1 jumper logic
- **JP1 Jumper Logic**
  - 3-position physical jumper or config register
  - Routes VSync to: Disabled / CPU IRQ / CPU NMI
  - See [[Interrupt System (JP1 Jumper)]]
- **Boot Process**
  - CPU reset
  - Execute from reset vector address
  - If specialization implemented: Idle loop
  - If no specialization: Execute from 0x0000 or reset vector
  - Display boot screen (requires software)
- **Host Interface Options**
  - **UART**: Simple serial, low bandwidth
  - **USB**: Higher bandwidth, more complex
  - **Ethernet**: Network-based semihosting
  - **PCIe**: Very high bandwidth, complex
  - **GPIO/SPI**: Bit-banged for simple systems
- **Software Requirements**
  - CPU-specific reset vector setup (if needed)
  - Boot ROM or initialized RAM with startup code
  - MC6847 console driver (optional, for boot screen)
  - Semihosting library for guest programs
- **Testing and Validation**
  - Test CPU reset and execution
  - Test RAM reads/writes
  - Test MC6847 display
  - Test semihosting syscalls
  - Compare behavior with MAME reference

**Source**: Original content based on ZBC specification
**Categories**: Implementation, Hardware

---

### 27. Emulator Implementation Guide
**Purpose**: Adding ZBC to other emulators

**Content**:
- **Overview**
  - ZBC can be added to any CPU emulator
  - Not limited to MAME
  - Follow platform-agnostic specification
  - Examples: QEMU, Unicorn, custom emulators
- **Implementation Steps**
  1. Choose target CPU architecture
  2. Set up memory map (dynamic layout)
  3. Implement MC6847 VDG emulation
  4. Implement semihosting device emulation
  5. Add quickload mechanism
  6. Add CPU-specific initialization
  7. Test with programs
- **Memory Management**
  - Allocate guest RAM based on address space
  - Map VRAM at high address (see [[Memory Layout and Addressing]])
  - Reserve space for semihosting registers
  - Use dynamic layout formulas
- **MC6847 Emulation**
  - 512 bytes VRAM (32x16 characters)
  - Character set: ASCII or MC6847 original
  - Rendering: Convert to framebuffer or text console
  - VSync timing: ~60-62 Hz
  - VSync callback for interrupt
- **Semihosting Device Emulation**
  - Trap memory accesses to semihosting register range
  - Implement register behavior (see [[Device Registers]])
  - RIFF_PTR: Store guest buffer address
  - DOORBELL: Trigger RIFF parsing and syscall execution
  - STATUS: Set ready flag after completion
  - Parse RIFF buffer from guest memory
  - Execute syscalls on host
  - Write RETN/ERRO to guest buffer
- **RIFF Protocol Implementation**
  - Parse RIFF structures from guest memory
  - Handle endianness (RIFF headers LE, data guest-native)
  - Parse CNFG chunk, cache configuration
  - Parse CALL/PARM/DATA chunks
  - Execute ARM semihosting syscalls
  - Build RETN/ERRO chunks
- **Syscall Execution**
  - Map ARM semihosting numbers to host operations
  - File I/O: Open, read, write, close host files
  - Console I/O: Print to host console
  - Timing: Host clock/timer
  - System: Execute host commands (with caution)
  - Sandboxing: Restrict file access to designated directories
- **Virtual Address Handling**
  - Emulators can accept virtual addresses
  - Translate using guest MMU state
  - Simplifies guest software (no physical address translation)
  - Physical hardware requires physical addresses
- **Quickload Mechanism**
  - Add option to load binary file
  - Read file from host filesystem
  - Write to guest memory at load address
  - Start CPU execution
- **CPU Initialization**
  - Set up reset vectors (CPU-specific)
  - Write idle loop at load address
  - See [[CPU Support and Initialization]]
- **Testing**
  - Boot without program (idle loop)
  - Load and execute test programs
  - Test semihosting syscalls
  - Compare output with MAME reference
- **Example: QEMU Integration**
  - Add ZBC machine type to QEMU
  - Implement as QEMU device models
  - Use QEMU's memory-mapped I/O framework
  - Leverage QEMU's existing file I/O

**Source**: Original content based on ZBC specification
**Categories**: Implementation, Emulator

---

### 28. Implementation Checklist
**Purpose**: Requirements for compliant ZBC system

**Content**:
- **Minimum Requirements (Core Specification)**
  - [ ] CPU (any architecture, any size)
  - [ ] RAM (sized appropriately for CPU)
  - [ ] MC6847 VDG text display (32x16 characters)
  - [ ] Semihosting peripheral (memory-mapped, 32-byte registers)
  - [ ] Dynamic memory layout (formulas from specification)
  - [ ] RIFF protocol support (parse/generate RIFF structures)
  - [ ] ARM semihosting syscall compatibility (minimum set)
- **Memory Layout**
  - [ ] Dynamic reserved region calculation: `2^addr_bits - 2^(addr_bits/2)`
  - [ ] VRAM at `reserved_start - 512`
  - [ ] Semihosting buffer at `reserved_start - 1536`
  - [ ] Available RAM from load address to semihost address
- **MC6847 VDG**
  - [ ] 512 bytes VRAM
  - [ ] 32 columns × 16 rows
  - [ ] ASCII character set (or MC6847 original)
  - [ ] Memory-mapped at calculated address
  - [ ] VSync signal generation (~60-62 Hz)
- **Semihosting Device Registers**
  - [ ] RIFF_PTR (16 bytes, guest pointer storage)
  - [ ] DOORBELL (1 byte, write-triggered)
  - [ ] IRQ_STATUS (1 byte, interrupt flags)
  - [ ] IRQ_ENABLE (1 byte, interrupt mask)
  - [ ] IRQ_ACK (1 byte, clear interrupts)
  - [ ] STATUS (1 byte, device status)
  - [ ] RESERVED (11 bytes, future use)
- **RIFF Protocol**
  - [ ] Parse RIFF container ('RIFF', size, 'SEMI')
  - [ ] Parse CNFG chunk, cache configuration
  - [ ] Parse CALL chunk with PARM/DATA sub-chunks
  - [ ] Execute syscalls based on opcode
  - [ ] Generate RETN chunk with result and errno
  - [ ] Generate ERRO chunk for malformed requests
  - [ ] Handle endianness (RIFF LE, data guest-native)
  - [ ] Support chunk nesting (2 levels max)
  - [ ] Pad odd-sized chunks to even boundary
- **ARM Semihosting Syscalls (Minimum Set)**
  - [ ] SYS_OPEN (0x01) - Open file
  - [ ] SYS_CLOSE (0x02) - Close file
  - [ ] SYS_WRITE (0x05) - Write to file/console
  - [ ] SYS_READ (0x06) - Read from file
  - [ ] SYS_WRITEC (0x03) - Write character
  - [ ] SYS_WRITE0 (0x04) - Write null-terminated string
  - [ ] SYS_READC (0x07) - Read character
  - [ ] SYS_CLOCK (0x10) - Get clock ticks
  - [ ] SYS_TIME (0x11) - Get time
  - [ ] SYS_EXIT (0x18) - Exit cleanly
- **Optional Features**
  - [ ] JP1 jumper (VSync interrupt routing)
  - [ ] Asynchronous mode (interrupt-driven semihosting)
  - [ ] Full ARM semihosting syscall set
  - [ ] Boot screen with system information
  - [ ] MC6847 console driver (scrolling, word wrap)
  - [ ] Quickload mechanism
  - [ ] CPU-specific initialization
- **Testing Checklist**
  - [ ] CPU boots and executes
  - [ ] RAM accessible (read/write)
  - [ ] VRAM displays characters
  - [ ] Semihosting registers accessible
  - [ ] DOORBELL triggers request processing
  - [ ] STATUS indicates completion
  - [ ] SYS_WRITE outputs to console
  - [ ] SYS_READ reads from file
  - [ ] SYS_OPEN/SYS_CLOSE work correctly
  - [ ] Endianness handled correctly (LE and BE CPUs)
  - [ ] int_size and ptr_size handled correctly
  - [ ] Errors return ERRO chunk
- **Compliance Levels**
  - **Level 1 (Minimal)**: Core spec, basic syscalls (OPEN, CLOSE, READ, WRITE, EXIT)
  - **Level 2 (Standard)**: All ARM semihosting syscalls, JP1 jumper
  - **Level 3 (Full)**: Async mode, boot screen, quickload, CPU specializations
- **Validation**
  - [ ] Compare behavior with MAME reference implementation
  - [ ] Test with known-good programs
  - [ ] Verify memory layout calculations
  - [ ] Test multiple CPU architectures (if applicable)
  - [ ] Test LE and BE endianness
  - [ ] Test different int_size and ptr_size combinations

**Source**: Original content based on ZBC specification
**Categories**: Implementation, Reference

---

## Section 5: User Documentation (4 pages)

### 29. Running ZBC Systems
**Purpose**: Command-line usage and basic operation

**Content**:
- **Prerequisites**
  - MAME installed (for MAME implementation)
  - OR other ZBC-compatible emulator/hardware
  - Binary program to load (optional)
- **Listing Available ZBC Systems**
  - Command: `mame -listfull zbc*`
  - Shows all ZBC systems with descriptions
  - Example output:
    ```
    zbcm6502         Zero Board Computer (MOS Technology 6502)
    zbcz80           Zero Board Computer (Zilog Z80)
    zbcm68000        Zero Board Computer (Motorola 68000)
    zbcarm7          Zero Board Computer (ARM7)
    ```
- **Running Without Program (Idle Mode)**
  - Command: `mame zbcz80`
  - System boots to idle loop
  - Boot screen displays system information:
    - CPU name and type
    - Load address
    - Available RAM size
    - Semihosting buffer address
    - Video RAM address
  - CPU idles until stopped
  - Useful for verifying system works
- **Loading Programs (Quickload)**
  - Command: `mame zbcz80 -quik program.bin`
  - Alternative: `-quickload program.bin`
  - Binary loaded at load address
  - Execution begins immediately
  - Boot screen cleared (VRAM becomes writable)
- **Using Semihosting**
  - Enable plugin: `-plugin semihost`
  - Full command: `mame zbcz80 -quik program.bin -plugin semihost`
  - Program can use semihosting syscalls
  - File I/O, console I/O, timing available
  - See [[Using Semihosting Services]] for details
- **Configuring JP1 Jumper**
  - Default: VSync interrupt disabled
  - Enable via UI: TAB → Machine Configuration → JP1
  - Options: Disabled (0), IRQ (1), NMI (2)
  - Configuration file: `mame.ini` or `zbcz80.ini`
  - See [[Interrupt System (JP1 Jumper)]] for details
- **Common Command-Line Options**
  - `-quik <file>` - Load program binary
  - `-plugin semihost` - Enable semihosting
  - `-window` - Run in window (not fullscreen)
  - `-debug` - Start with debugger
  - `-verbose` - Verbose logging
  - `-seconds_to_run N` - Run for N seconds, then exit
- **Reading Boot Screen Information**
  - CPU name: Identifies architecture
  - Load address: Where program starts (e.g., 0x0200)
  - Available RAM: Total usable RAM (e.g., 63,488 bytes)
  - Semihosting: Buffer address range (e.g., 0xFC00-0xFDFF)
  - Video RAM: Display address range (e.g., 0xFE00-0xFEFF)
  - Use this info when writing programs
- **Troubleshooting**
  - **No boot screen**: CPU may need specialization, or VRAM not working
  - **Program doesn't run**: Check load address in program linker script
  - **Semihosting not working**: Ensure plugin enabled, check buffer address
  - **Crash on boot**: May need JP1 disabled (VSync interrupt issue)
- **Examples by CPU**
  - **6502**: `mame zbcm6502 -quik test.bin -plugin semihost`
  - **Z80**: `mame zbcz80 -quik test.bin`
  - **68000**: `mame zbcm68000 -quik test.bin`
  - **ARM7**: `mame zbcarm7 -quik test.bin -plugin semihost`

**Source**: zbc.rst section 8.1
**Categories**: User Guides

---

### 30. Writing Programs for ZBC
**Purpose**: Toolchain setup and programming concepts (no code examples)

**Content**:
- **Toolchain Selection**
  - Choose compiler/assembler for target CPU
  - Examples by architecture:
    - **6502**: llvm-mos (Clang), cc65
    - **Z80**: z88dk, sdcc
    - **68000**: m68k-elf-gcc (GNU)
    - **ARM**: arm-none-eabi-gcc (GNU)
    - **x86**: gcc/clang with appropriate target
    - **MIPS**: mips-elf-gcc
- **Toolchain Installation**
  - llvm-mos: https://github.com/llvm-mos/llvm-mos-sdk
  - GNU toolchains: Via package manager or build from source
  - Platform-specific: Follow toolchain documentation
- **Linker Script Configuration**
  - Set load address (check boot screen for correct value)
  - Define memory regions
  - Place code/data in correct regions
  - Example memory regions:
    - ROM: Start at load address, size for code
    - RAM: After code, size for data/stack
    - VRAM: At VRAM address (see boot screen)
- **Building Programs**
  - Compile source to object files
  - Link with correct load address
  - Extract binary: `objcopy -O binary program.elf program.bin`
  - Resulting file: Raw binary, no headers
- **Memory Access**
  - **VRAM**: Memory-mapped at high address
    - 16-bit CPU: Typically 0xFE00
    - 32-bit CPU: Typically 0xFFFFFE00
    - Check boot screen for exact address
  - **Semihosting**: Buffer in guest RAM
    - Allocate 256-1024 bytes
    - Can be stack, heap, or static
    - Build RIFF structures in buffer
- **Display Output**
  - Write characters directly to VRAM
  - Address calculation: `vram[row * 32 + col] = character`
  - ASCII characters (0x20-0x7F printable)
  - 32 columns × 16 rows
- **Semihosting Output**
  - Build RIFF structures in RAM
  - Write buffer address to RIFF_PTR register
  - Trigger via DOORBELL register
  - Poll STATUS or wait for interrupt
  - See [[Using Semihosting Services]] for details
- **Interrupt Handling**
  - If using JP1 IRQ/NMI mode
  - Install interrupt vector at CPU-specific address
  - Handler must return with correct instruction (RETI, RTI, etc.)
  - Acknowledge interrupt if required
  - See [[Interrupt System (JP1 Jumper)]]
- **Program Structure**
  - Entry point at load address
  - Initialize stack pointer (CPU-specific)
  - Set up interrupt vectors (if using interrupts)
  - Main program logic
  - Exit cleanly via SYS_EXIT (if using semihosting)
- **Testing**
  - Start simple (idle loop, write to VRAM)
  - Add semihosting gradually
  - Test each feature incrementally
  - Use MAME debugger if needed
- **Common Mistakes**
  - Wrong load address (doesn't match linker script)
  - Wrong VRAM address (check boot screen)
  - Wrong semihosting buffer address
  - Endianness errors in RIFF structures
  - Not flushing cache (CPUs with data cache)
- **Resources**
  - Toolchain documentation
  - CPU architecture manuals
  - ZBC specification (this wiki)
  - MAME debugger for investigation

**Source**: zbc.rst section 8.2
**Categories**: User Guides

---

### 31. Using Semihosting Services
**Purpose**: Conceptual guide to file I/O, console, timing

**Content**:
- **Overview**
  - Semihosting provides host services to guest
  - File I/O without guest filesystem
  - Console I/O without guest drivers
  - Timing without guest timers
  - System calls without guest OS
- **Basic Workflow**
  1. Allocate RIFF buffer in guest RAM (256-1024 bytes)
  2. Build CNFG chunk (first time only)
  3. Build CALL chunk with syscall request
  4. Write buffer address to RIFF_PTR register
  5. Write to DOORBELL register to trigger
  6. Poll STATUS register or wait for interrupt
  7. Read RETN chunk from buffer
  8. Extract result and errno
  9. Process returned data if applicable
- **Configuration (CNFG Chunk)**
  - Send once at program start
  - Declare int_size, ptr_size, endianness
  - Device caches configuration
  - Subsequent requests omit CNFG
  - See [[CNFG Chunk]] for details
- **Console Output**
  - **SYS_WRITEC**: Write single character
  - **SYS_WRITE0**: Write null-terminated string
  - **SYS_WRITE**: Write to stdout (fd=1)
  - No guest console driver needed
  - Output appears on host console
- **File Operations**
  - **SYS_OPEN**: Open file on host
    - Provide filename, mode, length
    - Returns file descriptor or -1
  - **SYS_READ**: Read from file
    - Provide fd, length
    - Returns bytes read, DATA chunk with content
  - **SYS_WRITE**: Write to file
    - Provide fd, data, length
    - Returns bytes written
  - **SYS_CLOSE**: Close file
    - Provide fd
    - Returns 0 or -1
  - **SYS_SEEK**: Seek in file
  - **SYS_FLEN**: Get file length
- **File I/O Example Workflow**
  1. SYS_OPEN to get file descriptor
  2. Check result (-1 = error, check errno)
  3. SYS_READ or SYS_WRITE to transfer data
  4. Check result (bytes transferred or -1)
  5. Repeat as needed
  6. SYS_CLOSE when done
- **Timing Services**
  - **SYS_CLOCK**: Centiseconds since start
  - **SYS_TIME**: Seconds since epoch (Unix time)
  - **SYS_ELAPSED**: 64-bit tick count
  - **SYS_TICKFREQ**: Ticks per second
  - Use for benchmarking, timeouts, delays
- **Other Services**
  - **SYS_ERRNO**: Get last error code
  - **SYS_ISTTY**: Check if fd is TTY
  - **SYS_GET_CMDLINE**: Get command-line arguments
  - **SYS_SYSTEM**: Execute host command (may be disabled)
  - **SYS_EXIT**: Clean shutdown
- **Error Handling**
  - Check result value (usually -1 = error)
  - Read errno from RETN chunk
  - Common errno values:
    - ENOENT (2): File not found
    - EACCES (13): Permission denied
    - EBADF (9): Bad file descriptor
    - EIO (5): I/O error
  - See [[Error Codes and Errno Values]] for full list
- **Sandboxing** (MAME plugin)
  - File access restricted to configured directory
  - Prevents accidental host filesystem damage
  - Configure via plugin settings
- **Performance Considerations**
  - Semihosting has overhead (host interaction)
  - Use for debugging/development, not production
  - Batch operations when possible (larger reads/writes)
  - Avoid polling in tight loops (use interrupts if available)
- **Cache Coherency** (Important!)
  - CPUs with data cache: Must flush before DOORBELL
  - Must invalidate cache before reading RETN
  - See [[Operation Modes]] for details
  - Failure can cause stale data or corruption
- **Debugging**
  - Use SYS_WRITE0 for debug messages
  - Use SYS_CLOCK for timing measurements
  - Check errno for operation failures
  - Use MAME debugger to inspect RIFF buffers

**Source**: zbc.rst section 8, semihost.md sections 1, 4, 8
**Categories**: User Guides, Semihosting

---

### 32. Troubleshooting
**Purpose**: FAQ and common issues

**Content**:
- **Boot Issues**
  - **No boot screen displayed**
    - Check: VRAM address correct?
    - Check: MC6847 emulation working?
    - Check: CPU executing at all?
    - Try: Different ZBC system (different CPU)
  - **System hangs on boot**
    - Check: JP1 jumper set to Disabled?
    - Check: CPU has specialization or loaded program?
    - Try: Load program with -quik
  - **Crashes immediately**
    - Check: JP1 jumper (VSync NMI may crash without handler)
    - Check: CPU-specific initialization correct?
    - Try: MAME debugger to see where crash occurs
- **Program Loading Issues**
  - **Program doesn't execute**
    - Check: Load address in linker script matches system
    - Check: Binary file correct format (no headers)?
    - Check: objcopy extracted binary correctly?
    - Try: Disassemble binary to verify contents
  - **Program executes but behaves wrong**
    - Check: Endianness correct for CPU?
    - Check: int_size and ptr_size correct?
    - Try: MAME debugger to trace execution
- **Display Issues**
  - **Nothing appears on display**
    - Check: Writing to correct VRAM address?
    - Check: VRAM address from boot screen?
    - Check: ASCII characters (0x20-0x7F)?
    - Try: Simple test (write 'A' to offset 0)
  - **Garbage on display**
    - Check: Character codes correct?
    - Check: Row/column calculation correct?
    - Check: Not overwriting VRAM accidentally?
- **Semihosting Issues**
  - **Semihosting not working**
    - Check: Plugin enabled (-plugin semihost)?
    - Check: RIFF buffer address correct?
    - Check: RIFF structures well-formed?
    - Check: CNFG chunk sent first time?
    - Try: Use debugger to inspect RIFF buffer
  - **ERRO chunk returned**
    - Read error_code from ERRO chunk
    - Read error message if present
    - Common errors:
      - 0x01: Invalid chunk structure (check nesting, sizes)
      - 0x02: Malformed RIFF (check signature, form type)
      - 0x03: Missing CNFG (send CNFG first)
      - 0x04: Unsupported opcode (check syscall number)
      - 0x05: Invalid parameter count (check PARM/DATA chunks)
  - **File operations fail**
    - Check errno in RETN chunk
    - Check: File exists and accessible?
    - Check: Sandbox directory configured correctly?
    - Check: File path format correct?
- **Interrupt Issues**
  - **VSync interrupt crashes**
    - Check: JP1 jumper setting
    - Check: Interrupt handler installed?
    - Check: Handler returns with correct instruction?
    - Try: Disable JP1, use polling instead
  - **Interrupt never fires**
    - Check: JP1 jumper not Disabled?
    - Check: IRQ_ENABLE register set correctly?
    - Check: CPU interrupt mask not set?
- **Performance Issues**
  - **System very slow**
    - Check: Polling in tight loop?
    - Try: Use interrupt mode instead
    - Try: Larger read/write buffers
    - Try: Batch semihosting operations
  - **Excessive file I/O**
    - Try: Buffer reads/writes in guest RAM
    - Try: Reduce number of syscalls
- **Compatibility Issues**
  - **CPU not listed in MAME**
    - Check: CPU marked working in zbc_status.csv?
    - Try: Different CPU from same family
    - See: [[Supported CPU List]]
  - **Validation errors**
    - Check: `mame -validate zbcXXXX`
    - May indicate CPU or system issue
    - Report to MAME developers
- **Getting Help**
  - Check: This wiki documentation
  - Check: ZBC specification (docs/source/specification.rst)
  - Check: MAME documentation
  - Use: MAME debugger for investigation
  - Use: Verbose logging (-verbose flag)
  - Report: Issues to appropriate project

**Source**: Original content, common user issues
**Categories**: User Guides

---

## Section 6: Reference Documentation (7 pages)

### 33. Complete Register Reference
**Purpose**: All hardware registers with bit details

**Content**:
- Comprehensive reference of all semihosting device registers
- Complete register map table
- Detailed bit-level descriptions
- Register access patterns
- Timing characteristics
- Hardware interface details
- Content synthesized from [[Device Registers]] page

**Source**: semihost.md section 2
**Categories**: Reference, Semihosting

---

### 34. Memory Map Examples
**Purpose**: Calculated layouts for various CPU sizes

**Content**:
- **8-bit CPU Example** (theoretical)
  - Address space: 256 bytes
  - Reserved start: 0xF0 (16 bytes)
  - Calculations and layout
  - Limitations and notes
- **16-bit CPU Example** (6502, Z80, 6809)
  - Address space: 64KB
  - Reserved start: 0xFF00 (256 bytes)
  - VRAM: 0xFE00-0xFEFF (512 bytes)
  - Semihosting: 0xFC00-0xFDFF (1024 bytes)
  - Available RAM: 0x0200-0xFBFF (63,488 bytes)
  - Full memory map table
- **24-bit CPU Example** (68000 in 16MB mode)
  - Address space: 16MB
  - Reserved start: 0xFFF000 (4KB)
  - VRAM: 0xFFFE00-0xFFFEFF (512 bytes)
  - Semihosting: 0xFFFA00-0xFFFDFF (1024 bytes)
  - Available RAM: 0x002000-0xFFF9FF (~16MB)
  - Full memory map table
- **32-bit CPU Example** (68000, ARM, i386, MIPS)
  - Address space: 4GB
  - Reserved start: 0xFFFF0000 (64KB)
  - VRAM: 0xFFFFFE00-0xFFFFFFFF (512 bytes)
  - Semihosting: 0xFFFFFA00-0xFFFFFDFF (1024 bytes)
  - Available RAM: 0x00000200-0xFFFFF9FF (~4GB)
  - Full memory map table
- **64-bit CPU Example** (x86-64, ARM64, RISC-V 64)
  - Address space: 16 exabytes (theoretical)
  - Reserved start: 0xFFFFFFFF00000000 (4GB)
  - VRAM: 0xFFFFFFFFFFFFFE00-0xFFFFFFFFFFFFFFFF
  - Semihosting: 0xFFFFFFFFFFFFFA00-0xFFFFFFFFFFFFFDFF
  - Available RAM: massive
  - Full memory map table
- **Formula Reference**
  - reserved_start = 2^addr_bits - 2^(addr_bits/2)
  - vram_addr = reserved_start - 512
  - semihost_addr = reserved_start - 1536
  - available_ram = semihost_addr - load_addr
- **Address Width Table**
  - Quick reference for common CPU address widths
  - 8-bit, 16-bit, 20-bit, 24-bit, 32-bit, 64-bit
  - Reserved region sizes for each

**Source**: zbc.rst section 2.4
**Categories**: Reference, Architecture

---

### 35. Syscall Quick Reference
**Purpose**: Table of all operations

**Content**:
- **Complete Syscall Table** (expanded from page 20)
  - All ARM semihosting syscalls
  - Opcode, name, arguments, return values
  - Brief description for each
- **File I/O Syscalls**
  - SYS_OPEN, SYS_CLOSE, SYS_READ, SYS_WRITE
  - SYS_SEEK, SYS_FLEN
  - SYS_REMOVE, SYS_RENAME, SYS_TMPNAM
- **Console I/O Syscalls**
  - SYS_WRITEC, SYS_WRITE0
  - SYS_READC, SYS_GET_CMDLINE
- **Timing Syscalls**
  - SYS_CLOCK, SYS_TIME
  - SYS_ELAPSED, SYS_TICKFREQ
- **Utility Syscalls**
  - SYS_ISERROR, SYS_ISTTY
  - SYS_ERRNO, SYS_HEAPINFO
  - SYS_SYSTEM
- **Control Syscalls**
  - SYS_EXIT, SYS_EXIT_EXTENDED
- **Standard File Descriptors**
  - 0: stdin, 1: stdout, 2: stderr
- **Open Mode Flags Reference**
  - All 12 modes with descriptions
- **Return Value Conventions**
  - Success values
  - Error indicators (-1)
  - errno handling
- **Quick Usage Tips**
  - Most common operations
  - Error checking patterns
  - Typical workflows

**Source**: semihost.md section 8
**Categories**: Reference, Semihosting

---

### 36. RIFF Chunk Reference
**Purpose**: All chunk types and formats

**Content**:
- **Chunk Type Summary Table**
  - FourCC, Name, Direction, Size, Description
  - All current and future chunk types
- **CNFG Chunk Detailed**
  - Format, fields, field sizes
  - int_size, ptr_size, endianness
  - Examples for various CPUs
- **CALL Chunk Detailed**
  - Format, opcode, sub-chunks
  - Nesting rules
- **PARM Chunk Detailed**
  - Format, parameter types
  - Type 0x01 (integer), Type 0x02 (pointer)
  - Size calculations
- **DATA Chunk Detailed**
  - Format, data types
  - Type 0x01 (binary), Type 0x02 (string)
  - Padding rules
- **RETN Chunk Detailed**
  - Format, result field, errno field
  - Sub-chunk handling
  - Examples by syscall type
- **ERRO Chunk Detailed**
  - Format, error codes
  - Error messages
- **Future Chunks** (from protocol extensions)
  - STRM: Streaming data
  - EVNT: Host-initiated events
  - ABRT: Abort operation
  - META: Device capabilities
- **Chunk Nesting Rules**
  - Maximum depth: 2 levels
  - Valid nesting patterns
  - Invalid nesting patterns
- **Chunk Alignment Requirements**
  - Even boundary padding
  - Padding byte handling
  - Size field calculations
- **Endianness Handling**
  - RIFF structure: Always LE
  - Data values: Guest native
  - Conversion requirements

**Source**: semihost.md sections 3, 9, 10
**Categories**: Reference, Semihosting

---

### 37. Supported CPU List
**Purpose**: Table from zbc_status.csv

**Content**:
- **MAME CPU Support Table**
  - All CPUs with status=working
  - Shortname, type constant, full name
  - Header file reference
- **CPU Families**
  - **6502 Family**: 6502, 65C02, 65816, etc.
  - **Z80 Family**: Z80, Z180, etc.
  - **68000 Family**: 68000, 68020, 68030, 68040, 68060
  - **ARM Family**: ARM7, ARM9, ARM11, Cortex-M, Cortex-A
  - **x86 Family**: 8086, 80286, 80386, 80486, Pentium
  - **MIPS Family**: Various MIPS cores
  - **PowerPC Family**: 403, 601, 603, 750, etc.
  - **SPARC Family**: Various SPARC cores
  - **Other Architectures**: Many others
- **Status Statistics**
  - Total CPUs discovered: ~600
  - Working: ~300
  - Broken (various reasons): ~300
- **CPU Architecture Notes**
  - Special requirements
  - Known limitations
  - Specialization status
- **Finding Your CPU**
  - How to check if CPU is supported
  - Command: `mame -listfull zbc*`
  - Checking zbc_status.csv
- **Adding Support for New CPUs**
  - Link to [[Adding CPU Support in MAME]]
  - Brief overview of process
- **Platform-Agnostic Note**
  - List is MAME-specific
  - ZBC specification supports any CPU
  - Other implementations may support different CPUs

**Source**: zbc.rst section 3, zbc_status.csv
**Categories**: Reference, MAME Implementation

---

### 38. Error Codes and Errno Values
**Purpose**: Complete error reference

**Content**:
- **ERRO Chunk Error Codes**
  - 0x01: Invalid chunk structure
  - 0x02: Malformed RIFF format
  - 0x03: Missing CNFG chunk
  - 0x04: Unsupported opcode
  - 0x05: Invalid parameter count
  - 0x06-0xFFFF: Reserved
  - Detailed description for each
- **POSIX errno Values**
  - Common errno values with descriptions
  - ENOENT (2): No such file or directory
  - EACCES (13): Permission denied
  - EBADF (9): Bad file descriptor
  - EINVAL (22): Invalid argument
  - EIO (5): Input/output error
  - ENOSPC (28): No space left on device
  - EEXIST (17): File exists
  - EISDIR (21): Is a directory
  - ENOTDIR (20): Not a directory
  - ENOMEM (12): Cannot allocate memory
  - Many others
- **errno Encoding in RETN Chunk**
  - Always 4 bytes (32-bit)
  - Always little-endian
  - 0 = success
  - >0 = error code
- **Error Handling Patterns**
  - Check return value for -1
  - Read errno from RETN chunk
  - Convert errno to little-endian if needed
  - Look up meaning in reference table
- **Platform-Specific errno**
  - POSIX standard values
  - Some values may vary by host OS
  - Use standard values when possible
- **Error Message Best Practices**
  - Check errno for details
  - Log error messages from ERRO chunks
  - Provide context in application errors
  - Use SYS_ERRNO for last error

**Source**: semihost.md sections 3 (ERRO), 8 (ARM semihosting)
**Categories**: Reference, Semihosting

---

### 39. Glossary
**Purpose**: Technical terms and abbreviations

**Content**:
- **A**
  - **Address Space**: Range of memory addresses CPU can access
  - **ARM Semihosting**: Standard semihosting protocol from ARM
  - **ASIC**: Application-Specific Integrated Circuit
- **B**
  - **Big Endian**: MSB-first byte order
  - **Binary**: Executable program without headers
  - **BKPT**: Breakpoint instruction (ARM)
- **C**
  - **Cache Coherency**: Ensuring cached data matches memory
  - **CNFG Chunk**: Configuration chunk in RIFF protocol
  - **CPU**: Central Processing Unit
- **D**
  - **DATA Chunk**: Binary data or string chunk
  - **DMA**: Direct Memory Access
  - **DOORBELL**: Trigger register for semihosting
  - **DRC**: Dynamic Recompilation
- **E**
  - **Endianness**: Byte order in multi-byte values
  - **errno**: POSIX error number
  - **ERRO Chunk**: Error response chunk
- **F**
  - **FPGA**: Field-Programmable Gate Array
  - **FourCC**: Four-Character Code (chunk identifier)
- **G**
  - **Guest**: Emulated or target system
- **H**
  - **Harvard Architecture**: Separate code and data address spaces
  - **Host**: Host system running emulator
- **I**
  - **int_size**: Size of native integer type (CNFG field)
  - **IRQ**: Interrupt Request (maskable)
- **J**
  - **JP1**: Jumper for VSync interrupt configuration
- **L**
  - **LE**: Little Endian (LSB first)
  - **Little Endian**: LSB-first byte order
  - **Load Address**: Memory address where program loads
- **M**
  - **MAME**: Multiple Arcade Machine Emulator
  - **MC6847**: Motorola video display generator
  - **MMU**: Memory Management Unit
- **N**
  - **NMI**: Non-Maskable Interrupt
- **P**
  - **PARM Chunk**: Parameter value chunk
  - **PDP Endian**: Middle-endian byte order (PDP-11)
  - **ptr_size**: Size of pointer type (CNFG field)
- **Q**
  - **Quickload**: MAME feature for loading raw binaries
- **R**
  - **RAM**: Random Access Memory
  - **Reserved Region**: High memory region for peripherals
  - **RETN Chunk**: Return value chunk
  - **RIFF**: Resource Interchange File Format
  - **RIFF_PTR**: Register holding buffer address
- **S**
  - **Semihosting**: Host I/O services for guest
  - **STATUS**: Device status register
  - **Syscall**: System call (semihosting operation)
- **T**
  - **Template Specialization**: C++ feature for CPU-specific code
- **V**
  - **VDG**: Video Display Generator (MC6847)
  - **VRAM**: Video RAM (MC6847 display memory)
  - **VSync**: Vertical synchronization signal
- **Z**
  - **ZBC**: Zero Board Computer
  - **zbcgen**: ZBC code generation tool

**Source**: All source documents
**Categories**: Reference

---

## Section 7: Developer Documentation (5 pages)

### 40. Contributing to ZBC
**Purpose**: Development workflow

**Content**:
- **Project Organization**
  - ZBC is platform-agnostic specification
  - Multiple implementations possible
  - MAME implementation is reference
  - This wiki is authoritative documentation
- **How to Contribute**
  - **Documentation**: Improve wiki pages, fix errors, add examples
  - **Specification**: Propose extensions to RIFF protocol or ZBC design
  - **MAME Implementation**: Add CPU support, fix bugs, improve zbcgen tool
  - **Other Implementations**: Build ZBC for other emulators or hardware
  - **Testing**: Test programs, report issues, validate implementations
- **Documentation Contributions**
  - Wiki is primary documentation
  - Follow MediaWiki formatting conventions
  - Be clear, concise, technically accurate
  - Link related pages
  - Add examples where helpful
- **Specification Changes**
  - Discuss on appropriate forum/mailing list
  - Maintain backward compatibility when possible
  - Document breaking changes clearly
  - Update wiki after consensus
- **MAME Development Workflow**
  1. Fork MAME repository
  2. Create feature branch
  3. Make changes (code, documentation)
  4. Test thoroughly
  5. Submit pull request
  6. Address review feedback
- **Testing Requirements**
  - CPU specializations: Boot without program, load program, validate
  - zbcgen changes: Run full workflow, check generated files
  - Protocol changes: Test with multiple CPUs and endianness
- **Code Style**
  - Follow MAME coding conventions
  - Template specializations in zbc.cpp
  - Comment complex logic
  - Add error messages where helpful
- **Documentation Standards**
  - Update wiki for user-visible changes
  - Update zbc.rst for MAME implementation changes
  - Update semihost.md for protocol changes
  - Keep version-controlled docs in sync
- **Community**
  - MAME development: MAMEdev forums, GitHub
  - ZBC specification: (Define communication channels)
  - Be respectful, constructive, helpful

**Source**: Original content
**Categories**: Developer Documentation

---

### 41. Template Specialization Guide
**Purpose**: Adding new CPUs (detailed technical guide)

**Content**:
- **Overview**
  - Template specialization allows CPU-specific initialization
  - Default: no initialization (boot with uninitialized memory)
  - Specialization: writes reset vectors, exception handlers, idle loop
- **When to Specialize**
  - CPU requires reset vector setup
  - CPU needs exception handlers
  - Want idle loop for clean boot
  - CPU has complex boot requirements
- **Specialization Signature**
  ```cpp
  template <>
  void zbc_state<CPU_DEVICE_CLASS, LOAD_ADDR>::init_cpu_for_idle(address_space &space)
  ```
- **Parameters**
  - `space`: Address space for writing memory
  - `LOAD_ADDR`: Template parameter (default 0x0200)
- **Common Patterns**
  - **Reset Vector CPUs**: Write vector pointing to load address
  - **Direct Boot CPUs**: Write idle loop at boot address, jump to load address
  - **Exception Table CPUs**: Write full exception vector table
- **Memory Writing**
  - Use `space.write_byte(address, value)` for byte writes
  - Use `space.write_word(address, value)` for word writes (if available)
  - Use `space.write_dword(address, value)` for dword writes (if available)
  - Mind endianness! Use CPU's native byte order
- **Idle Loop Patterns**
  - **Infinite jump to self**: `JMP current_address`
  - **Halt instruction**: `HLT` or equivalent
  - **Wait for interrupt**: `WFI`, `WAIT`, etc.
- **Example Specializations** (detailed)
  - **6502 Family**
    - Reset vector at 0xFFFC-0xFFFD (LE)
    - IRQ vector at 0xFFFE-0xFFFF
    - NMI vector at 0xFFFA-0xFFFB
    - Idle loop: `4C 00 02` (JMP $0200)
  - **Z80 Family**
    - Boot address: 0x0000
    - NMI handler: 0x0066
    - Idle loop at 0x0000: `C3 00 02` (JP 0x0200)
    - NMI handler: `ED 45` (RETN)
  - **68000 Family**
    - Initial SSP: 0x0000-0x0003 (BE, e.g., 0x00010000)
    - Initial PC: 0x0004-0x0007 (BE, e.g., 0x00000200)
    - Idle loop at load address: `4EFA FFFE` (JMP PC-2)
  - **ARM Family**
    - Exception vectors at 0x0000-0x001C
    - Reset vector: 0x0000
    - Each vector: Branch instruction to handler
    - Idle loop: Infinite branch to self
- **Testing Checklist**
  - [ ] Boot without program shows boot screen
  - [ ] CPU doesn't crash on boot
  - [ ] Load program with quickload works
  - [ ] Program executes correctly
  - [ ] JP1 NMI mode doesn't crash
  - [ ] `mame zbcXXXX -validate` passes
- **Debugging**
  - Use MAME debugger to inspect memory
  - Check reset vector values
  - Trace execution from reset
  - Verify endianness of multi-byte values
- **Advanced Topics**
  - Multiple load addresses: Use LOAD_ADDR parameter
  - Different CPU variants: May need multiple specializations
  - Complex boot: Consider boot ROM instead

**Source**: zbc.rst section 6, MAME source
**Categories**: Developer Documentation, MAME Implementation

---

### 42. Protocol Extensions
**Purpose**: Future STRM, EVNT, ABRT, META chunks

**Content**:
- **Design Philosophy**
  - RIFF enables backward-compatible extensions
  - Unknown chunks can be skipped
  - New chunk types don't break old implementations
  - Version negotiation via META chunk (future)
- **Proposed STRM Chunk - Streaming Data**
  - **Purpose**: Large file transfers without full buffer copies
  - **Format**:
    - Chunk ID: 'STRM'
    - call_id (4 bytes): Associates with CALL
    - sequence (4 bytes): Packet sequence number
    - flags (2 bytes): 0x01=more, 0x02=end, 0x04=error
    - reserved (2 bytes)
    - data (variable): Chunk of file data
  - **Use Case**: Reading 100MB file in 4KB chunks
  - **Workflow**:
    1. SYS_READ request with large length
    2. Device returns RETN with partial data
    3. Device sends multiple STRM chunks for remaining data
    4. Guest reassembles from sequence numbers
    5. Final STRM has end flag set
- **Proposed EVNT Chunk - Host-Initiated Events**
  - **Purpose**: Host signals guest asynchronously
  - **Format**:
    - Chunk ID: 'EVNT'
    - event_type (2 bytes): 0x01=signal, 0x02=timer, 0x03=host_message
    - reserved (2 bytes)
    - payload (variable): Event-specific data
  - **Use Case**: Host sends Ctrl-C signal to guest
  - **Workflow**:
    1. Host writes EVNT chunk to guest buffer
    2. Host sets IRQ_STATUS bit
    3. Guest IRQ handler reads EVNT
    4. Guest processes event (e.g., call signal handler)
- **Proposed ABRT Chunk - Abort Operation**
  - **Purpose**: Cancel pending async operation
  - **Format**:
    - Chunk ID: 'ABRT'
    - call_id (4 bytes): Operation to cancel
    - reason (2 bytes): 0x01=user, 0x02=timeout, 0x03=error
    - reserved (2 bytes)
  - **Use Case**: Cancel slow file operation
  - **Workflow**:
    1. Guest issues CALL, continues other work
    2. Guest decides to cancel (timeout, user action)
    3. Guest writes ABRT chunk with call_id
    4. Device stops processing, returns RETN with error
- **Proposed META Chunk - Device Capabilities**
  - **Purpose**: Query device features and limits
  - **Format**:
    - Chunk ID: 'META'
    - query_type (2 bytes): 0x01=version, 0x02=features, 0x03=limits
    - reserved (2 bytes)
    - query_data (variable): Query-specific
  - **Response**: META chunk with results
  - **Use Case**: Guest queries max buffer size, supported syscalls
  - **Workflow**:
    1. Guest writes META query
    2. Device responds with META containing:
       - Protocol version
       - Supported syscalls bitmap
       - Max buffer size
       - Feature flags
- **Extended CNFG (Version 2)**
  - **Additional fields**:
    - protocol_version (1 byte): 0x01=v1, 0x02=v2
    - feature_flags (4 bytes):
      - Bit 0: ASYNC_OPS (supports async operations)
      - Bit 1: STREAMING (supports STRM chunks)
      - Bit 2: HOST_EVENTS (supports EVNT chunks)
      - Bit 3: LARGE_FILES (supports 64-bit file offsets)
      - Bits 4-31: Reserved
    - extensions (8 bytes): Reserved for future use
- **Backward Compatibility**
  - Old guests: Send CNFG v1, work as before
  - New guests: Send CNFG v2, negotiate features
  - Old devices: Ignore unknown chunks, work with subset
  - New devices: Support both CNFG v1 and v2
- **Implementation Notes**
  - Feature negotiation via META or extended CNFG
  - Guest probes capabilities before using new features
  - Device returns ERRO for unsupported features
  - Graceful degradation

**Source**: semihost.md section 9
**Categories**: Developer Documentation, Semihosting

---

### 43. Known Limitations
**Purpose**: Excluded CPU types, validation issues

**Content**:
- **CPUs Excluded from MAME ZBC**
  - **Microcontrollers with Internal ROM/RAM**
    - PICs, AVR, 8051, TMS7000, etc.
    - Require internal memory regions not provided by ZBC
    - Cannot be emulated with external RAM only
    - Status: needs_internal_rom
  - **CPUs Requiring Dependent Devices**
    - PlayStation 2 VU units (Vector Units)
    - Emotion Engine cores
    - Require additional support devices
    - Cannot function standalone
    - Status: needs_dependent_device
  - **CPUs with Broken DRC**
    - Various PowerPC cores (403GA, 601, 603, etc.)
    - DRC static handlers cause infinite loops
    - Work in interpreter mode but too slow
    - Status: broken_drc
  - **Non-CPU Devices**
    - DMA controllers (listed as CPUs in MAME)
    - Graphics interfaces
    - Accidentally included in CPU list
    - Status: not_cpu
  - **Validation Failures**
    - Some CPUs crash during `mame -validate`
    - Emulation bugs or initialization issues
    - Status: broken_validate
  - **Driver Name Limits**
    - MAME has 16-character driver name limit
    - Some CPU shortnames too long for `zbc` prefix
    - Status: disabled (name length)
- **ZBC Specification Limitations**
  - **Single Address Space**
    - Assumes unified address space
    - Harvard architectures: Code and data separate
    - Workaround: Map both spaces if possible
  - **No Memory Protection**
    - No MMU emulation
    - Programs can write anywhere
    - No user/supervisor mode separation
  - **Fixed Peripheral Addresses**
    - VRAM and semihosting at calculated addresses
    - Cannot be moved without code changes
    - May conflict with CPU-specific memory requirements
  - **Limited Display**
    - Only 32x16 text mode
    - No graphics, colors, or attributes
    - Suitable for console output only
  - **No Real-Time Clock**
    - Timing via semihosting (host-dependent)
    - No hardware timer peripheral
    - JP1 VSync provides ~60Hz interrupt only
- **MAME Implementation Limitations**
  - **Automated Testing Only**
    - zbcgen validates compilation and MAME validation
    - Does not test actual execution
    - No automated program testing
  - **Template Instantiation Overhead**
    - Hundreds of template instantiations
    - Increases compile time
    - Increases binary size
  - **Manual Specializations**
    - CPU-specific init requires manual coding
    - Not automatically generated
    - Limited coverage (6502, Z80, 68000 currently)
- **Semihosting Limitations**
  - **Performance Overhead**
    - Host I/O is slow (compared to guest memory)
    - Polling wastes CPU cycles
    - Not suitable for production use
  - **Sandboxing**
    - File access restricted to configured directory
    - Cannot access arbitrary host files
    - Good for security, may limit functionality
  - **SYS_SYSTEM Concerns**
    - Can execute host commands (security risk)
    - May be disabled in implementations
    - Use with caution
- **Workarounds and Future Work**
  - Internal ROM CPUs: Provide ROM image loading mechanism
  - Harvard architectures: Support separate code/data spaces
  - DRC issues: Fix underlying MAME CPU emulation
  - Validation failures: Debug and fix emulation bugs
  - Display limitations: Add graphics mode (future)
  - Real-time clock: Add timer peripheral (future)

**Source**: zbc.rst section 9
**Categories**: Developer Documentation, Reference

---

### 44. Future Enhancements
**Purpose**: Roadmap

**Content**:
- **Short-Term Enhancements**
  - **More CPU Specializations**
    - ARM, MIPS, SPARC, PowerPC families
    - RISC-V, AVR (with external ROM), PDP-11
    - Expand coverage beyond 6502/Z80/68000
  - **Improved zbcgen Tool**
    - Better error detection
    - Automatic retry on failure
    - Integration with MAME build system
    - CI/CD pipeline integration
  - **More Test Programs**
    - Test suite for each CPU
    - Automated execution testing
    - Performance benchmarks
  - **Documentation Improvements**
    - More code examples (after initial wiki complete)
    - Video tutorials
    - Porting guides for common toolchains
- **Medium-Term Enhancements**
  - **Protocol Extensions**
    - Implement STRM (streaming data)
    - Implement META (capability negotiation)
    - Extended CNFG with feature flags
    - 64-bit file offsets for large files
  - **Additional Peripherals**
    - Real-time clock/timer device
    - Simple GPIO for LED/button simulation
    - Serial UART for console I/O
    - Storage controller (SD card emulation)
  - **Graphics Enhancements**
    - MC6847 graphics modes (in addition to text)
    - Color support
    - Higher resolution text (40x25, 80x25)
    - Bitmap graphics mode
  - **Interrupt Enhancements**
    - Multiple interrupt sources
    - Programmable interrupt controller
    - Timer interrupts (separate from VSync)
- **Long-Term Enhancements**
  - **Multi-CPU Support**
    - Multiple CPUs on same system
    - Inter-processor communication
    - Shared memory
  - **Networking**
    - Simple network stack
    - UDP/TCP via semihosting
    - Network boot (TFTP)
  - **Storage**
    - Block device emulation
    - Filesystem support
    - Persistent storage
  - **Hardware Implementations**
    - Reference FPGA design
    - Open-source ASIC design
    - Development boards
  - **Performance Optimizations**
    - Asynchronous semihosting (non-blocking)
    - DMA transfers
    - Interrupt-driven I/O
- **Integration Opportunities**
  - **Toolchain Integration**
    - GCC/Clang target support
    - Newlib/picolibc ZBC board support
    - Debugger integration (GDB)
  - **Educational Use**
    - Course materials
    - Textbook adoption
    - Online tutorials and labs
  - **Compiler Testing**
    - Add to compiler test suites
    - Standard test platform
    - CI/CD integration
  - **Other Emulators**
    - QEMU ZBC machine type
    - Unicorn engine support
    - RetroArch ZBC core
- **Community Development**
  - Establish project governance
  - Define contribution process
  - Create mailing list or forum
  - Regular releases
  - Versioning scheme
- **Standards and Compatibility**
  - Formalize ZBC specification (v1.0)
  - Compliance test suite
  - Certification program
  - Hardware design guidelines

**Source**: zbc.rst section 10, original content
**Categories**: Developer Documentation

---

## Section 8: MediaWiki Templates (4 pages)

### 45. Template:Warning
**Purpose**: Warning message template

**Content**:
```mediawiki
<div style="border: 2px solid #ff0000; background-color: #ffe0e0; padding: 10px; margin: 10px 0;">
'''⚠ WARNING:''' {{{1}}}
</div>
```

**Usage**:
```mediawiki
{{Warning|This operation may cause data loss.}}
```

**Categories**: Templates

---

### 46. Template:Note
**Purpose**: Informational note template

**Content**:
```mediawiki
<div style="border: 1px solid #0066cc; background-color: #e0f0ff; padding: 10px; margin: 10px 0;">
'''ℹ NOTE:''' {{{1}}}
</div>
```

**Usage**:
```mediawiki
{{Note|Remember to flush the data cache before triggering semihosting.}}
```

**Categories**: Templates

---

### 47. Template:SeeAlso
**Purpose**: Cross-reference template

**Content**:
```mediawiki
<div style="border: 1px solid #cccccc; background-color: #f9f9f9; padding: 10px; margin: 10px 0;">
'''See also:'''
{{#if:{{{1|}}}|[[{{{1}}}]]}}{{#if:{{{2|}}}|, [[{{{2}}}]]}}{{#if:{{{3|}}}|, [[{{{3}}}]]}}{{#if:{{{4|}}}|, [[{{{4}}}]]}}{{#if:{{{5|}}}|, [[{{{5}}}]]}}
</div>
```

**Usage**:
```mediawiki
{{SeeAlso|Memory Layout and Addressing|Device Registers|Operation Modes}}
```

**Categories**: Templates

---

### 48. Template:ZBC Navigation
**Purpose**: Main navigation box for all pages

**Content**:
```mediawiki
{| class="wikitable" style="width: 100%; background-color: #f0f0f0;"
|-
! colspan="6" style="background-color: #003366; color: white;" | Zero Board Computer Documentation
|-
! Foundation
! Architecture
! Semihosting
! Implementation
! User Docs
! Reference
|-
| style="vertical-align: top;" |
* [[What is Zero Board Computer?]]
* [[Design Goals and Use Cases]]
* [[Getting Started]]
* [[Key Concepts]]
| style="vertical-align: top;" |
* [[System Overview]]
* [[Memory Layout and Addressing]]
* [[Video Display (MC6847)]]
* [[Interrupt System (JP1 Jumper)]]
* [[CPU Support and Initialization]]
* [[Quickload System]]
| style="vertical-align: top;" |
* [[Semihosting Overview]]
* [[Device Registers]]
* [[RIFF Protocol Fundamentals]]
* [[CNFG Chunk]]
* [[CALL and PARM Chunks]]
* [[DATA Chunk]]
* [[RETN and ERRO Chunks]]
* [[Operation Modes]]
* [[Syscall Reference]]
| style="vertical-align: top;" |
* [[MAME Implementation Overview]]
* [[zbcgen Tool]]
* [[Hardware Implementation Guide]]
* [[Emulator Implementation Guide]]
* [[Implementation Checklist]]
| style="vertical-align: top;" |
* [[Running ZBC Systems]]
* [[Writing Programs for ZBC]]
* [[Using Semihosting Services]]
* [[Troubleshooting]]
| style="vertical-align: top;" |
* [[Complete Register Reference]]
* [[Memory Map Examples]]
* [[Syscall Quick Reference]]
* [[RIFF Chunk Reference]]
* [[Supported CPU List]]
* [[Error Codes and Errno Values]]
* [[Glossary]]
|}
```

**Usage**:
Add to bottom of every content page:
```mediawiki
{{ZBC Navigation}}
```

**Categories**: Templates

---

## COMPLETE OUTLINE - 48 Pages

This outline provides detailed content specifications for all 48 wiki pages covering the complete Zero Board Computer documentation. Each page includes:

- Clear purpose statement
- Detailed content breakdown
- Source material references
- Category tags
- Cross-references to related pages

The content is organized for heavy reorganization (not direct conversion) and focuses on making the wiki the authoritative reference for the ZBC specification.
