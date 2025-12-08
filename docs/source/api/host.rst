Host API
========

Header: ``zbc_host.h``

The host API provides functions for processing semihosting requests
from guest code. Use this when implementing a semihosting device in
an emulator or virtual machine.

Types
-----

.. autoctype:: zbc_host.h::zbc_host_mem_ops_t

.. autoctype:: zbc_host.h::zbc_host_state_t

Functions
---------

.. autocfunction:: zbc_host.h::zbc_host_init

**Example:**

.. code-block:: c

   static zbc_host_state_t host;
   static uint8_t work_buf[1024];
   static zbc_ansi_insecure_state_t backend_state;

   zbc_host_mem_ops_t mem_ops = {
       .read_u8 = my_read_u8,
       .write_u8 = my_write_u8,
       .read_block = my_read_block,
       .write_block = my_write_block
   };

   zbc_ansi_insecure_init(&backend_state);

   zbc_host_init(&host, &mem_ops, NULL,
                 zbc_backend_ansi_insecure(), &backend_state,
                 work_buf, sizeof(work_buf));

.. autocfunction:: zbc_host.h::zbc_host_process

**Example:**

.. code-block:: c

   void on_doorbell_write(uint64_t riff_ptr) {
       int rc = zbc_host_process(&host, riff_ptr);
       if (rc == ZBC_OK) {
           set_status_response_ready();
       }
   }

Helper Functions
----------------

.. autocfunction:: zbc_host.h::zbc_host_read_guest_int

.. autocfunction:: zbc_host.h::zbc_host_write_guest_int

Work Buffer Sizing
------------------

The work buffer should be large enough to hold the largest RIFF request
plus space for the response. Recommended sizes:

- **Minimum:** 256 bytes (most syscalls)
- **Typical:** 1024 bytes (comfortable for file operations)
- **Large reads:** Match the largest expected SYS_READ count plus overhead

For SYS_READ operations, the buffer is split: half for reading the request,
half for the read data. So a 1024-byte buffer supports reads up to ~500 bytes.
