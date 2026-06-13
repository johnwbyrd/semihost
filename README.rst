Zero Board Computer
===================

**Working printf, fopen, and a real filesystem on any CPU -- real,
emulated, or yet to be designed. On day one, before any hardware
exists.**

Embedded bring-up has a chicken-and-egg problem. You can't debug
your firmware until the serial port works. You can't test your
filesystem until the flash driver works. The first six weeks of
every new-hardware project go to fighting your way to a working
printf.

Zero Board Computer (ZBC) eliminates the chicken-and-egg by giving
any CPU file input/output, console, and clock services through a
32-byte memory-mapped device. The same firmware runs against your
simulator, against a field-programmable gate array (FPGA)
prototype, against the manufactured chip, and against a QEMU or a
MAME virtual machine, because the contract is the same everywhere:
read and write memory.

**Why this matters, who it's for, and how it works:**
https://johnwbyrd.github.io/zbc/introduction.html

**Five-minute slide intro:** https://johnwbyrd.github.io/zbc/presentation-intro/

**Full documentation and reference:** https://johnwbyrd.github.io/zbc/

**Wiki and community:** https://www.zeroboardcomputer.com/

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
  ``docs/source/specification.rst`` is the canonical protocol spec, and
  `docs/source/documentation-sources.rst
  <docs/source/documentation-sources.rst>`_ records the governance
  boundary between this repo and the wiki at www.zeroboardcomputer.com.
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
