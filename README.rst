Zero Board Computer
===================

**One standard for getting bytes in and out of any CPU -- past, present,
or yet to be designed.**

The Zero Board Computer (ZBC) is a small, deliberately boring
specification for a memory-mapped I/O device.  Write your libc, your
operating system, or your test harness against it, and the same code
runs on a Xeon, on a 6502, on a doorbell controller with a few kilobytes
of RAM, on over 200 historical systems through MAME, and on hardware
that doesn't exist yet -- without writing a single device driver.

The word for this is *semihosting*: the guest program (the one running
on the target CPU) borrows file I/O, console output, and other syscalls
from a host machine -- an emulator, a simulator, or a developer's
laptop -- because the guest has no operating system of its own.  ARM
defined a flavor of this in the early '90s; it works, but only on ARM,
only with a debug probe, only via ARM-specific trap instructions.  ZBC
rebuilds the same idea around plain memory-mapped I/O so it travels.

**Full documentation:** https://johnwbyrd.github.io/zbc/

What ZBC Unlocks
----------------

- **Any CPU, from doorbells to data centers.**  The ZBC device is a
  32-byte register window any CPU can read and write.  No special
  instructions, no debug probe, no privileged mode.  An 8-bit
  microcontroller with 4 KB of RAM runs the same protocol an x86-64
  server runs.

- **Build embedded systems before any hardware exists.**  ZBC is a
  standard.  Write your libc, your operating system, or your application
  against it, and you can develop and test the whole stack in an emulator
  with no silicon at all.  When the real chip arrives, the software
  already works.

- **Computer archaeology, made practical.**  ZBC integrates with MAME,
  which emulates over 200 historical systems -- vintage 8- and 16-bit
  micros, arcade-era processors, classic minicomputers, chips you've
  only ever read about in old papers.  Drop a ZBC device into a
  MAME-emulated 6502, Z80, or 68000 system and ``printf``, ``fopen``,
  and the GCC test suite work on hardware that's been out of production
  for decades.

- **Bring up CPUs that don't exist yet.**  Designing a new ISA?  Writing
  a simulator for a research architecture?  ZBC gives the guest side a
  working libc on day one of bringup, before you've decided what your
  trap instructions look like.

How Is This Possible?
---------------------

ZBC trades trap instructions for a 32-byte memory-mapped device.  The
guest writes a small RIFF-formatted buffer into RAM, pokes a doorbell
register, and the host reads the buffer and executes the syscall.
Three consequences:

- **Architecture-agnostic by construction.**  The wire format is RIFF
  chunks with guest-declared endianness, so an 8-bit 6502 speaks the
  same protocol as a 64-bit Xeon.  Live on-target tests for both ship
  in this repo.

- **No debugger required.**  ARM semihosting needs GDB or a hardware
  probe to service the trap.  ZBC works in a stock software emulator
  with zero debug infrastructure, and works on real silicon with no
  JTAG attached.

- **ARM-compatible syscall numbers.**  Same opcode numbering as ARM
  semihosting, so picolibc, newlib, and toolchains that already speak
  ARM semihosting work nearly unchanged.  You inherit decades of libc
  effort instead of inventing a new ABI.

Who Reaches For This
--------------------

- **Bringing up a new CPU or toolchain.**  Drop the C client library
  into your libc port; run the GCC test suite against your simulator
  on day one.
- **Compiler regression suites across many architectures.**  One
  semihosting implementation for every target.
- **Adding semihosting to an emulator.**  Expose a 32-byte mmio window
  and link the C++ host library.  An afternoon, not a quarter.
- **Resurrecting vintage hardware.**  Run modern code on anything MAME
  emulates -- the 1980s home computer in your closet, the arcade board
  in the museum.
- **FPGA first-boot demos.**  ``printf`` working the day after first
  boot, no UART driver.

The Protocol Is the Contract
----------------------------

The single source of truth for the wire format is
`docs/source/specification.rst <docs/source/specification.rst>`_.  The
C and C++ host libraries in this repo are *implementations* of that
contract, and the conformance suite
(`test/conformance/test_conformance.cpp
<test/conformance/test_conformance.cpp>`_) enforces byte-for-byte
equivalence between them on every CI run.  Future Rust bindings or FPGA
implementations conform to the same wire format.  Semihosting stops
being a per-platform reinvention.

What's In This Repo
-------------------

Sources are organized by language so new implementations can slot in as
siblings without disturbing existing ones.  The protocol primitives
every implementation needs (RIFF codec, opcode table, header) live in
``shared/``; each language tier owns its own client, host, and tests on
top of that base.

- `include/shared/ <include/shared>`_ + `src/shared/ <src/shared>`_ --
  protocol primitives (``zbc_protocol.h``, RIFF codec, opcode table);
  compiled into every host and client so the wire format can't drift.
- `include/c/ <include/c>`_ + `src/c/ <src/c>`_ -- C90 reference client
  and host libraries plus the ANSI/stdio backend and platform sandbox
  code.
- `include/cpp/zbc/ <include/cpp/zbc>`_ + `src/cpp/ <src/cpp>`_ -- C++17
  host library (``zbc::Device`` / ``zbc::Backend`` / ``zbc::Policy``)
  intended for embedding in emulators.
- `test/ <test>`_ -- ``c/`` and ``cpp/`` host tests, ``conformance/`` for
  C-vs-C++ wire-protocol equivalence, ``target/`` for cross-compiled
  on-emulator runs (i386 under QEMU, 6502 under MAME), ``common/`` for
  the shared test harness.
- `docs/ <docs>`_ -- Sphinx site;
  ``docs/source/specification.rst`` is the canonical protocol spec.
- `fuzz/ <fuzz>`_ -- libFuzzer target and corpora for the RIFF parser.
- `web/ <web>`_ -- MediaWiki content for www.zeroboardcomputer.com.

Quick Start
-----------

.. code-block:: bash

    # Basic build + host tests
    cmake -B build && cmake --build build && ctest --test-dir build

    # With on-target tests (requires MAME + cross-compilers)
    cmake -B build -DZBC_TARGET_TESTS=ON

    # With seccomp sandbox (Linux)
    cmake -B build -DZBC_USE_SECCOMP=ON

    # With fuzzing (requires Clang)
    cmake -B build-fuzz -DENABLE_FUZZING=ON

Status
------

- CI matrix: Ubuntu, macOS, and Windows; gcc, clang, and MSVC.
- ASan + UBSan and seccomp-sandbox jobs gate every push.
- Continuous RIFF parser fuzzing via libFuzzer + ClusterFuzzLite.
- Zero heap allocation in the libraries, statically verified in
  `test/CMakeLists.txt <test/CMakeLists.txt>`_.

License
-------

`MIT <LICENSE>`_
