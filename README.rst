Zero Board Computer Semihosting
===============================

Bringing a new CPU to life is a chicken-and-egg problem: you have silicon
(or an emulator) that can compute, but no way to get bytes in or out.
No filesystem. No ``printf``.  No way to run the GCC test suite.  The
classical answer is *semihosting* -- let the host machine (the emulator,
the debugger probe) service the guest's syscalls, so a bare-metal
``fopen`` actually opens a real file on the developer's laptop.

ARM standardized semihosting in the early '90s and it works beautifully
-- if you're on ARM, with a debug probe, using ARM's specific trap
instructions.  Everywhere else, you reinvent it.  ZBC semihosting is the
same idea rebuilt around memory-mapped I/O: a 32-byte register window,
a RIFF buffer in RAM, and a doorbell write.  No trap instructions, no
debugger required, works the same on any CPU.

**Full documentation:** https://johnwbyrd.github.io/zbc/

What's Different
----------------

- **Memory-mapped, not trap-instruction.**  A 32-byte mmio device anyone
  can implement.  Works in a stock emulator with no debug infrastructure;
  works on real silicon with no JTAG probe; an FPGA can implement the
  device directly in hardware.
- **Architecture-agnostic by construction.**  The wire protocol is RIFF
  chunks with guest-declared endianness, so an 8-bit 6502 speaks the same
  language as 64-bit x86.  The repo has live on-target tests for both
  today.
- **ARM-compatible syscall numbers.**  Drop-in with picolibc, newlib, and
  toolchains that already speak ARM semihosting.  You inherit decades of
  libc work instead of inventing a new ABI.

Who Reaches For This
--------------------

- **Bringing up a new CPU or toolchain.**  Drop the C client library into
  your libc port; run the GCC test suite against your simulator on day
  one.  Hours, not weeks.
- **Compiler regression suites across many architectures.**  One
  semihosting implementation for all of them -- the MAME-style "300 CPUs,
  one test harness" model.
- **Adding semihosting to an emulator.**  Expose a 32-byte mmio window;
  link the C++ host library.  An afternoon, not a quarter.
- **FPGA first-boot demos.**  Show ``printf`` working the day after first
  boot, without writing a UART driver.

The Protocol Is the Contract
----------------------------

The single source of truth for the wire format is
`docs/source/specification.rst <docs/source/specification.rst>`_.  The C
and C++ host libraries in this repo are *implementations* of that
contract, and the conformance suite
(`test/conformance/test_conformance.cpp
<test/conformance/test_conformance.cpp>`_) enforces byte-for-byte
equivalence between them on every CI run.  Future Rust bindings or FPGA
implementations conform to the same wire format.  Semihosting stops
being a per-platform reinvention.

What's In This Repo
-------------------

Sources are organized by language so new implementations can slot in as
siblings without disturbing existing ones.  The protocol primitives every
implementation needs (RIFF codec, opcode table, header) live in
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
