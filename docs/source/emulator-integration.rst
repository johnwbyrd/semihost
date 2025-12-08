Emulator Integration
====================

This guide shows how to add semihosting support to your emulator. Once
integrated, guest programs can access the host filesystem, console, and
time services through a memory-mapped device.

Files to Include
----------------

.. code-block:: c

   #include "zbc_host.h"          /* Host processing API */
   #include "zbc_backend.h"       /* Backend interface */
   #include "zbc_backend_ansi.h"  /* ANSI backend state (if using) */

Device Registers
----------------

The semihosting device is a 32-byte memory-mapped peripheral. Map it at a
convenient address in your emulator's memory space:

.. list-table::
   :header-rows: 1
   :widths: 10 10 15 10 55

   * - Offset
     - Size
     - Name
     - Access
     - Description
   * - 0x00
     - 8
     - SIGNATURE
     - R
     - ASCII "SEMIHOST"
   * - 0x08
     - 16
     - RIFF_PTR
     - RW
     - Pointer to RIFF buffer in guest RAM
   * - 0x18
     - 1
     - DOORBELL
     - W
     - Write triggers request processing
   * - 0x19
     - 1
     - IRQ_STATUS
     - R
     - Bit 0: response ready
   * - 0x1A
     - 1
     - IRQ_ENABLE
     - RW
     - Bit 0: enable IRQ on response
   * - 0x1B
     - 1
     - IRQ_ACK
     - W
     - Write 1 to clear IRQ
   * - 0x1C
     - 1
     - STATUS
     - R
     - Bit 0: response ready, Bit 7: device present

Request Flow
------------

1. Guest writes RIFF buffer address to RIFF_PTR
2. Guest writes any value to DOORBELL
3. Your emulator calls ``zbc_host_process()`` to handle the request
4. Set STATUS bit 0 (and optionally assert IRQ)
5. Guest reads response from RIFF buffer

Memory Operations
-----------------

You provide callbacks so the host library can read and write guest memory:

.. code-block:: c

   typedef struct {
       uint8_t (*read_u8)(uint64_t addr, void *ctx);
       void (*write_u8)(uint64_t addr, uint8_t val, void *ctx);
       void (*read_block)(void *dest, uint64_t addr, size_t size, void *ctx);
       void (*write_block)(uint64_t addr, const void *src, size_t size, void *ctx);
   } zbc_host_mem_ops_t;

Implement these to access your emulator's guest memory:

.. code-block:: c

   static uint8_t my_read_u8(uint64_t addr, void *ctx) {
       return guest_memory[addr];
   }

   static void my_write_u8(uint64_t addr, uint8_t val, void *ctx) {
       guest_memory[addr] = val;
   }

   static void my_read_block(void *dest, uint64_t addr, size_t size, void *ctx) {
       memcpy(dest, &guest_memory[addr], size);
   }

   static void my_write_block(uint64_t addr, const void *src, size_t size, void *ctx) {
       memcpy(&guest_memory[addr], src, size);
   }

Initialization
--------------

Set up the host state with your memory operations and a backend:

.. code-block:: c

   static zbc_host_state_t host;
   static uint8_t work_buffer[1024];

   /* Using the insecure backend (unrestricted host access) */
   static zbc_ansi_insecure_state_t backend_state;

   void semihost_init(void) {
       zbc_host_mem_ops_t mem_ops = {
           .read_u8 = my_read_u8,
           .write_u8 = my_write_u8,
           .read_block = my_read_block,
           .write_block = my_write_block
       };

       zbc_ansi_insecure_init(&backend_state);

       zbc_host_init(&host, &mem_ops, NULL,
                     zbc_backend_ansi_insecure(), &backend_state,
                     work_buffer, sizeof(work_buffer));
   }

Processing Requests
-------------------

When the guest writes to DOORBELL, call ``zbc_host_process()``:

.. code-block:: c

   void on_doorbell_write(uint64_t riff_ptr) {
       zbc_host_process(&host, riff_ptr);
       /* Set STATUS bit 0 in your device register emulation */
   }

The host library reads the RIFF request from guest memory, dispatches to
the backend, and writes the response back to guest memory.

Built-in Backends
-----------------

Insecure ANSI Backend
^^^^^^^^^^^^^^^^^^^^^

Provides unrestricted access to the host filesystem using standard C I/O.
Guest code can read, write, and delete any file the host process can access.

.. code-block:: c

   #include "zbc_backend_ansi.h"

   static zbc_ansi_insecure_state_t backend_state;

   zbc_ansi_insecure_init(&backend_state);
   /* Use zbc_backend_ansi_insecure() and &backend_state */

Secure ANSI Backend
^^^^^^^^^^^^^^^^^^^

Sandboxes file operations to a specific directory. Guest code cannot escape
the sandbox or access files outside it.

.. code-block:: c

   #include "zbc_backend_ansi.h"

   static zbc_ansi_state_t backend_state;

   zbc_ansi_init(&backend_state, "/path/to/sandbox/");
   /* Use zbc_backend_ansi() and &backend_state */

Additional configuration for the secure backend:

.. code-block:: c

   /* Allow access to additional paths */
   zbc_ansi_add_path(&backend_state, "/usr/share/data/", 0);  /* read-only */
   zbc_ansi_add_path(&backend_state, "/tmp/scratch/", 1);     /* read-write */

   /* Set flags */
   backend_state.flags |= ZBC_ANSI_FLAG_ALLOW_SYSTEM;  /* enable system() */
   backend_state.flags |= ZBC_ANSI_FLAG_READ_ONLY;     /* block all writes */

   /* Set violation callback */
   zbc_ansi_set_callbacks(&backend_state, my_violation_handler, my_exit_handler, ctx);

Dummy Backend
^^^^^^^^^^^^^

All operations succeed with no side effects. Useful for testing.

.. code-block:: c

   zbc_host_init(&host, &mem_ops, NULL,
                 zbc_backend_dummy(), NULL,
                 work_buffer, sizeof(work_buffer));

Custom Backends
---------------

Implement the ``zbc_backend_t`` vtable (`include/zbc_backend.h`). Only implement needed ops; NULL = error to guest.

**Return Conventions**:

.. list-table::
   :header-rows: 1
   :widths: 25 30 45

   * - Operation
     - Success
     - Error
   * - open
     - fd ≥0
     - -1
   * - close/seek/remove/rename
     - 0
     - -1
   * - read/write
     - bytes NOT transferred (0=complete)
     - -1
   * - flen
     - length
     - -1
   * - clock/time
     - time value
     - -1

Example:

.. code-block:: c

   static const zbc_backend_t my_backend = {
       .open = my_open_impl,
       .close = my_close_impl,
       /* etc. */
   };

Cleanup
-------

.. code-block:: c

   zbc_ansi_cleanup(&backend_state);  /* secure */
   zbc_ansi_insecure_cleanup(&backend_state);  /* insecure */

See Also
--------

- :doc:`specification` -- RIFF details
- :doc:`client-library` -- guest API
- :doc:`security` -- backends/security
- ``include/zbc_host.h``
- ``include/zbc_backend.h``
- ``include/zbc_backend_ansi.h``
