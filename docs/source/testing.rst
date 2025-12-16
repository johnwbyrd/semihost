Testing
=======

Comprehensive test suite verifies correctness, memory safety, endianness,
pointer sizes, and round-trip integration.

Host Tests
----------

Tests that run on the build machine (native compilation).

Test Harness
^^^^^^^^^^^^

- **Plain C**: No frameworks, custom macros (TEST_ASSERT_EQ, etc.).
- **Guarded Buffers**: Stack-allocated with canaries for overflow detection.
- **Zero Alloc Check**: Static scan forbids malloc/free in libraries.
- **Mock Device/Memory**: Client/host integration without real hardware.

Test Categories
^^^^^^^^^^^^^^^

1. **Client Builder/Parser**: RIFF construction/parsing, buffer boundaries.
2. **Host Parser/Response**: CALL/RETN/ERRO handling.
3. **Round-Trip**: Full client-host via mock memory (all syscalls).
4. **Endianness/Pointer Sizes**: Manual vectors for cross-arch.
5. **Errors**: Protocol/host errno propagation.
6. **Memory Safety**: Canary checks, no stomps.

Run Host Tests
^^^^^^^^^^^^^^

.. code-block:: bash

   cmake -B build
   cmake --build build
   ctest --test-dir build -V

On-Target Tests
---------------

Cross-compiled tests run on emulated ZBC platforms via MAME.

Test Suite
^^^^^^^^^^

``test/target/zbc_target_test.c`` exercises all 23 semihosting syscalls:

- **File I/O**: OPEN, CLOSE, READ, WRITE, SEEK, FLEN, REMOVE, RENAME, TMPNAM
- **Console**: WRITEC, WRITE0, READC
- **Time**: CLOCK, TIME, ELAPSED, TICKFREQ
- **System**: ISERROR, ISTTY, ERRNO, GET_CMDLINE, HEAPINFO, SYSTEM
- **Exit**: EXIT (used to report test results)

Supported Platforms
^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1

   * - Platform
     - Compiler
     - MAME Machine
   * - i386
     - gcc -m32 (gcc-multilib)
     - zbci386
   * - m6502
     - mos-clang (llvm-mos)
     - zbcm6502

Building On-Target Tests
^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: bash

   cmake -B build -DZBC_TARGET_TESTS=ON
   cmake --build build
   ctest --test-dir build -L target

Requirements:

- **MAME**: With ZBC machine support (set ``MAME_PATH`` if not in PATH)
- **i386**: ``apt install gcc-multilib`` or equivalent
- **m6502**: Install llvm-mos, set ``LLVM_MOS_PATH``

Target Test Harness
^^^^^^^^^^^^^^^^^^^

``test/target/zbc_target_harness.h`` provides bare-metal test macros:

- ``TARGET_INIT()``: Initialize client and test state
- ``TARGET_PRINT(msg)``: Output via SH_SYS_WRITE0
- ``TARGET_ASSERT(cond)``: Assert condition
- ``TARGET_ASSERT_EQ(a, b)``: Assert equality with hex output
- ``TARGET_BEGIN_TEST(name)`` / ``TARGET_END_TEST()``: Test boundaries
- ``TARGET_EXIT()``: Exit with pass/fail status code

Fuzzing
-------

- **RIFF Parser**: libFuzzer + ClusterFuzzLite (fuzz/fuzz_riff_parser.c).
- **Corpus**: gen_malformed_corpus.py creates malformed RIFF inputs.

.. code-block:: bash

   cmake -B build-fuzz -DENABLE_FUZZING=ON -DCMAKE_C_COMPILER=clang
   cmake --build build-fuzz
   ./build-fuzz/fuzz/fuzz_riff_parser corpus/

Sanitizers
----------

CI runs ASan/UBSan on Linux/macOS/Windows:

.. code-block:: bash

   cmake -B build-san -DCMAKE_BUILD_TYPE=Debug
   cmake --build build-san

Coverage
--------

Target: >90% lines, >80% branches.
