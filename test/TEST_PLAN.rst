ZBC Semihosting Library Test Plan
==================================

This document describes the comprehensive test suite for the ZBC semihosting
client and host libraries.

Design Principles
-----------------

Zero Dependencies
^^^^^^^^^^^^^^^^^

- Tests use plain C with no external test frameworks (no Unity, no Check, no
  CTest assertions)
- CMake for build orchestration only
- Custom test harness with simple macros

Zero Allocation Verification
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- Libraries must never call ``malloc()``, ``calloc()``, ``realloc()``, or
  ``free()``
- Verified by **static check**: grep/scan library source files for allocation
  calls
- Build will fail if allocation functions are found in library code

Buffer Safety
^^^^^^^^^^^^^

- All tests use stack-allocated buffers with canary bytes before and after
- Canary violations indicate buffer overflows/underflows
- No heap allocation in test buffers -- everything on stack for simplicity

Test Harness Design
-------------------

Core Macros
^^^^^^^^^^^

.. code-block:: c

   /* test_harness.h */

   #define TEST_ASSERT(cond) \
       do { \
           if (!(cond)) { \
               test_fail(__FILE__, __LINE__, #cond); \
               return; \
           } \
       } while(0)

   #define TEST_ASSERT_EQ(a, b) \
       do { \
           if ((a) != (b)) { \
               test_fail_eq(__FILE__, __LINE__, #a, (long)(a), #b, (long)(b)); \
               return; \
           } \
       } while(0)

   #define TEST_ASSERT_MEM_EQ(a, b, n) \
       do { \
           if (memcmp((a), (b), (n)) != 0) { \
               test_fail_mem(__FILE__, __LINE__, #a, #b, (n)); \
               return; \
           } \
       } while(0)

   #define RUN_TEST(name) \
       do { \
           printf("  %-50s ", #name); \
           fflush(stdout); \
           test_##name(); \
           if (g_test_passed) { \
               printf("[PASS]\n"); \
               g_tests_passed++; \
           } else { \
               printf("[FAIL]\n"); \
               g_tests_failed++; \
           } \
           g_test_passed = 1; \
       } while(0)

Guarded Buffer Macro (Stack-Allocated)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: c

   /*
    * Stack-allocated buffer with canary bytes.
    * No heap allocation - portable across all platforms.
    */
   #define CANARY_SIZE   16
   #define CANARY_BYTE   0xDE

   #define GUARDED_BUF(name, size) \
       uint8_t name##_storage[CANARY_SIZE + (size) + CANARY_SIZE]; \
       uint8_t *name = name##_storage + CANARY_SIZE; \
       const size_t name##_size = (size)

   #define GUARDED_INIT(name) \
       do { \
           size_t _i; \
           for (_i = 0; _i < CANARY_SIZE; _i++) { \
               name##_storage[_i] = CANARY_BYTE; \
               name##_storage[CANARY_SIZE + name##_size + _i] = CANARY_BYTE; \
           } \
       } while(0)

   #define GUARDED_CHECK(name) \
       guarded_check_canaries(name##_storage, name##_size)

   /* Returns 0 if canaries intact, non-zero if stomped */
   static int guarded_check_canaries(uint8_t *storage, size_t data_size) {
       size_t i;
       for (i = 0; i < CANARY_SIZE; i++) {
           if (storage[i] != CANARY_BYTE) return 1;  /* Pre-canary stomped */
           if (storage[CANARY_SIZE + data_size + i] != CANARY_BYTE) return 2;
       }
       return 0;
   }

Usage:

.. code-block:: c

   void test_something(void) {
       GUARDED_BUF(buf, 256);  /* 256-byte buffer with canaries */
       GUARDED_INIT(buf);

       /* Use 'buf' as uint8_t*, 'buf_size' is 256 */
       some_function(buf, buf_size);

       TEST_ASSERT(GUARDED_CHECK(buf) == 0);  /* Canaries intact */
   }

Mock Device for Client Testing
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: c

   typedef struct mock_device {
       uint8_t regs[ZBC_REG_SIZE];      /* Simulated register space */
       uint8_t *riff_buf;               /* Points to client's RIFF buffer */
       size_t riff_size;
       int doorbell_count;

       /* Configurable response */
       void (*process_fn)(struct mock_device *dev);
   } mock_device_t;

   void mock_device_init(mock_device_t *dev);
   void mock_device_set_signature(mock_device_t *dev);  /* Sets "SEMIHOST" */
   void mock_device_process_default(mock_device_t *dev);

Mock Memory for Host Testing
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: c

   typedef struct mock_memory {
       uint8_t *data;
       size_t   size;
       int      read_count;
       int      write_count;
   } mock_memory_t;

   uint8_t mock_mem_read_u8(uintptr_t addr, void *ctx);
   void mock_mem_write_u8(uintptr_t addr, uint8_t val, void *ctx);
   void mock_mem_read_block(void *dest, uintptr_t addr, size_t size, void *ctx);
   void mock_mem_write_block(uintptr_t addr, const void *src, size_t size, void *ctx);

Test Categories
---------------

1. Client Builder Tests (``test_client_builder.c``)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

These test the RIFF structure building functions in isolation.

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - Test Name
     - Description
   * - ``builder_start_empty``
     - Start builder with minimal buffer, verify RIFF header
   * - ``builder_start_with_cnfg``
     - Verify CNFG chunk is written first time
   * - ``builder_cnfg_not_duplicated``
     - Second request omits CNFG if already sent
   * - ``builder_begin_call``
     - Verify CALL chunk header with opcode
   * - ``builder_add_parm_int_le``
     - Add integer PARM, verify little-endian encoding
   * - ``builder_add_parm_int_be``
     - Add integer PARM, verify big-endian encoding (if supported)
   * - ``builder_add_parm_uint``
     - Add unsigned integer PARM
   * - ``builder_add_data_binary``
     - Add binary DATA chunk
   * - ``builder_add_data_string``
     - Add string DATA chunk (includes null terminator)
   * - ``builder_add_data_empty``
     - Add empty DATA chunk (size=0)
   * - ``builder_finish_patches_sizes``
     - Verify RIFF and CALL sizes are patched correctly
   * - ``builder_multiple_parms``
     - Add multiple PARM chunks in sequence
   * - ``builder_multiple_data``
     - Add multiple DATA chunks in sequence
   * - ``builder_mixed_chunks``
     - PARM + DATA + PARM ordering preserved
   * - ``builder_riff_padding``
     - Odd-sized DATA gets padding byte

Buffer Boundary Tests
"""""""""""""""""""""

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - Test Name
     - Description
   * - ``builder_exact_fit``
     - Buffer exactly large enough succeeds
   * - ``builder_off_by_one_too_small``
     - Buffer one byte too small fails with ``ZBC_ERR_BUFFER_TOO_SMALL``
   * - ``builder_undersized_for_cnfg``
     - Buffer too small for CNFG fails
   * - ``builder_undersized_for_call``
     - Buffer too small for CALL fails
   * - ``builder_undersized_for_parm``
     - Buffer too small for PARM fails
   * - ``builder_undersized_for_data``
     - Buffer too small for DATA fails
   * - ``builder_sticky_error``
     - After error, subsequent operations also fail
   * - ``builder_no_canary_stomp``
     - Verify canaries intact after all operations

Edge Cases
""""""""""

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - Test Name
     - Description
   * - ``builder_null_buffer``
     - NULL buffer returns error
   * - ``builder_null_state``
     - NULL state returns error
   * - ``builder_zero_capacity``
     - Zero capacity returns error
   * - ``builder_max_parms``
     - 8 PARM chunks (max supported)
   * - ``builder_max_data``
     - 4 DATA chunks (max supported)
   * - ``builder_large_data``
     - 64KB data chunk
   * - ``builder_data_null_with_zero_size``
     - NULL data pointer with size 0 succeeds
   * - ``builder_data_null_with_nonzero_size``
     - NULL data pointer with size > 0 fails

2. Client Response Parser Tests (``test_client_parser.c``)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - Test Name
     - Description
   * - ``parser_retn_simple``
     - Parse RETN with just result + errno
   * - ``parser_retn_with_data``
     - Parse RETN containing DATA chunk
   * - ``parser_retn_with_parms``
     - Parse RETN containing PARM chunks (HEAPINFO)
   * - ``parser_erro_chunk``
     - Parse ERRO response, extract error code
   * - ``parser_skips_cnfg``
     - CNFG in response is skipped correctly
   * - ``parser_result_signed_negative``
     - Negative result values parsed correctly
   * - ``parser_result_max_positive``
     - Maximum positive int value
   * - ``parser_errno_values``
     - Various errno values (0, ENOENT, EINVAL, etc.)

Error Cases
"""""""""""

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - Test Name
     - Description
   * - ``parser_truncated_riff``
     - Buffer shorter than RIFF header
   * - ``parser_bad_riff_magic``
     - 'RIFF' magic missing/corrupted
   * - ``parser_bad_semi_magic``
     - 'SEMI' type missing/corrupted
   * - ``parser_truncated_retn``
     - RETN chunk truncated
   * - ``parser_no_retn_or_erro``
     - Neither RETN nor ERRO present

3. Host Parser Tests (``test_host_parser.c``)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - Test Name
     - Description
   * - ``host_parse_cnfg``
     - Parse CNFG and cache int_size, ptr_size, endianness
   * - ``host_parse_call_simple``
     - Parse CALL with opcode only
   * - ``host_parse_call_with_parms``
     - Parse CALL with PARM sub-chunks
   * - ``host_parse_call_with_data``
     - Parse CALL with DATA sub-chunks
   * - ``host_parse_call_mixed``
     - Parse CALL with PARM + DATA
   * - ``host_extract_parm_int``
     - Extract integer from PARM chunk
   * - ``host_extract_parm_ptr``
     - Extract pointer from PARM chunk
   * - ``host_extract_data_binary``
     - Extract binary data
   * - ``host_extract_data_string``
     - Extract null-terminated string

Error Cases
"""""""""""

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - Test Name
     - Description
   * - ``host_missing_cnfg``
     - CALL without prior CNFG returns ERRO
   * - ``host_malformed_riff``
     - Bad RIFF structure returns ERRO
   * - ``host_unsupported_opcode``
     - Unknown opcode returns ERRO (UNSUPPORTED_OP)
   * - ``host_truncated_call``
     - CALL chunk cut short

4. Host Response Builder Tests (``test_host_response.c``)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - Test Name
     - Description
   * - ``host_build_retn_simple``
     - Build RETN with result + errno
   * - ``host_build_retn_with_data``
     - Build RETN with DATA chunk
   * - ``host_build_retn_with_parms``
     - Build RETN with PARM chunks
   * - ``host_build_retn_multiple_parms``
     - Build RETN with 4 PARM chunks (HEAPINFO)
   * - ``host_build_erro``
     - Build ERRO chunk
   * - ``host_write_int_le``
     - Write integer in little-endian
   * - ``host_write_int_be``
     - Write integer in big-endian
   * - ``host_write_ptr_4byte``
     - Write 4-byte pointer
   * - ``host_write_ptr_8byte``
     - Write 8-byte pointer

5. Round-Trip Integration Tests (``test_roundtrip.c``)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

These tests exercise client and host together, with mock I/O connecting them.

::

   +---------+                 +----------+
   | Client  | --- RIFF --->   |   Host   |
   |         | <-- RETN ----   |          |
   +---------+                 +----------+
        ^                           |
        |     mock_memory           |
        +---------------------------+

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Test Name
     - Description
   * - ``roundtrip_open``
     - SYS_OPEN: client builds, host parses, host responds
   * - ``roundtrip_close``
     - SYS_CLOSE round-trip
   * - ``roundtrip_read``
     - SYS_READ: verify data returned in RETN DATA chunk
   * - ``roundtrip_write``
     - SYS_WRITE: verify data received by handler
   * - ``roundtrip_writec``
     - SYS_WRITEC: single character
   * - ``roundtrip_write0``
     - SYS_WRITE0: null-terminated string
   * - ``roundtrip_readc``
     - SYS_READC: single character returned
   * - ``roundtrip_seek``
     - SYS_SEEK round-trip
   * - ``roundtrip_flen``
     - SYS_FLEN round-trip
   * - ``roundtrip_remove``
     - SYS_REMOVE round-trip
   * - ``roundtrip_rename``
     - SYS_RENAME: two filenames
   * - ``roundtrip_tmpnam``
     - SYS_TMPNAM: path returned in DATA
   * - ``roundtrip_clock``
     - SYS_CLOCK round-trip
   * - ``roundtrip_time``
     - SYS_TIME round-trip
   * - ``roundtrip_system``
     - SYS_SYSTEM round-trip
   * - ``roundtrip_errno``
     - SYS_ERRNO round-trip
   * - ``roundtrip_get_cmdline``
     - SYS_GET_CMDLINE: cmdline in DATA
   * - ``roundtrip_heapinfo``
     - SYS_HEAPINFO: 4 PARM chunks returned
   * - ``roundtrip_elapsed``
     - SYS_ELAPSED: 64-bit value
   * - ``roundtrip_tickfreq``
     - SYS_TICKFREQ round-trip
   * - ``roundtrip_iserror``
     - SYS_ISERROR round-trip
   * - ``roundtrip_istty``
     - SYS_ISTTY round-trip

6. Endianness Tests (``test_endianness.c``)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Testing endianness is challenging without actual cross-compilation, but we can:

1. Manually construct byte arrays for each endianness
2. Verify the host library interprets them correctly

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - Test Name
     - Description
   * - ``endian_read_int_le_4byte``
     - Parse 4-byte LE int: ``{0x78, 0x56, 0x34, 0x12}`` -> ``0x12345678``
   * - ``endian_read_int_be_4byte``
     - Parse 4-byte BE int: ``{0x12, 0x34, 0x56, 0x78}`` -> ``0x12345678``
   * - ``endian_read_int_le_2byte``
     - Parse 2-byte LE int
   * - ``endian_read_int_be_2byte``
     - Parse 2-byte BE int
   * - ``endian_read_int_le_8byte``
     - Parse 8-byte LE int
   * - ``endian_read_int_be_8byte``
     - Parse 8-byte BE int
   * - ``endian_write_int_le_4byte``
     - Write 4-byte LE int, verify bytes
   * - ``endian_write_int_be_4byte``
     - Write 4-byte BE int, verify bytes
   * - ``endian_read_ptr_le_4byte``
     - Parse 4-byte LE pointer
   * - ``endian_read_ptr_be_4byte``
     - Parse 4-byte BE pointer
   * - ``endian_read_ptr_le_8byte``
     - Parse 8-byte LE pointer
   * - ``endian_read_ptr_be_8byte``
     - Parse 8-byte BE pointer
   * - ``endian_roundtrip_le_client_le_host``
     - LE client <-> host configured for LE
   * - ``endian_roundtrip_be_client_be_host``
     - Manually constructed BE request, host configured for BE
   * - ``endian_cnfg_respected``
     - Host uses CNFG endianness, not compile-time default

Approach for Cross-Endian Testing
"""""""""""""""""""""""""""""""""

Since we can't easily run BE code on an LE machine, we:

1. **Manually construct test vectors**: Create byte arrays representing
   BE-encoded RIFF structures
2. **Configure host state manually**: Set ``state->endianness = ZBC_ENDIAN_BIG``
   explicitly
3. **Verify parsing**: Check that values are decoded correctly

Example:

.. code-block:: c

   void test_endian_read_int_be_4byte(void) {
       zbc_host_state_t state;
       uint8_t buf[] = {0x12, 0x34, 0x56, 0x78};  /* BE: 0x12345678 */

       state.int_size = 4;
       state.endianness = ZBC_ENDIAN_BIG;

       intmax_t val = zbc_host_read_int(&state, buf);
       TEST_ASSERT_EQ(val, 0x12345678);
   }

7. Pointer Size Tests (``test_ptr_sizes.c``)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - Test Name
     - Description
   * - ``ptr_size_2byte``
     - 16-bit pointers (rare but spec-compliant)
   * - ``ptr_size_4byte``
     - 32-bit pointers (common embedded)
   * - ``ptr_size_8byte``
     - 64-bit pointers
   * - ``ptr_size_heapinfo_4byte``
     - HEAPINFO with 4-byte pointers
   * - ``ptr_size_heapinfo_8byte``
     - HEAPINFO with 8-byte pointers
   * - ``ptr_size_mismatch_handled``
     - Host handles ptr_size != host pointer size

8. Integer Size Tests (``test_int_sizes.c``)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - Test Name
     - Description
   * - ``int_size_2byte``
     - 16-bit integers
   * - ``int_size_4byte``
     - 32-bit integers
   * - ``int_size_8byte``
     - 64-bit integers
   * - ``int_size_sign_extend_2byte``
     - Negative 16-bit value sign-extended
   * - ``int_size_sign_extend_4byte``
     - Negative 32-bit value sign-extended
   * - ``int_size_elapsed_small_int``
     - SYS_ELAPSED with int_size < 8 uses DATA chunk

9. CNFG Caching Tests (``test_cnfg.c``)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - Test Name
     - Description
   * - ``cnfg_sent_once``
     - CNFG chunk only in first request
   * - ``cnfg_reset_resends``
     - After ``zbc_client_reset_cnfg()``, CNFG sent again
   * - ``cnfg_host_requires``
     - Host returns ERRO if CNFG missing
   * - ``cnfg_host_caches``
     - Host remembers CNFG across requests
   * - ``cnfg_host_reset``
     - After ``zbc_host_reset_cnfg()``, requires new CNFG

10. Error Propagation Tests (``test_errors.c``)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - Test Name
     - Description
   * - ``error_buffer_too_small``
     - ``ZBC_ERR_BUFFER_TOO_SMALL`` returned correctly
   * - ``error_invalid_arg_null``
     - NULL arguments return ``ZBC_ERR_INVALID_ARG``
   * - ``error_device_error``
     - ERRO chunk results in ``ZBC_ERR_DEVICE_ERROR``
   * - ``error_parse_error``
     - Malformed response returns ``ZBC_ERR_PARSE_ERROR``
   * - ``error_errno_preserved``
     - errno from handler appears in RETN
   * - ``error_proto_error_codes``
     - All ``ZBC_PROTO_ERR_*`` codes tested

11. Edge Case Tests (``test_edge_cases.c``)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - Test Name
     - Description
   * - ``edge_empty_string``
     - Empty string ("") in DATA chunk
   * - ``edge_max_path_length``
     - 4KB filename string
   * - ``edge_max_int_value``
     - ``INT32_MAX``, ``INT64_MAX`` values
   * - ``edge_min_int_value``
     - ``INT32_MIN``, ``INT64_MIN`` values
   * - ``edge_zero_fd``
     - fd=0 (stdin) handled correctly
   * - ``edge_negative_fd``
     - fd=-1 (invalid) handled
   * - ``edge_odd_data_size``
     - Data sizes 1, 3, 5, 7... get padding
   * - ``edge_large_read``
     - Request 64KB read
   * - ``edge_large_write``
     - Write 64KB data
   * - ``edge_rename_same_file``
     - Rename to same name

12. Memory Safety Tests (``test_memory_safety.c``)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - Test Name
     - Description
   * - ``safety_canary_client_build``
     - Canaries intact after client build
   * - ``safety_canary_client_parse``
     - Canaries intact after client parse
   * - ``safety_canary_host_parse``
     - Canaries intact after host parse
   * - ``safety_canary_host_build``
     - Canaries intact after host build response
   * - ``safety_canary_roundtrip``
     - Canaries intact after full round-trip

**Note:** Zero-allocation is verified at build time via static check, not at
runtime. See `Static Allocation Check`_ below.

File Structure
--------------

::

   test/
   +-- TEST_PLAN.rst              # This document
   +-- CMakeLists.txt             # Test build configuration
   +-- test_harness.h             # Test macros, guarded buffers, utilities
   +-- test_harness.c             # Test harness implementation
   +-- mock_device.h              # Mock device for client tests
   +-- mock_device.c              # Mock device implementation
   +-- mock_memory.h              # Mock memory for host tests
   +-- mock_memory.c              # Mock memory implementation
   |
   +-- test_client_builder.c      # Builder unit tests
   +-- test_client_parser.c       # Parser unit tests
   +-- test_host_parser.c         # Host parser tests
   +-- test_host_response.c       # Host response builder tests
   +-- test_roundtrip.c           # Integration tests
   +-- test_endianness.c          # Endianness tests
   +-- test_ptr_sizes.c           # Pointer size tests
   +-- test_int_sizes.c           # Integer size tests
   +-- test_cnfg.c                # CNFG caching tests
   +-- test_errors.c              # Error propagation tests
   +-- test_edge_cases.c          # Edge case tests
   +-- test_memory_safety.c       # Memory safety tests (canary checks)
   |
   +-- test_main.c                # Main test runner

CMake Configuration
-------------------

.. code-block:: cmake

   # test/CMakeLists.txt

   # Test executable
   add_executable(zbc_tests
       test_harness.c
       mock_device.c
       mock_memory.c
       test_client_builder.c
       test_client_parser.c
       test_host_parser.c
       test_host_response.c
       test_roundtrip.c
       test_endianness.c
       test_ptr_sizes.c
       test_int_sizes.c
       test_cnfg.c
       test_errors.c
       test_edge_cases.c
       test_memory_safety.c
       test_main.c
   )

   # Link against the semihosting libraries
   target_link_libraries(zbc_tests
       zbc_semi_client
       zbc_semi_host
   )

   # Include paths
   target_include_directories(zbc_tests PRIVATE
       ${CMAKE_SOURCE_DIR}/include
       ${CMAKE_CURRENT_SOURCE_DIR}
   )

   # Register with CTest
   enable_testing()
   add_test(NAME zbc_semihost_tests COMMAND zbc_tests)

Static Allocation Check
-----------------------

The libraries must not call ``malloc()``, ``calloc()``, ``realloc()``, or
``free()``. This is verified at build time by scanning the source files.

CMake Check (Portable)
^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cmake

   # In top-level CMakeLists.txt or test/CMakeLists.txt

   # Find allocation calls in library source files
   file(GLOB_RECURSE LIB_SOURCES
       "${CMAKE_SOURCE_DIR}/src/client/*.c"
       "${CMAKE_SOURCE_DIR}/src/host/*.c"
   )

   foreach(src ${LIB_SOURCES})
       file(READ ${src} content)
       # Check for allocation function calls (simple pattern match)
       string(REGEX MATCH "(malloc|calloc|realloc|free)[ \t]*\\(" found "${content}")
       if(found)
           message(FATAL_ERROR
               "Allocation function found in ${src}: ${found}\n"
               "Libraries must not allocate memory.")
       endif()
   endforeach()

   message(STATUS
       "Static allocation check passed - no malloc/calloc/realloc/free in library sources")

Alternative: Shell Script (for CI)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: bash

   #!/bin/sh
   # check_no_alloc.sh - Verify no allocation calls in library code

   SRCS="src/client/*.c src/host/*.c"

   if grep -E '\b(malloc|calloc|realloc|free)\s*\(' $SRCS; then
       echo "ERROR: Allocation functions found in library sources"
       exit 1
   fi

   echo "OK: No allocation functions in library sources"
   exit 0

This approach works on Windows, macOS, and Linux without platform-specific
tricks.

Test Execution
--------------

Running All Tests
^^^^^^^^^^^^^^^^^

.. code-block:: bash

   cd build
   cmake ..
   make
   ctest --output-on-failure

Running Individual Test Files
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The test main will accept optional arguments:

.. code-block:: bash

   ./zbc_tests                    # Run all tests
   ./zbc_tests builder            # Run only builder tests
   ./zbc_tests roundtrip          # Run only roundtrip tests
   ./zbc_tests --list             # List all test suites

Expected Output
^^^^^^^^^^^^^^^

::

   ZBC Semihosting Library Tests
   =============================

   Suite: Client Builder
     builder_start_empty                              [PASS]
     builder_start_with_cnfg                          [PASS]
     builder_cnfg_not_duplicated                      [PASS]
     ...

   Suite: Client Parser
     parser_retn_simple                               [PASS]
     parser_retn_with_data                            [PASS]
     ...

   Suite: Round-Trip
     roundtrip_open                                   [PASS]
     roundtrip_close                                  [PASS]
     roundtrip_read                                   [PASS]
     ...

   Suite: Memory Safety
     safety_no_malloc_client_init                     [PASS]
     safety_canary_client_build                       [PASS]
     ...

   =============================
   Results: 127 passed, 0 failed

Success Criteria
----------------

All tests must pass before:

1. Claiming the libraries are production-ready
2. Proceeding with picolibc integration
3. Tagging a release

A failing test indicates either:

- A bug in the library code
- A bug in the test (less likely if tests are simple)
- An incomplete implementation

Future Considerations
---------------------

Fuzz Testing
^^^^^^^^^^^^

Once basic tests pass, consider fuzzing with:

- Random RIFF structures to host parser
- Random responses to client parser
- AFL or libFuzzer integration

Coverage Analysis
^^^^^^^^^^^^^^^^^

- Use ``gcov``/``lcov`` to measure code coverage
- Target: >90% line coverage, >80% branch coverage

Performance Testing
^^^^^^^^^^^^^^^^^^^

- Not a priority for correctness, but could add:

  - Throughput tests (requests/second)
  - Memory usage profiling
