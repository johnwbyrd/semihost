Zero Board Computer Semihosting
===============================

The `Zero Board Computer <https://www.zeroboardcomputer.com>` is an
idealized computer architecture, designed for bringing up new toolchains,
new CPUs, and new compilers, quickly and easily.  Its design is simple
and clean; it's easy to emulate, debug, and understand.

Central to ZBC's architecture is a semihosting device, which provides an
ARM-like standardized interface for guest programs to call host services.
This repository is a reference implementation of that semihosting device,
which you can use to add semihosting support to your emulator or
toolchain.

Whether you're building a bare-metal toolchain for a new CPU, writing an
emulator, or just want to run programs on exotic hardware, ZBC semihosting
makes it easy to get standard C library features like file I/O, console
output, and time services up and running quickly, without having to 
muck around writing device drivers.

**Full documentation:** https://johnwbyrd.github.io/semihost/

Who Is This For?
----------------

- **Bare-metal developers** -- filesystem, console, and time services
  on a new (or old) architecture, without writing device drivers
- **Toolchain and SDK developers** -- run compiler test suites on
  emulated hardware
- **Emulator authors** -- add semihosting so guest programs can access
  host files
- **libc porters** -- implement ``fopen``/``fread``/``fwrite`` without
  real device drivers

Features
--------

- Works on any CPU from 8-bit to 64-bit (architecture-agnostic RIFF protocol)
- ARM semihosting compatible syscall numbers
- C90 compliant to be friendly with ancient compilers
- Extremely portable
- Does zero heap allocation -- you allocate all your own buffers
- Secure (sandboxed) and insecure backends
- GitHub test suite for Ubuntu, macOS, and Windows
- Automatic fuzzing of RIFF parser

Quick Start
-----------

.. code-block:: bash

   cmake -B build && cmake --build build
   ctest --test-dir build

License
-------

`MIT <LICENSE>`_
