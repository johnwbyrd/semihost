Testing
========

Comprehensive test suite verifies correctness, memory safety, endianness,
pointer sizes, and round-trip integration.

Test Harness
------------

- **Plain C**: No frameworks, custom macros (TEST_ASSERT_EQ, etc.).
- **Guarded Buffers**: Stack-allocated with canaries for overflow detection.
- **Zero Alloc Check**: Static scan forbids malloc/free in libraries.
- **Mock Device/Memory**: Client/host integration without real hardware.

Test Categories
---------------

1. **Client Builder/Parser**: RIFF construction/parsing, buffer boundaries.
2. **Host Parser/Response**: CALL/RETN/ERRO handling.
3. **Round-Trip**: Full client-host via mock memory (all syscalls).
4. **Endianness/Pointer Sizes**: Manual vectors for cross-arch.
5. **Errors**: Protocol/host errno propagation.
6. **Memory Safety**: Canary checks, no stomps.

Fuzzing
-------

- **RIFF Parser**: libFuzzer + ClusterFuzzLite (fuzz/fuzz_riff_parser.c).
- **Corpus**: gen_malformed_corpus.py creates invalid inputs.

Sanitizers
----------

CI runs ASan/UBSan on Linux/macOS/Windows.

Run Tests
---------

.. code-block:: bash

   cmake -B build
   cmake --build build
   ctest --test-dir build -V

See `test/TEST_PLAN.rst <test/TEST_PLAN.html>`_ for full details.

Coverage: >90% lines, >80% branches target.
