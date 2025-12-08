ZBC Semihosting
===============

Reference implementation of a semihosting device for the
`Zero Board Computer <https://www.zeroboardcomputer.com>`_.

C libraries for implementing a virtual semihosting device and for
marshaling function calls to and from the device. Provides file I/O,
console, and time services to guest programs via a memory-mapped
device register interface.

**Full documentation:** https://jbyrd.github.io/semihost/

Who Is This For?
----------------

- **Bare-metal developers** -- filesystem, console, and time services
  on a new architecture without writing device drivers
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
- C90 compliant, extremely portable, zero heap allocation
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
