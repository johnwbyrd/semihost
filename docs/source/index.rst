ZBC Semihosting
===============

Reference implementation of a semihosting device for the
`Zero Board Computer <https://www.zeroboardcomputer.com>`_.

This project provides C libraries for both implementing a virtual
semihosting device (host side) and marshaling function calls to and
from the device (client/guest side). A semihosting device provides
file I/O, console, and time services to guest programs via a simple
memory-mapped device register interface.

Your software gains immediate access to filesystem and timekeeping
services without explicitly porting device drivers to your new
emulator or architecture.

Who Is This For?
----------------

- **Bare-metal developers** -- need filesystem, console, and time
  services immediately on a new architecture
- **Toolchain and SDK developers** -- run compiler test suites on
  emulated hardware without writing filesystem drivers
- **Emulator authors** -- add semihosting to your emulator so guest
  programs can access host files
- **libc porters** -- implement ``fopen``/``fread``/``fwrite`` on a
  new target without real device drivers

Features
--------

- Works on any CPU from 8-bit to 64-bit (architecture-agnostic RIFF protocol)
- ARM semihosting compatible syscall numbers
- C90 compliant, extremely portable, zero heap allocation
- Secure (sandboxed) and insecure backends
- GitHub test suite for Ubuntu, macOS, and Windows
- Automatic fuzzing of RIFF parser

Documentation
-------------

.. toctree::
   :maxdepth: 2
   :caption: User Guides

   client-library
   emulator-integration
   specification

.. toctree::
   :maxdepth: 2
   :caption: API Reference

   api/index

Building
--------

.. code-block:: bash

   cmake -B build && cmake --build build

Testing
-------

.. code-block:: bash

   ctest --test-dir build

Tests run on Linux, macOS, and Windows. CI includes AddressSanitizer,
UndefinedBehaviorSanitizer, and continuous fuzzing via ClusterFuzzLite.

Related Projects
----------------

- `MAME <https://github.com/mamedev/mame>`_ -- includes ZBC machine drivers
- `zeroboardcomputer.com <https://www.zeroboardcomputer.com>`_ -- ZBC specification

License
-------

`MIT <https://github.com/jbyrd/semihost/blob/main/LICENSE>`_ (SPDX: ``MIT``)
