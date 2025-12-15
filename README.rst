Zero Board Computer Semihosting
===============================

The Zero Board Computer is the hardware equivalent of **Hello World.**
It's the minimum viable computer -- just enough to prove your CPU and
toolchain work.

The `Zero Board Computer <https://www.zeroboardcomputer.com>` is an
idealized computer architecture, designed for bringing up new toolchains,
new CPUs, and new compilers, quickly and easily.  Its design is simple
and clean; it's easy to emulate, debug, and understand.

Functionally, the ZBC just your favorite CPU, a whole bunch of RAM, 
a super simple (optional) video display, and a semihosting device.

This ZBC semihosting device provides an ARM-like standardized interface,
so that guest programs can request host services such as file I/O and
timekeeping.  This repository is a reference implementation of that
semihosting device, which you can use to add semihosting support to
your favorite emulator or toolchain.

Whether you're building a bare-metal toolchain for a new CPU, writing an
emulator, or compiling an operating system, ZBC semihosting makes it easy
for you to get standard C library features like file I/O, console
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
