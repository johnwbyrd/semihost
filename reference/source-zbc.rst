Zero Board Computer (ZBC)
==========================

.. contents:: :local:


1. Overview
-----------

1.1 Purpose
~~~~~~~~~~~

The ZBC (Zero Board Computer) system provides minimal, standardized test
environments for all emulatable CPU architectures in MAME. Each ZBC variant
consists of a CPU, RAM, text display, and semihosting interface, allowing
programs to be loaded and executed via quickload with host I/O support.

The zbcgen tool automates the discovery, generation, and maintenance of ZBC
definitions, serving as the canonical, comprehensive list of CPU support in
MAME with automated quality control.

1.2 Motivation
~~~~~~~~~~~~~~

MAME supports hundreds of CPU architectures across decades of computing history.
Manually maintaining test systems for each CPU is error-prone and incomplete.
The zbcgen system automates:

* **Discovery**: Enumerate all CPU devices from MAME's device registry
* **Generation**: Create minimal test systems for each CPU automatically
* **Validation**: Track which CPUs compile and validate successfully
* **Maintenance**: Update CPU status as MAME evolves

1.3 Design Philosophy
~~~~~~~~~~~~~~~~~~~~~

The ZBC system uses C++ templates to eliminate code duplication. A single
template class (``zbc_state``) is instantiated for each CPU type, with
compile-time customization of load address, clock speed, and video RAM
placement. CPU-specific initialization (reset vectors, exception tables)
is handled via template specialization.

The ``DEFINE_ZBC`` macro generates complete machine variants from a single
line, creating unique classes and registering them with MAME.


2. Architecture
---------------

2.1 System Components
~~~~~~~~~~~~~~~~~~~~~

The zbcgen system consists of:

**Source Files (Manual)**:
  * ``src/mame/zbc/zbc.cpp`` - Template classes, helper functions, CPU-specific specializations

**Generated Files (Automatic)**:
  * ``src/mame/zbc/zbcgen.hpp`` - All CPU header ``#include`` directives (alphabetical)
  * ``src/mame/zbc/zbcgen.ipp`` - All ``DEFINE_ZBC()`` macro invocations (included by zbc.cpp)

**Build Tools**:
  * ``scripts/build/zbcgen.py`` - Main generator script
  * ``src/mame/zbc/zbc_status.csv`` - Knowledge base tracking CPU status (version controlled)

**MAME Infrastructure**:
  * Enhanced ``-listcpu`` command - Outputs CPU metadata
  * Enhanced device type system - Tracks type constant names at runtime
  * ``src/mame/mame.lst`` - Driver registration list (updated by zbcgen.py)

2.2 ZBC Template System
~~~~~~~~~~~~~~~~~~~~~~~

Each ZBC system includes:

* **RAM**: Sized automatically based on CPU address space width
* **MC6847 VDG**: Text display (32x16 characters) at top of address space
* **Semihosting**: RIFF-based memory-mapped I/O interface (1024 bytes)
* **Quickload**: Support for loading headerless binary programs
* **CPU Init**: Architecture-specific boot code (reset vectors, etc.)
* **JP1 Jumper**: Configurable VSync interrupt routing (see 2.3)

2.3 VSync Interrupt Configuration (JP1 Jumper)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ZBC hardware includes a 3-position configuration jumper (JP1) that controls
how the MC6847 VDG's vertical sync (field sync) signal is routed to the CPU.
This allows software developers to choose the interrupt behavior that best suits
their application.

**JP1 Jumper Positions**:

* **Position 1-2: Disabled (Default)**

  VSync signal is not connected to CPU interrupts. The MC6847 generates video
  output normally but does not trigger any CPU interrupts.

  **Use case**: Simple test programs, debugging, any code that doesn't need
  periodic timer interrupts. This is the safe default that prevents unexpected
  interrupt-related bugs in programs without proper interrupt handlers.

* **Position 2-3: IRQ (Maskable Interrupt)**

  VSync signal drives the CPU's IRQ line at approximately 60Hz (NTSC) or 62Hz (PAL).
  The CPU can mask (disable) these interrupts using its interrupt disable flag.

  **Use case**: Operating system development, cooperative multitasking, applications
  requiring maskable periodic timing. Suitable for software that needs timer
  interrupts but also needs the ability to temporarily disable them during
  critical sections.

* **Position 3-4: NMI (Non-Maskable Interrupt)**

  VSync signal drives the CPU's NMI line at approximately 60Hz (NTSC) or 62Hz (PAL).
  These interrupts cannot be masked by software and will always fire.

  **Use case**: Hard real-time systems, preemptive multitasking, watchdog timers,
  or applications requiring guaranteed periodic execution. Software **must**
  provide proper NMI handlers or the system will crash.

**MAME Configuration**:

The jumper setting is controlled via MAME's configuration UI or command-line::

    # View/change jumper setting in MAME UI
    mame zbcz80 -quik program.bin
    # Press TAB → Machine Configuration → JP1: VSync Interrupt

    # Set via configuration file (mame.ini or zbcz80.ini)
    # Add under [zbcz80] section:
    # (values: 0=Disabled, 1=IRQ, 2=NMI)

**Technical Details**:

The MC6847 VDG generates a field sync (FS) pulse at the start of each video
frame. In PAL mode (used by ZBC), this occurs at approximately 62.5 Hz based
on the 4.433619 MHz crystal and 312 scanlines per frame. This signal is
connected to a callback that conditionally asserts the configured interrupt
line based on the JP1 jumper setting.

**Historical Context**:

This design mirrors authentic 1980s home computer hardware. For example, the
Tandy Color Computer and Dragon 32/64 routed the MC6847's FS signal through
a PIA (Peripheral Interface Adapter) to the CPU's IRQ line, providing a
system timer without requiring a separate timer IC. The ZBC's jumper-based
approach provides similar functionality while offering flexibility for
different use cases.

**Programming Considerations**:

When using IRQ or NMI modes, programs must:

1. **Install interrupt handlers** at the appropriate vector addresses:

   - Z80 NMI vector: 0x0066
   - 6502 IRQ vector: 0xFFFE-0xFFFF
   - 6502 NMI vector: 0xFFFA-0xFFFB
   - (other CPUs: consult CPU-specific documentation)

2. **Return from interrupt** using the appropriate instruction:

   - Z80 IRQ: RETI (0xED 0x4D)
   - Z80 NMI: RETN (0xED 0x45)
   - 6502: RTI (0x40)
   - (other CPUs: consult CPU-specific documentation)

3. **Acknowledge interrupts** if required by the CPU architecture

Without proper interrupt handlers, enabling JP1 IRQ/NMI modes will cause
system crashes or memory corruption as the CPU repeatedly pushes return
addresses onto the stack without returning.

2.4 Dynamic Memory Layout Calculation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ZBC memory layout is **dynamically calculated** based on CPU address width
using these algorithms:

**Reserved Region Start**::

    reserved_start = 2^addr_bits - 2^(addr_bits/2)

This scales the reserved region proportionally with address space:

* 16-bit (64KB): ``2^16 - 2^8 = 0xFF00`` (256 bytes reserved)
* 24-bit (16MB): ``2^24 - 2^12 = 0xFFF000`` (4096 bytes reserved)
* 32-bit (4GB): ``2^32 - 2^16 = 0xFFFF0000`` (65536 bytes reserved)
* 64-bit (16EB): ``2^64 - 2^32 = 0xFFFFFFFF00000000`` (4GB reserved)

**Video RAM Address**::

    vram_addr = reserved_start - 512

(unless overridden by VRAM_ADDR template parameter)

**Semihosting Buffer Address**::

    semihost_addr = reserved_start - 512 - 1024
                  = reserved_start - 1536

**Available RAM**::

    ram_start = LOAD_ADDR (default 0x0200)
    ram_end = semihost_addr - 1
    available_ram = semihost_addr - LOAD_ADDR

**Example: 16-bit CPU (Z80, 6502, etc.)**::

    Address Space: 16-bit (64KB total)

    reserved_start = 0xFF00
    vram_addr      = 0xFF00 - 512    = 0xFE00
    semihost_addr  = 0xFF00 - 1536   = 0xFC00
    ram_start      = 0x0200
    ram_end        = 0xFBFF
    available_ram  = 0xFC00 - 0x0200 = 63,488 bytes

    Memory Map:
    0x0000-0x01FF   Low memory (zero page, vectors, stack)
    0x0200-0xFBFF   Available RAM (63,488 bytes)
    0xFC00-0xFDFF   Semihosting buffer (1024 bytes)
    0xFE00-0xFEFF   Video RAM (512 bytes)
    0xFF00-0xFFFF   Reserved region (256 bytes)

**Example: 32-bit CPU (i386, 68000, ARM, etc.)**::

    Address Space: 32-bit (4GB total)

    reserved_start = 0xFFFF0000
    vram_addr      = 0xFFFF0000 - 512  = 0xFFFFFE00
    semihost_addr  = 0xFFFF0000 - 1536 = 0xFFFFFA00
    ram_start      = 0x00000200
    ram_end        = 0xFFFFF9FF
    available_ram  = 0xFFFFFA00 - 0x200 = 4,294,965,760 bytes (~4GB)

    Memory Map:
    0x00000000-0x000001FF   Low memory (vectors, boot code)
    0x00000200-0xFFFFF9FF   Available RAM (~4GB)
    0xFFFFFA00-0xFFFFFDFF   Semihosting buffer (1024 bytes)
    0xFFFFFE00-0xFFFFFFFF   Video RAM (512 bytes)
    0xFFFF0000-0xFFFFFFFF   Reserved region (65,536 bytes)

The layout scales automatically across all address space sizes, ensuring
consistent peripheral placement while maximizing available RAM.

2.5 DEFINE_ZBC Macro
~~~~~~~~~~~~~~~~~~~~

The ``DEFINE_ZBC`` macro in ``zbc.cpp`` generates a complete ZBC variant::

    DEFINE_ZBC(m6502_device, M6502, m6502, "MOS Technology 6502")

This expands to:

1. A derived class (``zbc_m6502_state``) inheriting from ``zbc_state<m6502_device>``
2. ROM definition (empty for ZBC systems)
3. MAME machine registration (``COMP`` macro)

Parameters:
  * ``cpu_class``: CPU device class for template instantiation (e.g., ``m6502_device``)
  * ``cpu_type``: MAME device type macro for machine_config (e.g., ``M6502``)
  * ``short_name``: Machine name suffix, creates ``zbc<short_name>`` (e.g., ``m6502`` → ``zbcm6502``)
  * ``display_name``: Human-readable name for UI (e.g., ``"MOS Technology 6502"``)
  * ``...``: Optional template parameter overrides (load_addr, cpu_speed, vram_addr)

Optional parameters allow customization::

    DEFINE_ZBC(pdp1_device, PDP1, pdp1_cpu, "DEC PDP-1 Central Processor", 0x0010, 200000)
    // Sets LOAD_ADDR=0x0010, CPU_SPEED=200kHz


3. Knowledge Base (zbc_status.csv)
----------------------------------

3.1 Schema
~~~~~~~~~~

The CSV file serves as the single source of truth for CPU status::

    shortname,type_constant,class_name,fullname,status,header_file,notes

Fields:
  * ``shortname``: Short identifier (e.g., ``"m6502"``)
  * ``type_constant``: Device type constant (e.g., ``"M6502"``)
  * ``class_name``: C++ device class name (e.g., ``"m6502_device"``)
  * ``fullname``: Human-readable name (e.g., ``"MOS Technology 6502"``)
  * ``status``: Current state (see 3.2)
  * ``header_file``: Include path (e.g., ``"cpu/m6502/m6502.h"``)
  * ``notes``: Freeform text (error messages, reasons for disabled status)

3.2 Status Values
~~~~~~~~~~~~~~~~~

* ``working``: CPU compiles, validates, and runs correctly
* ``broken_compile``: Compilation fails (syntax errors, missing dependencies)
* ``broken_validate``: Compiles but fails ``mame -validate`` checks
* ``broken_header``: Header file not found or doesn't declare device type
* ``disabled``: Manually disabled (requires special configuration, conflicting macros, driver name exceeds MAME limits)
* ``not_cpu``: Device is not a CPU (DMA controller, graphics interface, etc.)
* ``needs_internal_rom``: Requires internal ROM/RAM regions not provided by ZBC (PICs, AVR, 8051, etc.)
* ``needs_dependent_device``: Requires additional dependent devices (PlayStation 2 VU units, Emotion Engine, etc.)
* ``unknown``: Not yet tested (initial state)


4. zbcgen.py Tool
-----------------

4.1 Command-Line Interface
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``zbcgen.py`` script supports multiple operation modes:

**Initial Generation**::

    ./mame -listcpu > /tmp/cpus.txt
    python3 scripts/build/zbcgen.py --scan-mame /tmp/cpus.txt

Creates initial ``zbc_status.csv`` with all CPUs discovered, automatically
determines header files, and sets initial status (``working`` if header found,
``broken_header`` if not).

**Update from Build Errors**::

    make 2>&1 | tee /tmp/build.log
    python3 scripts/build/zbcgen.py --mark-broken-compile /tmp/build.log

Parses compilation errors and marks failing CPUs as ``broken_compile`` in CSV.

**Update from Validation Errors**::

    ./mame -validate 2>&1 | tee /tmp/validate.log
    python3 scripts/build/zbcgen.py --mark-broken-validate /tmp/validate.log

Parses validation errors and marks failing CPUs as ``broken_validate`` in CSV.

**Regenerate Output Files**::

    python3 scripts/build/zbcgen.py --build

Regenerates ``zbcgen.hpp``, ``zbcgen.ipp``, and updates ``mame.lst`` from
current CSV state without changing status values.

4.2 Workflow
~~~~~~~~~~~~

Typical iterative workflow::

    # 1. Generate fresh list from MAME
    ./mame -listcpu > /tmp/cpus.txt
    python3 scripts/build/zbcgen.py --scan-mame /tmp/cpus.txt

    # 2. Generate source files
    python3 scripts/build/zbcgen.py --build

    # 3. Build and capture errors
    make 2>&1 | tee /tmp/build.log
    python3 scripts/build/zbcgen.py --mark-broken-compile /tmp/build.log
    python3 scripts/build/zbcgen.py --build

    # 4. Rebuild with broken CPUs excluded
    make 2>&1 | tee /tmp/build2.log
    python3 scripts/build/zbcgen.py --mark-broken-compile /tmp/build2.log
    python3 scripts/build/zbcgen.py --build

    # 5. Validate working CPUs
    ./mame -validate 2>&1 | tee /tmp/validate.log
    python3 scripts/build/zbcgen.py --mark-broken-validate /tmp/validate.log
    python3 scripts/build/zbcgen.py --build

    # 6. Final build
    make

4.3 Implementation Details
~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Parsing -listcpu Output**:

The enhanced ``-listcpu`` command outputs (on GCC/Clang)::

    Short name:       Device type:      Device class:         Full name:
    m6502             M6502             m6502_device          "MOS Technology 6502"
    z80               Z80               z80_device            "Zilog Z80"

MSVC output omits the device class column::

    Short name:       Device type:      Full name:
    m6502             M6502             "MOS Technology 6502"

The script detects the format and parses accordingly. On MSVC, class names
cannot be inferred, so the script relies on existing CSV data.

**Header File Discovery**:

The script uses a sophisticated device type mapping system:

1. **Load all CPU headers into memory** (``src/devices/cpu/**/*.h``)
2. **Scan for DECLARE_DEVICE_TYPE declarations** using regex pattern::

    DECLARE_DEVICE_TYPE\s*\(\s*(\w+)\s*,

3. **Build mapping** from device type constant to header file path
4. **Look up header** for each CPU's type constant in the mapping

This approach is far more reliable than pattern matching heuristics, as it
directly parses MAME's actual device type declarations. The entire process
loads ~6.3 MB of headers into memory once, then performs in-memory searches.

Example mapping entries::

    M6502 → cpu/m6502/m6502.h
    Z80 → cpu/z80/z80.h
    PENTIUM → cpu/i386/i386.h
    ARM7 → cpu/arm7/arm7.h

CPUs without valid header mappings are marked ``broken_header`` and excluded
from generation.

**Build Error Parsing**:

The script recognizes common GCC/Clang/MSVC error patterns::

    # GCC/Clang
    src/mame/zbc/zbcgen.ipp:123:45: error: 'PENTIUM' was not declared

    # MSVC
    zbcgen.ipp(123): error C2065: 'PENTIUM': undeclared identifier

It extracts the line number, looks up which ``DEFINE_ZBC`` call failed, and
marks that CPU as ``broken_compile`` with the error message in notes.

**Validation Error Parsing**:

The ``mame -validate`` output format::

    Driver zbcm6502 (file zbc.cpp): 1 errors, 0 warnings
    Errors:
    Video screen ':screen' has no refresh rate

Validation errors are stored in the notes field. CPUs with validation errors
are marked ``broken_validate`` and excluded from generated output (commented
out with reason).


5. Integration with MAME Build System
--------------------------------------

5.1 Potential Build Targets
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The zbcgen system could potentially be integrated into MAME's build system
with targets similar to::

    # Makefile additions (example - not currently implemented):
    zbcgen-scan:
        @./mame -listcpu > /tmp/cpus.txt
        @python3 scripts/build/zbcgen.py --scan-mame /tmp/cpus.txt

    zbcgen-mark-broken-compile:
        @python3 scripts/build/zbcgen.py --mark-broken-compile $(BUILD_LOG)

    zbcgen-mark-broken-validate:
        @./mame -validate 2>&1 | tee /tmp/validate.log
        @python3 scripts/build/zbcgen.py --mark-broken-validate /tmp/validate.log

    zbcgen-build:
        @python3 scripts/build/zbcgen.py --build

**Note**: These are illustrative examples. Actual build system integration
would need to be coordinated with MAME's existing build infrastructure.

5.2 mame.lst Integration
~~~~~~~~~~~~~~~~~~~~~~~~~

The ``zbcgen.py --build`` command automatically updates ``src/mame/mame.lst``
with the list of working ZBC drivers. The script:

1. Locates the ``@source:zbc/zbc.cpp`` section in mame.lst
2. Replaces the driver list with only CPUs that have:
   - ``status=working``
   - Valid header files (not missing or broken)
3. Sorts drivers alphabetically by shortname
4. Writes the updated list back to mame.lst

This ensures MAME's driver registration stays synchronized with working ZBC
variants. Broken or disabled CPUs are automatically excluded.

5.3 Continuous Integration
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The zbcgen system could enable automated CPU testing in CI pipelines:

1. PR commits trigger build
2. Build failures could be automatically marked in CSV
3. Validation runs on successful builds
4. Status changes tracked in version control
5. Developers fix broken CPUs or mark as disabled with notes

This would ensure the ZBC driver list stays current as MAME evolves.


6. CPU-Specific Initialization
-------------------------------

6.1 Template Specialization
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Some CPUs require specific boot code to enter an idle loop. The ``init_cpu_for_idle``
function uses template specialization::

    template <typename CPU_TYPE, uint32_t LOAD_ADDR>
    void init_cpu_for_idle(address_space &space) {
        // Default: no initialization
    }

    // 6502 specialization
    template <>
    void init_cpu_for_idle<m6502_device, 0x0200>(address_space &space) {
        // Set reset vector to 0x0200
        space.write_byte(0xfffc, 0x00);
        space.write_byte(0xfffd, 0x02);

        // Idle loop at 0x0200: JMP $0200
        space.write_byte(0x0200, 0x4c);  // JMP absolute
        space.write_byte(0x0201, 0x00);
        space.write_byte(0x0202, 0x02);
    }

Current specializations:
  * **MOS 6502 family**: Reset vector at 0xFFFC, idle loop at load address
  * **Zilog Z80 family**: Boot at 0x0000 with JP to load address, NMI handler at 0x0066
  * **Motorola 68000 family**: Vector table at 0x0000 (initial SP and PC)

6.2 Adding New Specializations
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To add CPU-specific initialization:

1. Identify CPU requirements (reset vector address, boot sequence, exception handlers)
2. Add template specialization in ``zbc.cpp``::

    template <>
    void init_cpu_for_idle<my_cpu_device, 0x0200>(address_space &space) {
        // Set up reset vector
        // Write idle loop at load address
        // Configure any required exception handlers
    }

3. Rebuild and test with ``mame zbcmycpu -quik test.bin``

CPUs without specializations boot with uninitialized memory. They may require
boot code loaded via quickload, or may not execute at all. Check the MC6847
display for system information including memory addresses.


7. Semihosting Integration
---------------------------

7.1 RIFF-Based Semihosting
~~~~~~~~~~~~~~~~~~~~~~~~~~

The ZBC system includes full semihosting support via the MAME semihosting plugin.
Each ZBC variant includes a 1024-byte memory-mapped semihosting buffer placed
just before video RAM (calculated dynamically based on address space size).

The semihosting plugin must be enabled::

    mame zbcm6502 -quik program.bin -plugin semihost

7.2 Semihosting Memory Layout
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The semihosting buffer address is calculated as::

    semihost_addr = get_reserved_start() - 512 - 1024

For a 16-bit CPU, this places the buffer at 0xFC00-0xFDFF.

Programs can use RIFF-based semihosting calls to:
  * Write debug output to console
  * Read/write files on the host filesystem (sandboxed)
  * Get system time and clock information
  * Exit cleanly

See ``plugins/semihost/README.md`` for complete semihosting protocol details.

7.3 Example Semihosting Usage
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

6502 assembly example (write "Hello" to console)::

    ; Semihosting buffer at 0xFC00 (16-bit CPU)
    SEMIHOST = $FC00

    ; Write RIFF header
    LDA #'R'
    STA SEMIHOST+0   ; 'RIFF'
    LDA #'I'
    STA SEMIHOST+1
    ; ... (set up RIFF structure)

    ; Write CALL chunk with SYS_WRITE0 (0x04)
    LDA #$04
    STA SEMIHOST+$18  ; Opcode

    ; Trigger semihosting by writing complete RIFF structure
    ; Plugin polls memory each frame

See semihosting plugin documentation for complete protocol details.


8. Usage Examples
-----------------

8.1 Running a ZBC System
~~~~~~~~~~~~~~~~~~~~~~~~

::

    # Run 6502 ZBC system
    mame zbcm6502 -quik program.bin

    # Run Z80 ZBC system with semihosting
    mame zbcz80 -quik program.bin -plugin semihost

    # Run 68000 ZBC system
    mame zbcm68000 -quik program.bin

The program will be loaded at the configured address (default 0x0200) and
executed. The MC6847 display shows system information on boot, including:
  * CPU name
  * Load address
  * Available RAM size
  * Semihosting buffer address range
  * Video RAM address range

8.2 Writing Test Programs
~~~~~~~~~~~~~~~~~~~~~~~~~~

Example 6502 program using llvm-mos toolchain (displays 'A' on screen)::

    // test.c - Display 'A' on screen using llvm-mos
    // Video RAM at 0xFE00 (16-bit address space)
    // Compile: clang -O2 -target mos --config mos-sim -o test.elf test.c
    // Extract: llvm-objcopy -O binary test.elf test.bin

    volatile char *videoram = (volatile char *)0xFE00;

    int main(void) {
        // Write 'A' to top-left of 32x16 display
        videoram[0] = 'A';

        // Infinite loop
        while (1) {
            // CPU idle
        }

        return 0;
    }

Build and run::

    # Install llvm-mos SDK (https://github.com/llvm-mos/llvm-mos-sdk)
    clang -O2 -target mos --config mos-sim -o test.elf test.c
    llvm-objcopy -O binary --only-section=.text test.elf test.bin
    mame zbcm6502 -quik test.bin

**Important**: Video RAM address varies by CPU address space size. Check the
boot screen or calculate using formulas in section 2.3. For 16-bit CPUs,
VRAM is at 0xFE00. For 32-bit CPUs, VRAM is at 0xFFFFFE00.

8.3 Automated Testing
~~~~~~~~~~~~~~~~~~~~~

The ZBC system enables automated CPU testing::

    # Test all working ZBC drivers
    for driver in $(grep '^zbc' src/mame/mame.lst | grep -A999 '@source:zbc/zbc.cpp' | grep '^zbc'); do
        echo "Testing $driver..."
        timeout 5 ./mame $driver -quik test.bin -seconds_to_run 2
    done

This validates that each CPU boots, loads programs, and executes correctly.


9. Known Limitations
--------------------

CPUs excluded from ZBC coverage:

* **Microcontrollers with internal ROM/RAM** - PICs, AVR, 8051, TMS7000, etc.
  These require internal memory regions not provided by external RAM mapping.

* **CPUs requiring dependent devices** - PlayStation 2 VU units, Emotion Engine cores
  These require additional support devices beyond simple RAM/video.

* **Validation failures** - Sony PlayStation CPUs that crash during MAME validation

* **Non-CPU devices** - DMA controllers, graphics interfaces accidentally included

* **Driver name limits** - CPUs whose driver names exceed MAME's 16-character limit


10. Future Enhancements
-----------------------

Planned improvements:

* **Performance benchmarking**: Standardized test suite to compare CPU emulation speed
* **Extended semihosting**: Additional syscalls for file I/O, networking
* **Conflict resolution**: Better handling of CPUs with conflicting macro definitions
* **Custom configurations**: Support for CPUs requiring special machine_config setup
* **Automated regression testing**: CI integration for detecting CPU emulation bugs
