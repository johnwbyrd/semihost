Examples
========

On-Target Test Suite
--------------------

The ``test/target/`` directory contains a comprehensive example of ZBC
semihosting on bare metal. The test suite exercises all 23 syscalls
and runs on multiple architectures.

Key Files
^^^^^^^^^

- **zbc_target_test.c**: Complete test program using all semihosting calls
- **zbc_target_harness.h**: Bare-metal test macros (no libc needed)
- **platforms/m6502/**: 6502-specific files

  - ``crt6502.c``: C runtime startup
  - ``zbc6502.ld``: Linker script

- **platforms/i386/**: x86 32-bit files

  - ``crti386.S``: Assembly startup
  - ``zbci386.ld``: Linker script

Building and Running
^^^^^^^^^^^^^^^^^^^^

.. code-block:: bash

   cmake -B build -DZBC_TARGET_TESTS=ON
   cmake --build build
   ctest --test-dir build -L target

This builds test binaries for available platforms and runs them on MAME's
``zbci386`` and ``zbcm6502`` machines.

Minimal Client Example
----------------------

Using the high-level API for a POSIX-like interface:

.. code-block:: c

   #include "zbc_api.h"

   static zbc_client_state_t client;
   static zbc_api_t api;
   static uint8_t buf[256];

   void _start(void) {
       /* Platform-specific device address */
       volatile uint8_t *dev = (volatile uint8_t *)0xFFFFBFDF;

       zbc_client_init(&client, dev);
       zbc_api_init(&api, &client, buf, sizeof(buf));

       /* Print via semihosting */
       zbc_api_write0(&api, "Hello, ZBC!\n");

       /* File I/O */
       int fd = zbc_api_open(&api, "/tmp/test.txt", SH_OPEN_W);
       if (fd >= 0) {
           zbc_api_write(&api, fd, "Test output\n", 12);
           zbc_api_close(&api, fd);
       }

       /* Exit */
       zbc_api_exit(&api, 0);
   }

Using the low-level API (for libc integration or direct RIFF control):

.. code-block:: c

   #include "zbc_semihost.h"

   static zbc_client_state_t client;
   static uint8_t riff_buf[256];

   void _start(void) {
       /* Platform-specific device address */
       volatile uint8_t *dev = (volatile uint8_t *)0xFFFFBFDF;

       zbc_client_init(&client, dev);

       /* Print via semihosting */
       uintptr_t args[1];
       args[0] = (uintptr_t)"Hello, ZBC!\n";
       zbc_semihost(&client, riff_buf, sizeof(riff_buf),
                    SH_SYS_WRITE0, (uintptr_t)args);

       /* Exit */
       args[0] = 0;
       zbc_semihost(&client, riff_buf, sizeof(riff_buf),
                    SH_SYS_EXIT, (uintptr_t)args);
   }

The device address depends on the platform. For 32-bit systems, ZBC
places the semihost device at ``0xFFFFBFDF`` (end of address space
minus reserved area).

Customization
-------------

- **Device Address**: See ``zbc_target_harness.h`` for the address calculation
  formula based on pointer size.
- **Backend**: Use ``zbc_backend_ansi()`` for sandboxed file access or
  ``zbc_backend_ansi_insecure()`` for full host filesystem access.
- **CPU**: Works 8/16/32/64-bit; the CNFG chunk conveys int_size/ptr_size
  automatically.

See Also
--------

- :doc:`client-library` for full client API documentation
- :doc:`testing` for test infrastructure details
- ``test/target/`` source for complete platform examples
