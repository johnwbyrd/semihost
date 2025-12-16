Building
========

Simple CMake-based build system, C90 compliant.

Quick Start
-----------

.. code-block:: bash

   cmake -B build
   cmake --build build
   ctest --test-dir build

Platforms
---------

- Linux, macOS, Windows (MSVC, MinGW)
- Tested on x86_64, ARM64
- Cross-compilation supported (toolchain files)

Options
-------

.. code-block:: bash

   cmake -B build -DZBC_USE_SECCOMP=ON     # Linux seccomp sandbox
   cmake -B build-fuzz -DENABLE_FUZZING=ON # Fuzz targets (Clang)

On-Target Tests
---------------

Build cross-compiled tests for emulated platforms:

.. code-block:: bash

   cmake -B build -DZBC_TARGET_TESTS=ON
   cmake -B build -DMAME_PATH=/path/to/mame
   cmake -B build -DZBC_TARGET_PLATFORMS="i386;m6502"

Platform requirements:

- **i386**: gcc-multilib (``apt install gcc-multilib``)
- **m6502**: llvm-mos toolchain (set ``LLVM_MOS_PATH``)
- **MAME**: With ZBC machine support (``zbci386``, ``zbcm6502``)

Sanitizers
----------

.. code-block:: bash

   cmake -B build-san -DCMAKE_BUILD_TYPE=Debug
   cmake --build build-san                 # ASan/UBSan

Targets
-------

- ``zbc_semi_client``: Guest library
- ``zbc_semi_host``: Host library
- ``zbc_tests``: Unit tests
- Fuzzers: ``fuzz_riff_parser``

Install
-------

.. code-block:: bash

   cmake --install build --prefix /usr/local

See CMakeLists.txt for details.
